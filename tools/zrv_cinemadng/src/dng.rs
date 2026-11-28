//! This product includes DNG technology under license by Adobe.

use crate::zrv::{ClipInfo, ClipSequence, FrameFormat};
use std::ffi::OsStr;
use std::fs;
use std::io::{self, Write};
use std::ops::Range;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex, mpsc};
use std::thread;

pub const DEFAULT_BLACK_LEVEL: u16 = 256;
pub const WHITE_LEVEL: u32 = 4095;
const TIME_CODES: u16 = 51043;
const FRAME_RATE: u16 = 51044;
const REEL_NAME: u16 = 51081;

#[derive(Clone, Copy)]
pub enum MissingFrame {
    Previous,
    Black,
}

fn sequence_name(source: &Path) -> &OsStr {
    let numbered = source.extension().and_then(OsStr::to_str).is_some_and(|ext| {
        let b = ext.as_bytes();
        b.len() >= 3 && b[0].eq_ignore_ascii_case(&b'Z') && b[1..].iter().all(u8::is_ascii_digit)
    });
    if numbered { source.file_name() } else { source.file_stem() }.unwrap_or_default()
}

pub fn destination(source: &Path, parent: &Path, synced: bool) -> PathBuf {
    let mut name = sequence_name(source).to_os_string();
    name.push(if synced { "_CinemaDNG_MOV" } else { "_CinemaDNG" });
    parent.join(name)
}

pub fn frame_report_path(source: &Path, parent: &Path, synced: bool, number: u32) -> PathBuf {
    let mut name = sequence_name(source).to_os_string();
    name.push(if synced { "_frame_report" } else { "_dropped_frames" });
    if number > 1 { name.push(format!("_{number}")); }
    name.push(".csv");
    parent.join(name)
}

pub fn movie_timeline(clip: &ClipSequence, source: &Path, movie: &Path) -> io::Result<(Vec<u32>, u32)> {
    if !source.file_stem().zip(movie.file_stem()).is_some_and(|(a, b)| a.eq_ignore_ascii_case(b)) {
        return Err(io::Error::new(io::ErrorKind::InvalidInput, "请选择与 ZRV 首段同名的同期 MOV 或 MP4"));
    }
    let total = crate::mov::frames(movie, clip.info)?;
    let positions = clip.movie_positions(Some(total))?;
    if !positions.iter().any(|&frame| (1..=total).contains(&frame)) {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "MOV 与机内帧对应表没有重合区间"));
    }
    Ok((positions, total))
}

pub fn export(
    source: &Path,
    parent: &Path,
    black_level: u16,
    missing_frame: MissingFrame,
    movie: Option<&Path>,
    cancelled: &AtomicBool,
    progress: impl FnMut(u32, u32),
) -> io::Result<PathBuf> {
    if u32::from(black_level) >= WHITE_LEVEL {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "黑电平必须为 0–4094",
        ));
    }
    let mut clip = ClipSequence::open(source)?;
    let output = destination(source, parent, movie.is_some());
    if output.try_exists()? {
        return Err(io::Error::new(
            io::ErrorKind::AlreadyExists,
            "同名序列目录已存在，请选择另一个输出位置",
        ));
    }
    let mapping = movie.map(|movie| movie_timeline(&clip, source, movie)).transpose()?;
    let stem = sequence_name(source).to_string_lossy();
    let reel: String = stem
        .chars()
        .map(|c| {
            if c.is_ascii_graphic() || c == ' ' {
                c
            } else {
                '_'
            }
        })
        .collect();
    let staging = tempfile::Builder::new()
        .prefix(".zrv-cinemadng-")
        .tempdir_in(parent)?;
    let frames = FrameOutput { path: staging.path(), stem: &stem, reel: &reel, black_level };
    export_frames(&mut clip, &frames, missing_frame, mapping.as_ref(), cancelled, progress)?;
    check_cancelled(cancelled)?;
    let wave = staging.path().join(format!("{stem}.wav"));
    if let Some((positions, total)) = mapping {
        clip.export_synced_audio(&wave, &positions, total)?;
        let report = frame_report_path(source, staging.path(), true, 1);
        clip.export_movie_frame_report(&report, &positions, total)?;
    } else {
        clip.export_audio(&wave)?;
        if clip.info.dropped_frames > 0 {
            let report = frame_report_path(source, staging.path(), false, 1);
            clip.export_drop_report(&report)?;
        }
    }
    check_cancelled(cancelled)?;
    // Publish the finished directory in one rename on the destination volume.
    // On Windows an existing destination directory makes this rename fail.
    fs::rename(staging.path(), &output)?;
    let _ = staging.keep();
    Ok(output)
}

struct FrameOutput<'a> {
    path: &'a Path,
    stem: &'a str,
    reel: &'a str,
    black_level: u16,
}

struct FrameJob {
    format: FrameFormat,
    data: Vec<u8>,
    first: Range<u32>,
    repeats: Vec<Range<u32>>,
}

impl FrameJob {
    fn new(format: FrameFormat, data: Vec<u8>, index: u32) -> Self {
        Self { format, data, first: index..index + 1, repeats: Vec::new() }
    }

    fn add_frame(&mut self, index: u32) {
        let last = self.repeats.last_mut().unwrap_or(&mut self.first);
        if last.end == index {
            last.end += 1;
        } else {
            self.repeats.push(index..index + 1);
        }
    }
}

fn export_frames(
    clip: &mut ClipSequence,
    output: &FrameOutput<'_>,
    missing_frame: MissingFrame,
    mapping: Option<&(Vec<u32>, u32)>,
    cancelled: &AtomicBool,
    mut progress: impl FnMut(u32, u32),
) -> io::Result<()> {
    let info = clip.info;
    let total = mapping.map_or(info.frames, |(_, frames)| *frames);
    let workers = thread::available_parallelism().map_or(1, usize::from)
        .min(4)
        .min(info.frames as usize);
    let stop = AtomicBool::new(false);
    progress(0, total);
    thread::scope(|scope| {
        // Read each source file sequentially. A rendezvous keeps full frame
        // buffers limited to the workers and the reader's lookahead.
        let (jobs, pending_jobs) = mpsc::sync_channel::<FrameJob>(0);
        let pending_jobs = Arc::new(Mutex::new(pending_jobs));
        let (finished, completions) = mpsc::channel::<io::Result<(u32, Vec<u8>)>>();
        for _ in 0..workers {
            let pending_jobs = Arc::clone(&pending_jobs);
            let finished = finished.clone();
            let stop = &stop;
            thread::Builder::new().spawn_scoped(scope, move || {
                let mut header = DngHeader::new(info, output.black_level, output.reel);
                let mut scratch = Vec::new();
                let mut pixels = vec![0; (info.width * info.height * 2) as usize];
                while !stop.load(Ordering::Relaxed) && !cancelled.load(Ordering::Relaxed) {
                    let Ok(job) = pending_jobs.lock().unwrap().recv() else { break };
                    if stop.load(Ordering::Relaxed) { break; }
                    let result = (|| {
                        check_cancelled(cancelled)?;
                        if let Some(raw) = job.format.decode(&job.data, &mut scratch, info)? {
                            unpack_raw12(raw, &mut pixels, info);
                        } else {
                            let sample = output.black_level.to_le_bytes();
                            for pixel in pixels.chunks_exact_mut(2) {
                                pixel.copy_from_slice(&sample);
                            }
                        }
                        let mut written = 0;
                        for index in std::iter::once(job.first).chain(job.repeats).flatten() {
                            check_cancelled(cancelled)?;
                            if stop.load(Ordering::Relaxed) { return Ok((0, job.data)); }
                            header.set_timecode(index);
                            let path = output.path.join(format!("{}_{:06}.dng", output.stem, index + 1));
                            let mut out = fs::File::create_new(path)?;
                            out.write_all(&header.bytes)?;
                            out.write_all(&pixels)?;
                            written += 1;
                        }
                        Ok((written, job.data))
                    })();
                    if result.is_err() {
                        stop.store(true, Ordering::Relaxed);
                    }
                    if finished.send(result).is_err() { break; }
                }
            })?;
        }
        drop(pending_jobs);
        drop(finished);

        let mut done = 0;
        let mut report = |count| {
            if count != 0 {
                done += count;
                progress(done, total);
            }
        };
        let mut recycled = Vec::new();
        let mut worker_error = None;
        let submit = |job| jobs.send(job).map_err(|_| {
            io::Error::other("导出线程提前结束")
        });
        let reader_result: io::Result<()> = (|| {
            let mut lookahead = Vec::new();
            let mut previous: Option<FrameJob> = None;
            let mut black: Option<FrameJob> = None;
            let mut raw = 0;
            for index in 0..total {
                check_cancelled(cancelled)?;
                for result in completions.try_iter() {
                    match result {
                        Ok((count, buffer)) => {
                            report(count);
                            if buffer.capacity() != 0 { recycled.push(buffer); }
                        }
                        Err(error) => { worker_error = Some(error); return Ok(()); }
                    }
                }
                if stop.load(Ordering::Relaxed) { break; }
                let from_raw = if let Some((positions, _)) = mapping {
                    while positions.get(raw) == Some(&0) {
                        clip.read_frame_data(&mut lookahead)?;
                        raw += 1;
                    }
                    positions.get(raw) == Some(&(index + 1))
                } else { true };
                let format = if from_raw {
                    raw += 1;
                    clip.read_frame_data(&mut lookahead)?
                } else { FrameFormat::Missing };
                if format == FrameFormat::Missing {
                    // Keep the last real RAW frame available across MOV padding gaps.
                    if from_raw && matches!(missing_frame, MissingFrame::Previous)
                        && let Some(job) = &mut previous
                    {
                        job.add_frame(index);
                    } else if let Some(job) = &mut black {
                        job.add_frame(index);
                    } else {
                        black = Some(FrameJob::new(FrameFormat::Missing, Vec::new(), index));
                    }
                    continue;
                }
                if let Some(job) = previous.take() { submit(job)?; }
                if let Some(job) = black.take() { submit(job)?; }
                let data = std::mem::replace(&mut lookahead, recycled.pop().unwrap_or_default());
                previous = Some(FrameJob::new(format, data, index));
            }
            if !stop.load(Ordering::Relaxed) {
                for job in [previous, black].into_iter().flatten() {
                    check_cancelled(cancelled)?;
                    submit(job)?;
                }
            }
            Ok(())
        })();
        if reader_result.is_err() { stop.store(true, Ordering::Relaxed); }
        drop(jobs);
        for result in completions {
            match result {
                Ok((count, _)) => report(count),
                Err(error) => { worker_error.get_or_insert(error); }
            }
        }
        if let Some(error) = worker_error { return Err(error); }
        check_cancelled(cancelled)?;
        reader_result?;
        if done != total { return Err(io::Error::other("导出帧数不完整")); }
        Ok(())
    })
}

fn check_cancelled(cancelled: &AtomicBool) -> io::Result<()> {
    if cancelled.load(Ordering::Relaxed) {
        return Err(io::Error::new(io::ErrorKind::Interrupted, "转换已取消"));
    }
    Ok(())
}

fn unpack_raw12(packed: &[u8], pixels: &mut [u8], info: ClipInfo) {
    let row = info.width as usize;
    for (source, target) in packed
        .chunks_exact(info.stride as usize)
        .zip(pixels.chunks_exact_mut(row * 2))
    {
        for (pair, out) in source[..row * 3 / 2]
            .chunks_exact(3)
            .zip(target.chunks_exact_mut(4))
        {
            out.copy_from_slice(&[
                pair[0],
                pair[1] & 15,
                (pair[1] >> 4) | (pair[2] << 4),
                pair[2] >> 4,
            ]);
        }
    }
}

struct Tag {
    id: u16,
    kind: u16,
    count: u32,
    bytes: Vec<u8>,
}

impl Tag {
    fn raw(id: u16, kind: u16, count: u32, bytes: Vec<u8>) -> Self {
        Self {
            id,
            kind,
            count,
            bytes,
        }
    }

    fn bytes(id: u16, value: &[u8]) -> Self {
        Self::raw(id, 1, value.len() as u32, value.to_vec())
    }

    fn ascii(id: u16, value: &str) -> Self {
        Self::raw(
            id,
            2,
            value.len() as u32 + 1,
            [value.as_bytes(), &[0]].concat(),
        )
    }

    fn shorts(id: u16, values: &[u16]) -> Self {
        Self::raw(
            id,
            3,
            values.len() as u32,
            values.iter().flat_map(|v| v.to_le_bytes()).collect(),
        )
    }

    fn longs(id: u16, values: &[u32]) -> Self {
        Self::raw(
            id,
            4,
            values.len() as u32,
            values.iter().flat_map(|v| v.to_le_bytes()).collect(),
        )
    }

    fn signed_rational(id: u16, values: &[(i32, i32)]) -> Self {
        let bytes = values
            .iter()
            .flat_map(|&(n, d)| [n.to_le_bytes(), d.to_le_bytes()].concat());
        Self::raw(id, 10, values.len() as u32, bytes.collect())
    }
}

struct DngHeader {
    bytes: Vec<u8>,
    timecode_offset: Option<usize>,
    timecode_fps: u32,
}

impl DngHeader {
    fn new(info: ClipInfo, black_level: u16, reel: &str) -> Self {
        let (w, h) = (info.width, info.height);
        // Match the existing Python exporter's Nikon Z 7II still-image matrix
        // and daylight defaults. ZRV does not carry an as-shot color profile.
        let matrix =
            [13705, -6004, -1400, -5464, 13568, 2062, -940, 1706, 7618].map(|value| (value, 10000));
        let daylight = [3127u32, 10000, 3290, 10000]
            .into_iter()
            .flat_map(u32::to_le_bytes)
            .collect();
        let mut tags = vec![
            Tag::longs(254, &[0]),
            Tag::longs(256, &[w]),
            Tag::longs(257, &[h]),
            Tag::shorts(258, &[16]),
            Tag::shorts(259, &[1]),
            Tag::shorts(262, &[32803]), // CFA
            Tag::ascii(271, "NIKON CORPORATION"),
            Tag::ascii(272, "NIKON Z 7_2"),
            Tag::longs(273, &[0]), // StripOffsets, patched below.
            Tag::shorts(274, &[info.orientation]),
            Tag::shorts(277, &[1]),
            Tag::longs(278, &[h]),
            Tag::longs(279, &[w * h * 2]),
            Tag::shorts(284, &[1]),
            Tag::ascii(305, "ZRV CinemaDNG"),
            Tag::shorts(33421, &[2, 2]),
            Tag::bytes(33422, &[0, 1, 1, 2]),
            Tag::bytes(50706, &[1, 4, 0, 0]),
            Tag::bytes(50707, &[1, 1, 0, 0]),
            Tag::ascii(50708, "Nikon Z 7II video RAW"),
            Tag::bytes(50710, &[0, 1, 2]),
            Tag::shorts(50711, &[1]),
            Tag::shorts(50714, &[black_level]),
            Tag::longs(50717, &[WHITE_LEVEL]),
            Tag::longs(50719, &[0, 0]),
            Tag::longs(50720, &[w, h]),
            Tag::signed_rational(50721, &matrix),
            Tag::raw(50729, 5, 2, daylight),
            Tag::shorts(50778, &[21]),
            Tag::longs(50829, &[0, 0, h, w]),
        ];
        if matches!(info.fps_den, 1 | 1001) {
            tags.push(Tag::bytes(TIME_CODES, &[0; 8]));
        }
        tags.push(Tag::signed_rational(
            FRAME_RATE,
            &[(info.fps_num as i32, info.fps_den as i32)],
        ));
        tags.push(Tag::ascii(REEL_NAME, reel));
        let mut bytes = b"II\x2a\0\x08\0\0\0".to_vec();
        bytes.extend_from_slice(&(tags.len() as u16).to_le_bytes());
        let extra_start = bytes.len() + tags.len() * 12 + 4;
        let mut extra = Vec::new();
        let (mut strip, mut timecode_offset) = (0, None);
        for tag in tags {
            bytes.extend_from_slice(&tag.id.to_le_bytes());
            bytes.extend_from_slice(&tag.kind.to_le_bytes());
            bytes.extend_from_slice(&tag.count.to_le_bytes());
            if tag.id == 273 {
                strip = bytes.len();
            }
            if tag.bytes.len() <= 4 {
                bytes.extend_from_slice(&tag.bytes);
                bytes.extend_from_slice(&[0; 4][..4 - tag.bytes.len()]);
            } else {
                let offset = extra_start + extra.len();
                if tag.id == TIME_CODES {
                    timecode_offset = Some(offset);
                }
                bytes.extend_from_slice(&(offset as u32).to_le_bytes());
                extra.extend_from_slice(&tag.bytes);
                if extra.len() % 2 != 0 {
                    extra.push(0);
                }
            }
        }
        bytes.extend_from_slice(&[0; 4]); // No additional IFD.
        bytes.extend_from_slice(&extra);
        let pixel_offset = bytes.len() as u32;
        bytes[strip..strip + 4].copy_from_slice(&pixel_offset.to_le_bytes());
        Self {
            bytes,
            timecode_offset,
            timecode_fps: info.fps_num.div_ceil(info.fps_den),
        }
    }

    fn set_timecode(&mut self, frame: u32) {
        let Some(offset) = self.timecode_offset else {
            return;
        };
        let seconds = frame / self.timecode_fps;
        let frame_in_second = frame % self.timecode_fps;
        let field_mark_byte = match self.timecode_fps {
            50 => Some(3),
            60 => Some(1),
            _ => None,
        };
        let frame_address = if field_mark_byte.is_some() { frame_in_second / 2 } else { frame_in_second };
        // ST 331: four packed BCD time-and-flags bytes, then four user bytes.
        let mut clock = [
            frame_address,
            seconds % 60,
            seconds / 60 % 60,
            seconds / 3600 % 24,
        ]
        .map(|digits| (((digits / 10) << 4) | (digits % 10)) as u8);
        if let Some(byte) = field_mark_byte {
            // ST 12-1 numbers frame pairs and marks the second frame of each pair.
            clock[byte] |= ((frame_in_second % 2) as u8) << 7;
        }
        self.bytes[offset..offset + 4].copy_from_slice(&clock);
    }
}

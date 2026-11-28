use super::{Clip, ClipInfo, FrameFormat, invalid, stream};
use std::fs::{self, File};
use std::io::{self, BufReader, Read, Seek, SeekFrom, Write};
use std::ops::Range;
use std::path::{Path, PathBuf};

pub struct ClipSequence {
    pub info: ClipInfo,
    parts: Vec<(String, Clip)>,
    part: usize,
    position: u32,
}

#[derive(Clone, Copy, Default)]
pub struct MovieFrameCounts {
    pub timeout: u32,
    pub sync: u32,
    pub unknown: u32,
}

#[derive(Clone, Copy)]
enum MovieFrameKind {
    Timeout,
    Sync,
    Unknown,
}

fn extension(path: &Path) -> &str {
    path.extension().and_then(|ext| ext.to_str()).unwrap_or_default()
}

fn folder(path: &Path) -> &Path {
    path.parent().filter(|p| !p.as_os_str().is_empty()).unwrap_or(Path::new("."))
}

fn continuation_number(ext: &str) -> Option<u32> {
    let digits = ext.strip_prefix(['z', 'Z'])?;
    if digits.len() < 2 || !digits.bytes().all(|byte| byte.is_ascii_digit()) {
        return None;
    }
    let number: u32 = digits.parse().ok()?;
    (ext.eq_ignore_ascii_case(&format!("Z{number:02}"))).then_some(number)
}

fn continuations(head: &Path) -> io::Result<Vec<(u32, PathBuf)>> {
    let Some(stem) = head.file_stem() else { return Ok(Vec::new()) };
    let mut parts = Vec::new();
    for entry in fs::read_dir(folder(head))? {
        let path = entry?.path();
        if path.file_stem().is_some_and(|part| part.eq_ignore_ascii_case(stem))
            && let Some(number) = continuation_number(extension(&path))
        {
            parts.push((number, path));
        }
    }
    parts.sort_unstable_by_key(|part| part.0);
    Ok(parts)
}

impl ClipSequence {
    pub fn open(path: &Path) -> io::Result<Self> {
        let head = Clip::open(path)?;
        let mut info = head.info;
        let has_stream = head.stream.is_some();
        let ext = extension(path);
        let label = if ext.eq_ignore_ascii_case("zrv") || continuation_number(ext).is_some() {
            ext.to_ascii_uppercase()
        } else {
            String::new()
        };
        let mut parts = vec![(label, head)];
        let mut missing_footer = info.end.is_none();
        if ext.eq_ignore_ascii_case("zrv") && has_stream {
            for (expected, (number, path)) in continuations(path)?.into_iter().enumerate() {
                if number as usize != expected {
                    return Err(invalid(&format!("续段编号不连续，预期 .Z{expected:02}，发现 .Z{number:02}")));
                }
                let part = Clip::open(&path).map_err(|error| {
                    io::Error::new(error.kind(), format!("{} · {error}", path.display()))
                })?;
                let (_, previous) = parts.last().unwrap();
                let previous_index = previous.stream.as_ref().unwrap();
                let part_index = part.stream.as_ref().ok_or_else(|| invalid("续段缺少 ZRV 流式帧表"))?;
                let (_, last, phase) = previous_index.bounds();
                let (first, _, next_phase) = part_index.bounds();
                let part_info = part.info;
                let same_format = (info.width, info.height, info.stride, info.fps_num, info.fps_den, info.black_level, info.orientation)
                    == (part_info.width, part_info.height, part_info.stride, part_info.fps_num, part_info.fps_den, part_info.black_level, part_info.orientation);
                let period = stream::pcm_phase_period(info.fps_num, info.fps_den);
                let expected_phase = (phase + previous.info.frames % period) % period;
                if !same_format || last.checked_add(1) != Some(first) || next_phase != expected_phase {
                    return Err(invalid(&format!(".Z{number:02} 的画面规格、源帧号或音频相位与前一段无法接续")));
                }
                if previous_index.movie_session != part_index.movie_session {
                    return Err(invalid("续段的 MOV 同步会话与首段不同"));
                }
                info.frames = info.frames.checked_add(part_info.frames)
                    .ok_or_else(|| invalid("合并的 ZRV 帧数超出上限"))?;
                info.dropped_frames += part_info.dropped_frames;
                info.tracks_dropped_frames &= part_info.tracks_dropped_frames;
                if let (Some(audio), Some(part_audio)) = (&mut info.audio, part_info.audio) {
                    audio.samples = audio.samples.saturating_add(part_audio.samples);
                    audio.status = audio.status.max(part_audio.status);
                }
                missing_footer |= part_info.end.is_none();
                info.end = if missing_footer { None } else { part_info.end };
                info.expired_source = part_info.expired_source;
                parts.push((format!("Z{number:02}"), part));
            }
        }
        Ok(Self { info, parts, part: 0, position: 0 })
    }

    pub fn part_count(&self) -> usize {
        self.parts.len()
    }

    pub fn movie_positions(&self, movie_frames: Option<u32>) -> io::Result<Vec<u32>> {
        let mut positions = Vec::new();
        let (mut previous, mut unknown, mut gap_real) = (0u32, 0u32, 0u32);
        let mut gap_start = 0;
        for (_, clip) in &self.parts {
            let stream = clip.stream.as_ref().filter(|index| index.movie_session.is_some())
                .ok_or_else(|| invalid("此录像没有机内 MOV 同步记录"))?;
            for (position, dropped) in stream.movie_positions() {
                if position == 0 {
                    gap_real += u32::from(!dropped);
                } else {
                    if position <= previous { return Err(invalid("机内记录的 MOV 帧号没有严格递增")); }
                    let gap = &mut positions[gap_start..];
                    // Known positions must leave exactly one MOV slot for each
                    // intervening RAW interval before we can fill its number.
                    if previous != 0 && position - previous - 1 == gap.len() as u32 {
                        for (i, item) in gap.iter_mut().enumerate() { *item = previous + 1 + i as u32; }
                    } else if movie_frames.is_some_and(|total| previous < total) {
                        unknown += gap_real;
                    }
                    previous = position;
                    gap_real = 0;
                    gap_start = positions.len() + 1;
                }
                positions.push(position);
            }
        }
        if previous == 0 { return Err(invalid("没有可用的 MOV 帧对应关系")); }
        // The recorded movie resolves whether an unnumbered RAW tail overlaps
        // its timeline. A tail beyond the last MOV frame is irrelevant here.
        if movie_frames.is_some_and(|total| previous < total) { unknown += gap_real; }
        if unknown != 0 { return Err(invalid(&format!("有 {unknown} 张实拍 RAW 缺少可验证的 MOV 帧号"))); }
        Ok(positions)
    }

    pub fn read_frame_data(&mut self, data: &mut Vec<u8>) -> io::Result<FrameFormat> {
        let (_, clip) = self.parts.get_mut(self.part).ok_or_else(|| invalid("没有更多 RAW 帧"))?;
        let format = clip.read_frame_data(data)?;
        self.position += 1;
        if self.position == clip.info.frames {
            self.part += 1;
            self.position = 0;
        }
        Ok(format)
    }

    pub fn export_audio(&mut self, path: &Path) -> io::Result<()> {
        if self.parts.len() == 1 {
            return self.parts[0].1.export_audio(path);
        }
        if self.info.audio.is_none_or(|audio| !audio.has_samples()) {
            return Ok(());
        }
        let samples = self.parts.iter().map(|(_, clip)| {
            clip.stream.as_ref().unwrap().samples_until(clip.info.frames)
        }).sum();
        let mut out = File::create_new(path)?;
        stream::write_wav_header(&mut out, samples)?;
        for (_, clip) in &mut self.parts {
            clip.stream.as_ref().unwrap().write_audio(&mut clip.reader, &mut out, clip.info.frames)?;
        }
        Ok(())
    }

    pub fn export_synced_audio(&mut self, path: &Path, positions: &[u32], total: u32) -> io::Result<()> {
        if self.info.audio.is_none_or(|audio| !audio.has_samples()) { return Ok(()); }
        let staging = tempfile::tempdir_in(folder(path))?;
        let source = staging.path().join("source.wav");
        self.export_audio(&source)?;
        let mut input = BufReader::new(File::open(source)?);
        let index = self.parts[0].1.stream.as_ref().unwrap();
        let phase = index.bounds().2;
        let rate = 48000 * u64::from(self.info.fps_den);
        let samples = |n: u64| n * rate / u64::from(self.info.fps_num);
        let src_samples = |n: u32| samples(u64::from(n) + u64::from(phase)) - samples(u64::from(phase));
        let data = input.get_ref().metadata()?.len() - src_samples(self.info.frames) * 4;
        let mut out = File::create_new(path)?;
        stream::write_wav_header(&mut out, samples(u64::from(total)))?;
        let (mut raw, mut written) = (0, 0);
        while raw < positions.len() {
            let frame = positions[raw];
            if frame == 0 { raw += 1; continue; }
            if frame > total { break; }
            let mut end = raw + 1;
            while end < positions.len() && positions[end] <= total
                && positions[end - 1].checked_add(1) == Some(positions[end]) { end += 1; }
            let first_sample = samples(u64::from(frame - 1));
            let wanted = samples(u64::from(positions[end - 1])) - first_sample;
            io::copy(&mut io::repeat(0).take((first_sample - written) * 4), &mut out)?;
            let begin = src_samples(raw as u32);
            let copied = wanted.min(src_samples(end as u32) - begin) * 4;
            input.seek(SeekFrom::Start(data + begin * 4))?;
            if io::copy(&mut input.by_ref().take(copied), &mut out)? != copied {
                return Err(invalid("ZRV 同步音频数据不完整"));
            }
            io::copy(&mut io::repeat(0).take(wanted * 4 - copied), &mut out)?;
            raw = end;
            written = first_sample + wanted;
        }
        io::copy(&mut io::repeat(0).take((samples(u64::from(total)) - written) * 4), &mut out)?;
        Ok(())
    }

    fn movie_frame_report(&self, positions: &[u32], total: u32) -> (MovieFrameCounts, Vec<(Range<u32>, MovieFrameKind)>) {
        let mut counts = MovieFrameCounts::default();
        let mut rows = Vec::new();
        let mut add = |range: Range<u32>, kind| {
            if range.is_empty() { return; }
            let count = range.end - range.start;
            match kind {
                MovieFrameKind::Timeout => counts.timeout += count,
                MovieFrameKind::Sync => counts.sync += count,
                MovieFrameKind::Unknown => counts.unknown += count,
            }
            rows.push((range, kind));
        };
        let source = self.parts.iter().flat_map(|(_, clip)| clip.stream.as_ref().unwrap().movie_positions());
        let mut next = 0;
        let mut unmapped_timeout = false;
        let gap_kind = |unmapped| if unmapped { MovieFrameKind::Unknown } else { MovieFrameKind::Sync };
        for ((_, timeout), &frame) in source.zip(positions) {
            if frame == 0 {
                unmapped_timeout |= timeout;
                continue;
            }
            // A timeout without a frame number makes the surrounding MOV gap
            // ambiguous. Report its slots separately until they can be mapped.
            add(next..(frame - 1).min(total), gap_kind(unmapped_timeout));
            next = frame.min(total);
            unmapped_timeout = false;
            if frame > total { break; }
            if timeout { add(frame - 1..frame, MovieFrameKind::Timeout); }
        }
        add(next..total, gap_kind(unmapped_timeout));
        (counts, rows)
    }

    pub fn movie_frame_counts(&self, positions: &[u32], total: u32) -> MovieFrameCounts {
        self.movie_frame_report(positions, total).0
    }

    pub fn export_movie_frame_report(&self, path: &Path, positions: &[u32], total: u32) -> io::Result<MovieFrameCounts> {
        let (counts, rows) = self.movie_frame_report(positions, total);
        let related = |suffix: &str| {
            let mut name = path.file_stem().unwrap_or_default().to_os_string();
            name.push(suffix);
            path.with_file_name(name)
        };
        let paths = [path.to_owned(), related("_timeout.csv"), related("_sync.csv")];
        for path in &paths {
            if path.try_exists()? { return Err(io::ErrorKind::AlreadyExists.into()); }
        }
        let create = || -> io::Result<_> {
            let mut report = tempfile::NamedTempFile::new_in(folder(path))?;
            report.write_all("\u{feff}MOV 帧号,DNG 帧号,序列内时间（秒）,帧类型\r\n".as_bytes())?;
            Ok(report)
        };
        let (mut all, mut timeout, mut sync) = (create()?, create()?, create()?);
        for (range, kind) in rows {
            let (label, mut category) = match kind {
                MovieFrameKind::Timeout => ("超时帧", Some(timeout.as_file_mut())),
                MovieFrameKind::Sync => ("同步帧", Some(sync.as_file_mut())),
                MovieFrameKind::Unknown => ("类型待确认", None),
            };
            for index in range {
                let frame = index + 1;
                let seconds = f64::from(index) / self.info.fps();
                let row = format!("{frame},{frame},{seconds:.6},{label}\r\n");
                all.write_all(row.as_bytes())?;
                if let Some(out) = category.as_mut() { out.write_all(row.as_bytes())?; }
            }
        }
        let mut published = Vec::new();
        for (path, report) in paths.into_iter().zip([all, timeout, sync]) {
            if let Err(failure) = report.persist_noclobber(&path) {
                for path in published { let _ = fs::remove_file(path); }
                return Err(failure.error);
            }
            published.push(path);
        }
        Ok(counts)
    }

    pub fn export_drop_report(&self, path: &Path) -> io::Result<bool> {
        if !self.info.tracks_dropped_frames && self.info.dropped_frames == 0 {
            return Ok(false);
        }
        let mut report = tempfile::NamedTempFile::new_in(folder(path))?;
        let out = report.as_file_mut();
        out.write_all(b"\xef\xbb\xbf")?;
        write!(out, "分段,源帧号,本段帧号,DNG 帧号,序列内时间（秒）\r\n")?;
        let mut offset = 0;
        for (label, clip) in &self.parts {
            if let Some(index) = &clip.stream {
                index.write_drop_rows(out, label, offset)?;
            }
            offset += clip.info.frames;
        }
        report.persist_noclobber(path).map_err(|failure| failure.error)?;
        Ok(true)
    }
}

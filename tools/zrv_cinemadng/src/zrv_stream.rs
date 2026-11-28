use super::{BLACK_LEVEL_TAG, Clip, ClipInfo, FrameFormat, PcmTrack, SLOTS, invalid, word};
use std::fs::File;
use std::io::{self, BufReader, Read, Seek, SeekFrom, Write};
use std::path::Path;

struct AudioBlock {
    offset: u64,
    lead: u32,
    samples: u32,
    duration: u32,
}

struct VideoFrame {
    payload: u64,
    bytes: u32,
    ordinal: u32,
    packed: bool,
}

pub(super) struct Index {
    frames: Vec<VideoFrame>,
    next: usize,
    audio: Vec<AudioBlock>,
    phase: u32,
    fps_num: u32,
    fps_den: u32,
    pub(super) movie_session: Option<u32>,
    movie_frames: Vec<u32>,
}

pub(super) fn pcm_phase_period(fps_num: u32, fps_den: u32) -> u32 {
    match (fps_num, fps_den) {
        (30000 | 60000, 1001) => 5,
        _ => 1,
    }
}

pub(super) fn open(mut reader: BufReader<File>, size: u64, header: &[u8; 64]) -> io::Result<Clip> {
    let version = word(header, 1);
    let hd = matches!(version, 10 | 12 | 13) && word(header, 4) == 2070;
    let (width, height, stride) = if hd { (2070, 1166, 3120) } else { (3826, 2158, 5744) };
    let (fps_num, fps_den) = (word(header, 8), word(header, 9));
    let supported_rate = match (fps_num, fps_den) {
        (25, 1) | (24000 | 30000, 1001) => true,
        (50, 1) | (60000, 1001) => hd,
        _ => false,
    };
    let fields = [
        0x01700072, 64, width, height, stride, 12, fps_num, fps_den, 0x02010100, width, height, 0, 0, 0,
    ];
    if &header[..4] != b"ZRS1"
        || !supported_rate
        || !fields
            .iter()
            .enumerate()
            .all(|(i, &v)| word(header, 2 + i) == v)
    {
        return Err(invalid("ZRV 文件头无效"));
    }
    let mut phase = [0; 4];
    reader.read_exact(&mut phase)?;
    let phase = u32::from_le_bytes(phase);
    if phase >= pcm_phase_period(fps_num, fps_den) {
        return Err(invalid("ZRV 音频分段相位无效"));
    }
    let timeout = if version >= 12 {
        let mut setting = [0; 4];
        reader.read_exact(&mut setting)?;
        let setting = u32::from_le_bytes(setting);
        if setting > 2 {
            return Err(invalid("ZRV 超时设置无效"));
        }
        setting
    } else {
        1
    };
    let movie_session = if version == 13 {
        let mut sync = [0; 8];
        reader.read_exact(&mut sync)?;
        match (&sync[..4], word(&sync, 1)) {
            (b"ZMS1", session) if session != 0 => Some(session),
            (b"\0\0\0\0", 0) => None,
            _ => return Err(invalid("ZRV 的 MOV 同步标记无效")),
        }
    } else { None };
    reader.seek(SeekFrom::Start(80))?;
    let mut rotation = [0; 8];
    reader.read_exact(&mut rotation)?;
    let orientation = match (&rotation[..4], word(&rotation, 1)) {
        (b"ZOR1", tag @ (1 | 3 | 6 | 8)) => tag as u16,
        (b"ZOR1", _) => return Err(invalid("ZRV 方向标记无效")),
        _ => 1,
    };
    let mut info = ClipInfo {
        width, height, stride, fps_num, fps_den,
        frames: 0,
        dropped_frames: 0,
        tracks_dropped_frames: version >= 12,
        prefer_black: timeout == 0,
        end: None,
        expired_source: None,
        black_level: None,
        orientation,
        audio: None,
    };
    let mut index = Index {
        frames: Vec::new(),
        audio: Vec::new(),
        next: 0,
        phase, fps_num, fps_den,
        movie_session,
        movie_frames: Vec::new(),
    };
    let mut offset = 4096u64;
    let mut previous = None::<u32>;
    let (mut timed, mut elapsed, mut longest) = (0u32, 0u32, 0u32);
    let mut samples = 0u64;
    let mut status = 0;
    while offset + 64 <= size {
        reader.seek(SeekFrom::Start(offset))?;
        let mut h = [0; 64];
        reader.read_exact(&mut h)?;
        if word(&h, 1) != 64 {
            return Err(invalid("ZRV 块头无效"));
        }
        match &h[..4] {
            b"ZFR1" => {
                let (packed, dropped) = match (version, word(&h, 15)) {
                    (9, _) | (11..=13, 1) if !hd => (false, false),
                    (10, _) | (11..=13, 2) => (true, false),
                    (12..=13, 3) => (false, true),
                    _ => return Err(invalid("ZRV 帧编码标记无效")),
                };
                let bytes = word(&h, 2);
                let ordinal = word(&h, 3);
                if (packed && bytes as usize != info.packed_bytes()) || (bytes == 0) != dropped
                    || u64::from(bytes) > info.max_nef_bytes()
                    || ordinal == 0
                    || previous.is_some_and(|v| v.checked_add(1) != Some(ordinal))
                    || !SLOTS.contains(&word(&h, 4))
                {
                    return Err(invalid("ZRV 源帧长度或序号无效"));
                }
                let next = offset + (64 + u64::from(bytes)).next_multiple_of(4096);
                if next > size {
                    break;
                }
                let level = (word(&h, 8) == BLACK_LEVEL_TAG).then(|| word(&h, 9));
                if level.is_some_and(|v| v >= 4095 || word(&h, 10) != 1)
                    || (info.frames > 0 && level != info.black_level.map(u32::from))
                {
                    return Err(invalid("ZRV 黑电平无效"));
                }
                index.frames.push(VideoFrame { payload: offset + 64, bytes, ordinal, packed });
                info.frames += 1;
                info.dropped_frames += u32::from(dropped);
                info.black_level = level.map(|v| v as u16);
                previous = Some(ordinal);
                offset = next;
            }
            b"ZAV1" => {
                let (count, lead, n, code) = (word(&h, 4), word(&h, 5), word(&h, 6), word(&h, 7));
                if !(1..=25).contains(&count) || count > info.frames - timed {
                    return Err(invalid("ZRV 音频帧数无效"));
                }
                let duration = (index.samples_until(timed + count) - index.samples_until(timed)) as u32;
                if word(&h, 3) != timed
                    || lead > duration
                    || n > duration - lead
                    || code > 4
                    || (code == 0 && n != duration)
                    || (word(&h, 8), word(&h, 9), word(&h, 10)) != (48000, 2, 16)
                {
                    return Err(invalid("ZRV 音频时间范围无效"));
                }
                let mapping_bytes = if movie_session.is_some() { count * 4 } else { 0 };
                let len = u64::from((64 + 12 * count + 4 * n + mapping_bytes).next_multiple_of(4096));
                if u64::from(word(&h, 2)) != len {
                    return Err(invalid("ZRV 音频块长度无效"));
                }
                if offset + len > size {
                    break;
                }
                for _ in 0..count {
                    let mut row = [0; 12];
                    reader.read_exact(&mut row)?;
                    if !(1..=3).contains(&word(&row, 2)) {
                        return Err(invalid("ZRV 队列深度无效"));
                    }
                    elapsed = elapsed.wrapping_add(word(&row, 1));
                    longest = longest.max(word(&row, 1));
                }
                if mapping_bytes != 0 {
                    reader.seek(SeekFrom::Current(i64::from(n) * 4))?;
                    for _ in 0..count {
                        let mut number = [0; 4];
                        reader.read_exact(&mut number)?;
                        index.movie_frames.push(u32::from_le_bytes(number));
                    }
                }
                index.audio.push(AudioBlock {
                    offset: offset + 64 + 12 * u64::from(count),
                    lead,
                    samples: n,
                    duration,
                });
                timed += count;
                samples += u64::from(n);
                status = status.max(code);
                offset += len;
            }
            b"ZEND" => {
                let reason = word(&h, 2);
                if !(1..=8).contains(&reason)
                    || timed != info.frames
                    || word(&h, 3) != timed
                    || word(&h, 4) != timed
                    || word(&h, 11) != elapsed
                    || word(&h, 12) != longest
                    || offset + 64 != size
                {
                    return Err(invalid("ZRV 文件尾无效"));
                }
                info.end = Some(reason);
                if reason == 4 && word(&h, 8) == 2 && word(&h, 10) != 0 {
                    info.expired_source = Some(word(&h, 10));
                }
                break;
            }
            _ => return Err(invalid("未知 ZRV 数据块")),
        }
    }
    if info.frames == 0 {
        return Err(invalid("ZRV 中没有完整帧"));
    }
    if timed < info.frames {
        status = status.max(3);
    }
    info.audio = Some(PcmTrack {
        offset: 0,
        lead: 0,
        samples: samples.min(u64::from(u32::MAX)) as u32,
        status,
    });
    Ok(Clip {
        info,
        reader,
        encoded_version: None,
        stream: Some(index),
    })
}

impl Index {
    pub(super) fn movie_positions(&self) -> impl Iterator<Item = (u32, bool)> + '_ {
        self.frames.iter().enumerate().map(|(i, frame)| {
            (self.movie_frames.get(i).copied().unwrap_or(0), frame.bytes == 0)
        })
    }

    pub(super) fn bounds(&self) -> (u32, u32, u32) {
        (self.frames[0].ordinal, self.frames[self.frames.len()-1].ordinal, self.phase)
    }

    pub(super) fn write_drop_rows(&self, out: &mut impl Write, part: &str, offset: u32) -> io::Result<()> {
        for (position, frame) in self.frames.iter().enumerate() {
            if frame.bytes == 0 {
                let output = u64::from(offset) + position as u64;
                let seconds = output as f64 * f64::from(self.fps_den) / f64::from(self.fps_num);
                write!(out, "{part},{},{},{},{seconds:.6}\r\n", frame.ordinal, position + 1, output + 1)?;
            }
        }
        Ok(())
    }

    pub(super) fn read_frame_data(
        &mut self,
        reader: &mut BufReader<File>,
        data: &mut Vec<u8>,
    ) -> io::Result<FrameFormat> {
        let frame = self.frames.get(self.next).ok_or_else(|| invalid("没有更多 RAW 帧"))?;
        let format = if frame.bytes == 0 {
            FrameFormat::Missing
        } else {
            reader.seek(SeekFrom::Start(frame.payload))?;
            data.resize(frame.bytes as usize, 0);
            reader.read_exact(data)?;
            if frame.packed {
                FrameFormat::Packed
            } else {
                FrameFormat::NikonNef
            }
        };
        self.next += 1;
        Ok(format)
    }

    pub(super) fn samples_until(&self, frame: u32) -> u64 {
        let scale = 48000 * u64::from(self.fps_den);
        let phase = u64::from(self.phase);
        let rate = u64::from(self.fps_num);
        (phase + u64::from(frame)) * scale / rate - phase * scale / rate
    }

    pub(super) fn export_audio(
        &self,
        reader: &mut BufReader<File>,
        path: &Path,
        frames: u32,
    ) -> io::Result<()> {
        if self.audio.iter().all(|part| part.samples == 0) {
            return Ok(());
        }
        let mut out = File::create_new(path)?;
        write_wav_header(&mut out, self.samples_until(frames))?;
        self.write_audio(reader, &mut out, frames)
    }

    pub(super) fn write_audio(
        &self,
        reader: &mut BufReader<File>,
        out: &mut impl Write,
        frames: u32,
    ) -> io::Result<()> {
        let duration = self.samples_until(frames);
        let mut written = 0;
        for part in &self.audio {
            reader.seek(SeekFrom::Start(part.offset))?;
            io::copy(&mut io::repeat(0).take(u64::from(part.lead) * 4), out)?;
            let length = u64::from(part.samples) * 4;
            if io::copy(&mut reader.by_ref().take(length), out)? != length {
                return Err(invalid("ZRV 音频数据不完整"));
            }
            let tail = part.duration - part.lead - part.samples;
            io::copy(&mut io::repeat(0).take(u64::from(tail) * 4), out)?;
            written += u64::from(part.duration);
        }
        io::copy(&mut io::repeat(0).take((duration - written) * 4), out)?;
        Ok(())
    }
}

pub(super) fn write_wav_header(out: &mut impl Write, duration: u64) -> io::Result<()> {
    let bytes = duration * 4;
    if bytes + 36 <= u64::from(u32::MAX) {
        out.write_all(b"RIFF")?;
        out.write_all(&((bytes + 36) as u32).to_le_bytes())?;
        out.write_all(b"WAVE")?;
    } else {
        out.write_all(b"RF64\xff\xff\xff\xffWAVEds64\x1c\0\0\0")?;
        for v in [bytes + 72, bytes, duration] {
            out.write_all(&v.to_le_bytes())?;
        }
        out.write_all(&0u32.to_le_bytes())?;
    }
    out.write_all(b"fmt \x10\0\0\0\x01\0\x02\0")?;
    out.write_all(&48000u32.to_le_bytes())?;
    out.write_all(&192000u32.to_le_bytes())?;
    out.write_all(b"\x04\0\x10\0data")?;
    let length = if bytes + 36 > u64::from(u32::MAX) {
        u32::MAX
    } else {
        bytes as u32
    };
    out.write_all(&length.to_le_bytes())?;
    Ok(())
}

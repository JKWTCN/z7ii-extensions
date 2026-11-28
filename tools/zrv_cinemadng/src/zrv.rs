use std::fs::File;
use std::io::{self, BufReader, Read, Seek, SeekFrom, Write};
use std::path::Path;

#[path = "zrv_nef.rs"]
mod zrv_nef;
#[path = "zrv_pack.rs"]
mod zrv_pack;
#[path = "zrv_stream.rs"]
mod stream;
#[path = "zrv_sequence.rs"]
mod sequence;
pub use sequence::{ClipSequence, MovieFrameCounts};

const BLOCK_BYTES: u64 = 64;
const WRITE_SAMPLE_BYTES: u64 = 12;
const BLACK_LEVEL_TAG: u32 = u32::from_le_bytes(*b"ZBL1");
const SLOTS: [u32; 4] = [0x54900000, 0x56b0d000, 0x58d1a000, 0x5af27000];
const PEER_SLOTS: [u32; 4] = [0x83900000, 0x85b0d000, 0x87d1a000, 0x89f27000];

#[derive(Clone, Copy)]
pub struct ClipInfo {
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub fps_num: u32,
    pub fps_den: u32,
    pub frames: u32,
    pub dropped_frames: u32,
    pub tracks_dropped_frames: bool,
    pub prefer_black: bool,
    pub end: Option<u32>,
    expired_source: Option<u32>,
    pub black_level: Option<u16>,
    pub orientation: u16,
    pub audio: Option<PcmTrack>,
}

#[derive(Clone, Copy)]
pub struct PcmTrack {
    offset: u64,
    samples: u32,
    lead: u32,
    pub status: u32,
}

impl PcmTrack {
    pub fn has_samples(self) -> bool { self.samples != 0 }
}

impl ClipInfo {
    fn max_nef_bytes(self) -> u64 {
        (u64::from(self.width) * u64::from(self.height) * 19).div_ceil(8)
    }

    pub fn packed_bytes(self) -> usize {
        (self.stride * self.height) as usize
    }

    pub fn fps(self) -> f64 {
        f64::from(self.fps_num) / f64::from(self.fps_den)
    }

    pub fn end_label(self) -> String {
        if let Some(ordinal) = self.expired_source {
            return format!("相机提前结束 RAW 录制 · 源帧 {ordinal} 的缓冲区已轮换");
        }
        match self.end {
            Some(1 | 2 | 7) => "文件正常收尾",
            Some(3) => "相机提前结束 RAW 录制 · 后续帧不连续",
            Some(4) => "相机提前结束 RAW 录制 · 取帧失败",
            Some(5) => "相机提前结束 RAW 录制 · 写卡队列满",
            Some(6) => "相机提前结束 RAW 录制 · 写卡失败",
            Some(8) => "相机提前结束 RAW 录制 · 帧源停滞",
            _ => "缺少完整文件尾，将导出可恢复的帧",
        }.to_owned()
    }
}

pub struct Clip {
    pub info: ClipInfo,
    reader: BufReader<File>,
    encoded_version: Option<u32>,
    stream: Option<stream::Index>,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum FrameFormat {
    Missing,
    Packed,
    NikonNef,
    Lossless { byte_deltas: bool },
}

impl FrameFormat {
    pub fn decode<'a>(
        self,
        data: &'a [u8],
        scratch: &'a mut Vec<u8>,
        info: ClipInfo,
    ) -> io::Result<Option<&'a [u8]>> {
        match self {
            Self::Missing => return Ok(None),
            Self::Packed => return Ok(Some(data)),
            Self::NikonNef => {
                scratch.resize(info.packed_bytes(), 0);
                zrv_nef::unpack(data, scratch, info)?;
            }
            Self::Lossless { byte_deltas } => {
                scratch.resize(info.packed_bytes(), 0);
                zrv_pack::unpack(data, scratch, byte_deltas)?;
            }
        }
        Ok(Some(scratch))
    }
}

fn word(bytes: &[u8], index: usize) -> u32 {
    let offset = index * 4;
    u32::from_le_bytes(bytes[offset..offset + 4].try_into().unwrap())
}

fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}

impl Clip {
    pub fn open(path: &Path) -> io::Result<Self> {
        let mut options = File::options();
        options.read(true);
        #[cfg(windows)]
        {
            use std::os::windows::fs::OpenOptionsExt;
            options.share_mode(1); // FILE_SHARE_READ
        }
        let file = options.open(path)?;
        let size = file.metadata()?.len();
        let mut reader = BufReader::new(file);
        let mut block = [0; BLOCK_BYTES as usize];
        if size < BLOCK_BYTES {
            return Err(invalid("ZRV 文件头不完整"));
        }
        reader.read_exact(&mut block)?;
        let version = word(&block, 1);
        if matches!(version, 9..=13) {
            return stream::open(reader, size, &block);
        }
        let fps_num = word(&block, 8);
        let fps_den = word(&block, 9);
        let limit = word(&block, 15);
        let source_width = word(&block, 11);
        let source_height = word(&block, 12);
        let dual_50p = matches!(
            (version, fps_num, fps_den, source_width, source_height, limit),
            (1, 50, 1, 3856, 2176, 2 | 4) | (8, 50, 1, 3856, 2176, 2 | 50)
        );
        let ntsc = matches!((version, fps_num, fps_den, word(&block, 15)), (8, 24000 | 30000, 1001, 750));
        let step = if ntsc { 1 } else { fps_den };
        let burst = matches!((version, step, limit), (8, 1, 2 | 25 | 250 | 750))
            || (dual_50p && version == 8);
        if &block[..4] != b"ZRS1"
            || (!burst && !matches!((version, fps_den), (1 | 2, 1) | (4, 2 | 3 | 8) | (5..=8, 2)))
            || word(&block, 2) != 0x0170_0072
            || word(&block, 3) != BLOCK_BYTES as u32
            || word(&block, 7) != 12
            || (fps_num != 25 && !ntsc && !dual_50p)
        {
            return Err(invalid("此文件不是支持的 Z7 II ZRV 录像"));
        }
        let (width, height, stride) = (word(&block, 4), word(&block, 5), word(&block, 6));
        let (x, y) = (word(&block, 13), word(&block, 14));
        let live_pair = (source_width, source_height, version, fps_num, fps_den) == (3856, 2176, 8, 25, 1)
            && matches!(limit, 2 | 25);
        let uhd = dual_50p || live_pair || (source_width, source_height) == (3826, 2158);
        let max_stride = if dual_50p || live_pair { 5792 } else if uhd { 5744 } else { 3120 };
        if (uhd
            && ((!dual_50p && !burst && !matches!((version, limit), (2 | 7 | 8, 150) | (8, 750)))
                || (width, height, stride, x, y) != (source_width, source_height, max_stride, 0, 0)))
            || (!uhd && (source_width, source_height) != (2070, 1166))
            || word(&block, 10) != 0x0201_0100
            || width == 0
            || height == 0
            || (width | height | x | y) & 1 != 0
            || u64::from(x) + u64::from(width) > u64::from(source_width)
            || u64::from(y) + u64::from(height) > u64::from(source_height)
            || !(width * 3 / 2..=max_stride).contains(&stride)
        {
            return Err(invalid("ZRV 尺寸、行步长或 Bayer 排列无效"));
        }
        if limit == 0 {
            return Err(invalid("ZRV 帧数上限无效"));
        }
        if (version >= 7 && !uhd)
            || ((4..=6).contains(&version)
                && ((width, height, stride, x, y) != (2070, 1166, 3120, 0, 0)
                    || !matches!((fps_den, limit), (2, 150) | (3, 100) | (8, 4))))
        {
            return Err(invalid("ZRV 全尺寸采样参数无效"));
        }
        let mut info = ClipInfo {
            width,
            height,
            stride,
            fps_num,
            fps_den,
            frames: 0,
            dropped_frames: 0,
            tracks_dropped_frames: false,
            prefer_black: false,
            end: None,
            expired_source: None,
            black_level: None,
            orientation: 1,
            audio: None,
        };
        let mut offset = BLOCK_BYTES;
        let mut ordinal = 0u32;
        let mut first_tick = 0;
        let mut last_source = None::<(bool, u32)>;
        let mut timing = None;
        let mut queue_capacity = match (version, step) {
            _ if live_pair || (dual_50p && version == 8) => 2,
            (1, _) => 4,
            (4, 2 | 3) | (5 | 6, 2) => 3,
            (4, 8) => 1,
            (8, 1) if matches!(limit, 25 | 250 | 750) => 3,
            (7 | 8, _) => 2,
            _ => 10,
        };
        while size - offset >= BLOCK_BYTES {
            reader.read_exact(&mut block)?;
            if word(&block, 1) != BLOCK_BYTES as u32 {
                return Err(invalid("ZRV 块头长度无效"));
            }
            match &block[..4] {
                b"ZFR1" => {
                    let current = word(&block, 3);
                    let address = word(&block, 4);
                    let peer = dual_50p && PEER_SLOTS.contains(&address);
                    let tick = word(&block, 5);
                    let bytes = u64::from(word(&block, 2));
                    let valid_size = if version >= 7 {
                        (1..=info.max_nef_bytes()).contains(&bytes)
                    } else if version >= 5 {
                        zrv_pack::valid_size(bytes, version == 6)
                    } else {
                        bytes == info.packed_bytes() as u64
                    };
                    if timing.is_some()
                        || info.frames == limit
                        || !valid_size
                        || current == 0
                        || (info.frames != 0 && ordinal.checked_add(step) != Some(current))
                        || !(peer || SLOTS.contains(&address))
                        || (dual_50p
                            && last_source.is_some_and(|(previous, time)| {
                                peer == previous || !(16..=24).contains(&tick.wrapping_sub(time))
                            }))
                    {
                        return Err(invalid("ZRV 帧序号、像素长度或源缓冲地址无效"));
                    }
                    let stored = if version == 8 {
                        bytes.next_multiple_of(BLOCK_BYTES)
                    } else {
                        bytes
                    };
                    if size - offset - BLOCK_BYTES < stored {
                        break;
                    }
                    let black_level = if word(&block, 8) == BLACK_LEVEL_TAG {
                        let level = word(&block, 9);
                        if level >= 4095 || word(&block, 10) != 1 {
                            return Err(invalid("ZRV 黑电平参考值无效"));
                        }
                        Some(level as u16)
                    } else {
                        None
                    };
                    if info.frames != 0 && info.black_level != black_level {
                        return Err(invalid("ZRV 各帧的黑电平参考值不一致"));
                    }
                    info.black_level = black_level;
                    if info.frames == 0 { first_tick = word(&block, 5); }
                    info.frames += 1;
                    ordinal = current;
                    last_source = Some((peer, tick));
                    offset += BLOCK_BYTES + stored;
                    reader.seek(SeekFrom::Start(offset))?;
                }
                b"ZTIO" => {
                    if version == 1
                        || timing.is_some()
                        || word(&block, 2) != WRITE_SAMPLE_BYTES as u32
                        || word(&block, 3) != info.frames
                    {
                        return Err(invalid("ZRV 逐帧写入统计无效"));
                    }
                    if ((live_pair && limit == 25) || (dual_50p && limit == 50)) && word(&block, 4) == 2 {
                        queue_capacity = word(&block, 11);
                        if !matches!(queue_capacity, 2 | 3) {
                            return Err(invalid("ZRV 写入缓冲数量无效"));
                        }
                    }
                    let bytes = u64::from(info.frames) * WRITE_SAMPLE_BYTES;
                    if size - offset - BLOCK_BYTES < bytes {
                        break;
                    }
                    let mut entry = [0; WRITE_SAMPLE_BYTES as usize];
                    let (mut total, mut longest) = (0u64, 0u32);
                    for _ in 0..info.frames {
                        reader.read_exact(&mut entry)?;
                        if !(1..=queue_capacity).contains(&word(&entry, 2)) {
                            return Err(invalid("ZRV 写入队列深度无效"));
                        }
                        let elapsed = word(&entry, 1);
                        total += u64::from(elapsed);
                        longest = longest.max(elapsed);
                    }
                    timing = Some((total, longest));
                    offset += BLOCK_BYTES + bytes;
                }
                b"ZAU1" => {
                    let bytes = u64::from(word(&block, 2));
                    let lead = word(&block, 8);
                    let samples = word(&block, 9);
                    let status = word(&block, 10);
                    let timeline = (u64::from(lead)+u64::from(samples))*u64::from(fps_num);
                    let duration = u64::from(info.frames)*48000*u64::from(fps_den);
                    if info.audio.is_some() || timing.is_none() || info.frames == 0
                        || (word(&block, 3),word(&block, 4),word(&block, 5),word(&block, 6)) != (1,48000,2,16)
                        || word(&block, 7) != first_tick || bytes != u64::from(samples)*4
                        || timeline > duration || status > 4 || block[52..].iter().any(|&v| v != 0)
                        || (status == 0 && (lead != 0 || timeline != duration)) {
                        return Err(invalid("ZRV 音轨格式或采样数无效"));
                    }
                    if size-offset-BLOCK_BYTES < bytes { break; }
                    info.audio = Some(PcmTrack {offset: offset+BLOCK_BYTES, samples, lead, status});
                    offset += BLOCK_BYTES+bytes;
                    reader.seek(SeekFrom::Start(offset))?;
                }
                b"ZEND" => {
                    let reason = word(&block, 2);
                    if !(1..=8).contains(&reason)
                        || word(&block, 3) != info.frames
                        || word(&block, 4) != info.frames
                        || word(&block, 7) > queue_capacity
                        || (reason == 1) != (info.frames == limit)
                        || size - offset != BLOCK_BYTES
                        || (version != 1
                            && timing != Some((u64::from(word(&block, 11)), word(&block, 12))))
                    {
                        return Err(invalid("ZRV 文件尾与帧数据不一致"));
                    }
                    info.end = Some(reason);
                    break;
                }
                _ => return Err(invalid("ZRV 包含未知数据块")),
            }
        }
        if info.frames == 0 {
            return Err(invalid("ZRV 中没有可导出的完整帧"));
        }
        reader.seek(SeekFrom::Start(BLOCK_BYTES))?;
        Ok(Self {
            info,
            reader,
            encoded_version: (version >= 5).then_some(version),
            stream: None,
        })
    }

    pub fn export_audio(&mut self, path: &Path) -> io::Result<()> {
        if let Some(index) = &self.stream {
            return index.export_audio(&mut self.reader, path, self.info.frames);
        }
        let Some(track) = self.info.audio else { return Ok(()) };
        if track.samples == 0 { return Ok(()); }
        let bytes = track.samples.checked_add(track.lead).and_then(|n| n.checked_mul(4))
            .filter(|&n| n <= u32::MAX-36).ok_or_else(|| invalid("音轨超出 WAV 长度上限"))?;
        let mut out = File::create_new(path)?;
        out.write_all(b"RIFF")?;
        out.write_all(&(36+bytes).to_le_bytes())?;
        out.write_all(b"WAVEfmt \x10\0\0\0\x01\0\x02\0")?;
        out.write_all(&48000u32.to_le_bytes())?;
        out.write_all(&192000u32.to_le_bytes())?;
        out.write_all(b"\x04\0\x10\0data")?;
        out.write_all(&bytes.to_le_bytes())?;
        io::copy(&mut io::repeat(0).take(u64::from(track.lead)*4), &mut out)?;
        self.reader.seek(SeekFrom::Start(track.offset))?;
        let expected = u64::from(track.samples)*4;
        if io::copy(&mut self.reader.by_ref().take(expected), &mut out)? != expected {
            return Err(invalid("ZRV 音频数据提前结束"));
        }
        Ok(())
    }

    fn read_frame_data(&mut self, data: &mut Vec<u8>) -> io::Result<FrameFormat> {
        if let Some(index) = &mut self.stream {
            return index.read_frame_data(&mut self.reader, data);
        }
        if let Some(version) = self.encoded_version {
            let mut block = [0; BLOCK_BYTES as usize];
            self.reader.read_exact(&mut block)?;
            let bytes = word(&block, 2);
            let byte_deltas = version == 6;
            let valid_size = if version >= 7 {
                (1..=self.info.max_nef_bytes()).contains(&u64::from(bytes))
            } else {
                zrv_pack::valid_size(u64::from(bytes), byte_deltas)
            };
            if &block[..4] != b"ZFR1" || !valid_size {
                return Err(invalid("ZRV 无损 RAW 帧无效"));
            }
            data.resize(bytes as usize, 0);
            self.reader.read_exact(data)?;
            if version == 8 {
                let mut pad = [0; BLOCK_BYTES as usize];
                let count = bytes.next_multiple_of(BLOCK_BYTES as u32) - bytes;
                let padding = &mut pad[..count as usize];
                self.reader.read_exact(padding)?;
                if padding.iter().any(|&value| value != 0) {
                    return Err(invalid("ZRV 无损 RAW 帧对齐填充无效"));
                }
            }
            Ok(if version >= 7 { FrameFormat::NikonNef } else { FrameFormat::Lossless { byte_deltas } })
        } else {
            self.reader.seek_relative(BLOCK_BYTES as i64)?;
            data.resize(self.info.packed_bytes(), 0);
            self.reader.read_exact(data)?;
            Ok(FrameFormat::Packed)
        }
    }
}

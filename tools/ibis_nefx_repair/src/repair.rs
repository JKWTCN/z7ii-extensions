use std::fs::{self, File};
use std::io::{self, BufReader, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

const ENTRY_SIZE: usize = 12;
const NIKON_NOTE: &[u8] = b"Nikon\0\x02\x11\0\0II*\0\x08\0\0\0";
const OUTPUT_SUFFIX: &str = "_NX_compatible";

type Field = (u16, u32, u32);

struct Ifd {
    entries: Vec<(u16, Field)>,
}

impl Ifd {
    fn read(file: &mut File, offset: u64, limit: u64) -> io::Result<Self> {
        let count = usize::from(u16::from_le_bytes(read_at(file, offset)?));
        let size = count * ENTRY_SIZE;
        require(offset + 2 + size as u64 <= limit, "NEFX 目录不完整")?;
        let mut bytes = vec![0; size];
        file.read_exact(&mut bytes)?;
        let entries = bytes
            .chunks_exact(ENTRY_SIZE)
            .map(|entry| {
                let tag = u16::from_le_bytes([entry[0], entry[1]]);
                let kind = u16::from_le_bytes([entry[2], entry[3]]);
                let count = le_u32(&entry[4..8]);
                let value = le_u32(&entry[8..12]);
                (tag, (kind, count, value))
            })
            .collect();
        Ok(Self { entries })
    }

    fn field(&self, tag: u16) -> io::Result<Field> {
        self.entries
            .iter()
            .rfind(|entry| entry.0 == tag)
            .map(|entry| entry.1)
            .ok_or_else(|| invalid(format!("NEFX 缺少元数据标签 0x{tag:04X}")))
    }

    fn size(&self) -> u64 {
        (2 + self.entries.len() * ENTRY_SIZE + 4) as u64
    }
}

fn le_u32(bytes: &[u8]) -> u32 {
    u32::from_le_bytes(bytes.try_into().unwrap())
}

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

fn require(condition: bool, message: &str) -> io::Result<()> {
    if condition {
        Ok(())
    } else {
        Err(invalid(message))
    }
}

fn read_at<const N: usize>(file: &mut File, offset: u64) -> io::Result<[u8; N]> {
    let mut bytes = [0; N];
    file.seek(SeekFrom::Start(offset))?;
    file.read_exact(&mut bytes)?;
    Ok(bytes)
}

fn note_field<const N: usize>(
    file: &mut File,
    ifd: &Ifd,
    note: u64,
    note_size: u64,
    tag: u16,
) -> io::Result<[u8; N]> {
    let (kind, count, offset) = ifd.field(tag)?;
    // MakerNote offsets are relative to its embedded TIFF header.
    let offset = u64::from(offset) + 10;
    require(
        kind == 7
            && count as usize == N
            && offset >= NIKON_NOTE.len() as u64 + ifd.size()
            && offset + N as u64 <= note_size,
        "Nikon 像素变化元数据格式不符",
    )?;
    read_at(file, note + offset)
}

fn compression_offset(file: &mut File) -> io::Result<(u64, u32)> {
    let file_size = file.metadata()?.len();
    let header: [u8; 8] = read_at(file, 0)?;
    require(&header[..4] == b"II*\0", "需要小端格式的 Nikon NEFX")?;
    let root = Ifd::read(file, u64::from(le_u32(&header[4..])), file_size)?;
    let (kind, size, model) = root.field(0x110)?;
    require(
        (kind, size) == (2, 12) && &read_at::<12>(file, u64::from(model))? == b"NIKON Z 7_2\0",
        "需要 Nikon Z7 II 合成文件",
    )?;

    let (kind, count, sub_ifds) = root.field(0x14a)?;
    require(
        kind == 4 && (2..=8).contains(&count),
        "NEFX 图像目录格式不符",
    )?;
    let sensor_offset = u32::from_le_bytes(read_at(file, u64::from(sub_ifds) + 4)?);
    let sensor = Ifd::read(file, u64::from(sensor_offset), file_size)?;
    let width = sensor.field(0x100)?.2;
    let height = sensor.field(0x101)?.2;
    require(
        matches!((width, height), (8256, 5504) | (16512, 11008)),
        "Z7 II NEFX 图像尺寸不符",
    )?;
    let strip_size = (u64::from(width) * u64::from(height) * 3 * 14 / 8) as u32;
    for (tag, expected) in [
        (0xfe, (4, 1, 0)),
        (0x103, (3, 1, 34713)),
        (0x106, (3, 1, 2)),
        (0x115, (3, 1, 3)),
        (0x116, (4, 1, height)),
        (0x117, (4, 1, strip_size)),
        (0x11c, (3, 1, 1)),
    ] {
        require(sensor.field(tag)? == expected, "需要单条带打包的 RGB 数据")?;
    }
    let (kind, count, bits) = sensor.field(0x102)?;
    require(
        (kind, count) == (3, 3) && read_at::<6>(file, u64::from(bits))? == [14, 0, 14, 0, 14, 0],
        "需要三个 14 位通道",
    )?;
    let (kind, count, strip) = sensor.field(0x111)?;
    let strip = u64::from(strip);
    require(
        (kind, count) == (4, 1) && strip >= 8 && strip + u64::from(strip_size) <= file_size,
        "NEFX 像素数据不完整",
    )?;

    let exif = Ifd::read(file, u64::from(root.field(0x8769)?.2), file_size)?;
    let (kind, note_size, note) = exif.field(0x927c)?;
    let (note_size, note) = (u64::from(note_size), u64::from(note));
    let ifd_offset = note + NIKON_NOTE.len() as u64;
    let note_end = note + note_size;
    require(
        kind == 7
            && ifd_offset + 6 <= note_end
            && note_end <= strip
            && read_at::<{ NIKON_NOTE.len() }>(file, note)? == NIKON_NOTE,
        "Nikon MakerNote 格式不符",
    )?;
    let ifd = Ifd::read(file, ifd_offset, note_end)?;
    require(
        ifd_offset + ifd.size() <= note_end,
        "Nikon MakerNote 目录不完整",
    )?;

    let pixel_shift: [u8; 16] = note_field(file, &ifd, note, note_size, 0xe2d)?;
    require(
        &pixel_shift[..5] == b"0100\x02" && (1..=4).contains(&pixel_shift[6]),
        "需要像素变化合成后的 NEFX",
    )?;
    let compression: [u8; 24] = note_field(file, &ifd, note, note_size, 0x51)?;
    require(
        matches!(&compression[..4], b"0101" | b"0102") && compression[10..12] == [10, 0],
        "缺少 14 位打包压缩标记",
    )?;
    let (index, (_, (kind, count, value))) = ifd
        .entries
        .iter()
        .enumerate()
        .find(|(_, entry)| entry.0 == 0x93)
        .ok_or_else(|| invalid("缺少旧压缩标记"))?;
    require(
        (*kind, *count) == (3, 1) && matches!(value, 3 | 10),
        "旧压缩标记格式不符",
    )?;
    Ok((ifd_offset + (2 + index * ENTRY_SIZE + 8) as u64, *value))
}

fn is_nefx(path: &Path) -> bool {
    path.extension()
        .is_some_and(|extension| extension.eq_ignore_ascii_case("nefx"))
}

pub fn source_files(path: &Path) -> io::Result<Vec<PathBuf>> {
    if !path.is_dir() {
        return Ok(vec![path.to_path_buf()]);
    }

    let mut sources = Vec::new();
    for entry in fs::read_dir(path)? {
        let item = entry?.path();
        if is_nefx(&item)
            && !item
                .file_stem()
                .unwrap_or_default()
                .to_string_lossy()
                .to_ascii_lowercase()
                .ends_with("_nx_compatible")
            && item.is_file()
        {
            sources.push(item);
        }
    }
    sources.sort();
    Ok(sources)
}

pub fn repair(source: &Path) -> io::Result<Option<PathBuf>> {
    require(is_nefx(source), "请选择合成后的 .NEFX 文件")?;
    let mut name = source.file_stem().unwrap_or_default().to_os_string();
    name.push(OUTPUT_SUFFIX);
    name.push(".NEFX");
    let target = source.with_file_name(name);
    let mut raw = File::open(source)?;
    let (offset, compression) = compression_offset(&mut raw)?;
    if compression == 10 {
        return Ok(None);
    }
    if target.try_exists()? {
        return Err(output_exists(&target));
    }

    let directory = target
        .parent()
        .filter(|path| !path.as_os_str().is_empty())
        .unwrap_or_else(|| Path::new("."));
    let mut copy = tempfile::Builder::new()
        .prefix(".z72-nefx-")
        .tempfile_in(directory)?;
    raw.rewind()?;
    io::copy(
        &mut BufReader::with_capacity(1024 * 1024, raw),
        copy.as_file_mut(),
    )?;
    require(
        read_at::<2>(copy.as_file_mut(), offset)? == [3, 0],
        "复制期间旧压缩标记发生变化",
    )?;
    copy.seek(SeekFrom::Start(offset))?;
    copy.write_all(&[10])?;
    match copy.persist_noclobber(&target) {
        Ok(_) => Ok(Some(target)),
        Err(error) if error.error.kind() == io::ErrorKind::AlreadyExists => {
            Err(output_exists(&target))
        }
        Err(error) => Err(error.error),
    }
}

fn output_exists(target: &Path) -> io::Error {
    io::Error::new(
        io::ErrorKind::AlreadyExists,
        format!("输出已存在：{}", target.display()),
    )
}

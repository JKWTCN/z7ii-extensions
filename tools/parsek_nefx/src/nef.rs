use anyhow::{Context, Result, ensure};
use rawler::RawImageData;
use std::collections::BTreeMap;
use std::fs::File;
use std::io::{Read, Seek, SeekFrom};
use std::path::Path;

pub const RAW_WIDTH: usize = 8288;
pub const RAW_HEIGHT: usize = 5520;
pub const LEFT: usize = 16;
pub const TOP: usize = 8;
pub const WIDTH: usize = 8256;
pub const HEIGHT: usize = 5504;
pub const CFA: [usize; 4] = [0, 1, 1, 2];
const NIKON_NOTE: &[u8] = b"Nikon\0\x02\x11\0\0II*\0\x08\0\0\0";

#[derive(Clone, Copy)]
pub struct Field {
    pub kind: u16,
    pub count: u32,
    pub value: u32,
}

impl Field {
    pub fn new(kind: u16, count: u32, value: u32) -> Self {
        Self { kind, count, value }
    }

    pub fn size(self) -> Result<usize> {
        let widths = [0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8, 4];
        let width = widths.get(self.kind as usize).copied().unwrap_or(0);
        ensure!(width != 0, "unsupported TIFF field type {}", self.kind);
        Ok(self.count as usize * width)
    }

    pub fn offset(self, kind: u16, count: u32) -> Result<usize> {
        ensure!(
            (self.kind, self.count) == (kind, count),
            "unexpected TIFF field type or count"
        );
        Ok(self.value as usize)
    }
}

pub struct Ifd {
    pub fields: BTreeMap<u16, Field>,
    pub next: u32,
}

impl Ifd {
    pub fn read(bytes: &[u8], at: usize) -> Result<Self> {
        let count = u16::from_le_bytes(slice(bytes, at, 2)?.try_into().unwrap()) as usize;
        let body = slice(bytes, at + 2, count * 12 + 4)?;
        let mut fields = BTreeMap::new();
        for e in body[..count * 12].chunks_exact(12) {
            let tag = u16::from_le_bytes([e[0], e[1]]);
            let kind = u16::from_le_bytes([e[2], e[3]]);
            let field = Field::new(kind, le32(&e[4..8]), le32(&e[8..12]));
            ensure!(
                fields.insert(tag, field).is_none(),
                "duplicate TIFF tag 0x{tag:04x}"
            );
        }
        Ok(Self {
            fields,
            next: le32(&body[count * 12..]),
        })
    }

    pub fn size(&self) -> usize {
        6 + self.fields.len() * 12
    }

    pub fn get(&self, tag: u16) -> Result<Field> {
        self.fields
            .get(&tag)
            .copied()
            .with_context(|| format!("missing TIFF tag 0x{tag:04x}"))
    }

    pub fn set(&mut self, tag: u16, kind: u16, count: u32, value: usize) {
        self.fields
            .insert(tag, Field::new(kind, count, value as u32));
    }

    pub fn encode(&self) -> Vec<u8> {
        let mut bytes = Vec::with_capacity(self.size());
        bytes.extend_from_slice(&(self.fields.len() as u16).to_le_bytes());
        for (&tag, field) in &self.fields {
            bytes.extend_from_slice(&tag.to_le_bytes());
            bytes.extend_from_slice(&field.kind.to_le_bytes());
            bytes.extend_from_slice(&field.count.to_le_bytes());
            bytes.extend_from_slice(&field.value.to_le_bytes());
        }
        bytes.extend_from_slice(&self.next.to_le_bytes());
        bytes
    }
}

pub fn slice(bytes: &[u8], at: usize, size: usize) -> Result<&[u8]> {
    bytes
        .get(at..at + size)
        .context("TIFF field is outside its container")
}

pub fn le32(bytes: &[u8]) -> u32 {
    u32::from_le_bytes(bytes.try_into().unwrap())
}

pub fn read_at(file: &mut File, at: usize, size: usize) -> Result<Vec<u8>> {
    let mut bytes = vec![0; size];
    file.seek(SeekFrom::Start(at as u64))?;
    file.read_exact(&mut bytes)?;
    Ok(bytes)
}

fn read_ifd(file: &mut File, at: usize) -> Result<Ifd> {
    let count = read_at(file, at, 2)?;
    let count = u16::from_le_bytes(count.try_into().unwrap()) as usize;
    Ok(Ifd::read(&read_at(file, at, 6 + count * 12)?, 0)?)
}

pub struct Nef {
    pub root: Ifd,
    pub root_at: usize,
    pub raw: Ifd,
    pub raw_at: usize,
    pub exif: Ifd,
    pub exif_at: usize,
    pub strip: usize,
    pub note: Vec<u8>,
    pub note_ifd: Ifd,
    pub orientation: u32,
    pub exposure: [Vec<u8>; 3],
}

impl Nef {
    pub fn read(path: &Path) -> Result<Self> {
        let mut file = File::open(path)?;
        let header = read_at(&mut file, 0, 8)?;
        ensure!(
            &header[..4] == b"II*\0",
            "expected a little-endian Nikon NEF"
        );
        let root_at = le32(&header[4..]) as usize;
        let root = read_ifd(&mut file, root_at)?;
        let model = root.get(0x110)?.offset(2, 12)?;
        ensure!(
            read_at(&mut file, model, 12)? == b"NIKON Z 7_2\0",
            "expected a Nikon Z7 II NEF"
        );
        let subs = root.get(0x14a)?;
        ensure!(
            subs.kind == 4 && (2..=8).contains(&subs.count),
            "unexpected NEF SubIFDs"
        );
        let raw_at = le32(&read_at(&mut file, subs.value as usize + 4, 4)?) as usize;
        let raw = read_ifd(&mut file, raw_at)?;
        for (tag, kind, value) in [
            (0x100, 4, RAW_WIDTH),
            (0x101, 4, RAW_HEIGHT),
            (0x102, 3, 14),
            (0x103, 3, 34713),
            (0x106, 3, 32803),
            (0x115, 3, 1),
            (0x116, 4, RAW_HEIGHT),
            (0x828d, 3, 0x0002_0002),
            (0x828e, 1, 0x0201_0100),
        ] {
            ensure!(
                raw.get(tag)?.offset(
                    kind,
                    if tag == 0x828e {
                        4
                    } else if tag == 0x828d {
                        2
                    } else {
                        1
                    }
                )? == value,
                "expected a full-size 14-bit RGGB NEF (tag 0x{tag:04x})"
            );
        }
        let strip = raw.get(0x111)?.offset(4, 1)?;
        let strip_size = raw.get(0x117)?.offset(4, 1)?;
        let file_size = file.metadata()?.len();
        ensure!(
            strip <= 32 * 1024 * 1024 && (strip + strip_size) as u64 <= file_size,
            "unsupported or incomplete NEF strip"
        );
        let exif_at = root.get(0x8769)?.offset(4, 1)?;
        let exif = read_ifd(&mut file, exif_at)?;
        for (at, ifd) in [(root_at, &root), (raw_at, &raw), (exif_at, &exif)] {
            ensure!(
                at >= 8 && at + ifd.size() <= strip,
                "NEF directory outside metadata prefix"
            );
        }
        let maker = exif.get(0x927c)?;
        ensure!(
            maker.kind == 7 && (24..=512 * 1024).contains(&maker.count),
            "unexpected Nikon MakerNote"
        );
        let note = read_at(&mut file, maker.value as usize, maker.count as usize)?;
        ensure!(
            note.starts_with(NIKON_NOTE),
            "unexpected Nikon MakerNote header"
        );
        let note_ifd = Ifd::read(&note, NIKON_NOTE.len())?;
        let format = note_ifd.get(0x93)?.offset(3, 1)?;
        ensure!(
            matches!(format, 3 | 10),
            "use 14-bit lossless compressed or uncompressed NEF"
        );
        let crop = note_ifd.get(0x45)?.offset(3, 4)?;
        ensure!(
            slice(&note, 10 + crop, 8)? == [16, 0, 8, 0, 64, 32, 128, 21],
            "expected the Z7 II FX large image area"
        );

        let mut exposure = [Vec::new(), Vec::new(), Vec::new()];
        for (dest, tag) in exposure.iter_mut().zip([0x829a, 0x829d, 0x8827]) {
            let field = exif.get(tag)?;
            let size = field.size()?;
            ensure!((1..=8).contains(&size), "unexpected EXIF exposure field");
            *dest = if size <= 4 {
                field.value.to_le_bytes()[..size].to_vec()
            } else {
                read_at(&mut file, field.value as usize, size)?
            };
        }
        let orientation = root.get(0x112)?.value;
        Ok(Self {
            root,
            root_at,
            raw,
            raw_at,
            exif,
            exif_at,
            strip,
            note,
            note_ifd,
            orientation,
            exposure,
        })
    }
}

pub fn decode(path: &Path) -> Result<Vec<u16>> {
    let image = rawler::decode_file(path)?;
    ensure!(
        (image.width, image.height, image.cpp) == (RAW_WIDTH, RAW_HEIGHT, 1),
        "decoded NEF geometry differs from the sensor directory"
    );
    let RawImageData::Integer(samples) = image.data else {
        anyhow::bail!("expected integer Bayer samples");
    };
    ensure!(
        samples.len() == RAW_WIDTH * RAW_HEIGHT,
        "decoded NEF sample count is incorrect"
    );
    Ok(samples)
}

use crate::zrv::ClipInfo;
use std::fs::File;
use std::io::{self, BufReader, Read, Seek, SeekFrom};
use std::path::Path;

#[derive(Clone, Copy)]
struct Atom {
    kind: [u8; 4],
    start: u64,
    end: u64,
}

fn invalid() -> io::Error {
    io::Error::new(
        io::ErrorKind::InvalidData,
        "MOV 视频轨不是可同步的单一固定帧率时间线",
    )
}

impl Atom {
    fn read<const N: usize>(self, file: &mut File, offset: u64) -> io::Result<[u8; N]> {
        if offset + N as u64 > self.end - self.start {
            return Err(invalid());
        }
        file.seek(SeekFrom::Start(self.start + offset))?;
        let mut bytes = [0; N];
        file.read_exact(&mut bytes)?;
        Ok(bytes)
    }

    fn children(self, file: &mut File) -> io::Result<Vec<Atom>> {
        let mut result = Vec::new();
        let mut start = self.start;
        while start < self.end {
            let remaining = self.end - start;
            if remaining < 8 {
                return Err(invalid());
            }
            let header = Atom { start, ..self }.read::<8>(file, 0)?;
            let (size, prefix) = match be(&header, 0) {
                0 => (remaining, 8),
                1 => (
                    u64::from_be_bytes(Atom { start, ..self }.read::<8>(file, 8)?),
                    16,
                ),
                n => (u64::from(n), 8),
            };
            if size < prefix || size > remaining || result.len() == 4096 {
                return Err(invalid());
            }
            result.push(Atom {
                kind: header[4..8].try_into().unwrap(),
                start: start + prefix,
                end: start + size,
            });
            start += size;
        }
        Ok(result)
    }

    fn child(self, file: &mut File, kind: &[u8; 4]) -> io::Result<Atom> {
        self.children(file)?
            .into_iter()
            .find(|atom| &atom.kind == kind)
            .ok_or_else(invalid)
    }

    fn clock(self, file: &mut File) -> io::Result<(u32, u64)> {
        match self.read::<1>(file, 0)?[0] {
            0 => {
                let fields = self.read::<8>(file, 12)?;
                Ok((be(&fields, 0), u64::from(be(&fields, 4))))
            }
            1 => {
                let fields = self.read::<12>(file, 20)?;
                Ok((
                    be(&fields, 0),
                    u64::from_be_bytes(fields[4..].try_into().unwrap()),
                ))
            }
            _ => Err(invalid()),
        }
    }
}

fn be(buf: &[u8], offset: usize) -> u32 {
    u32::from_be_bytes(buf[offset..offset + 4].try_into().unwrap())
}

fn check_presentation(
    file: &mut File,
    composition: Option<Atom>,
    media_start: u64,
    count: u32,
    delta: u32,
) -> io::Result<()> {
    let Some(composition) = composition else {
        return if media_start == 0 {
            Ok(())
        } else {
            Err(invalid())
        };
    };
    let header = composition.read::<8>(file, 0)?;
    let entries = be(&header, 4);
    if be(&header, 0) & 0x00ff_ffff != 0
        || header[0] > 1
        || entries == 0
        || entries > count
        || composition.end - composition.start != 8 + u64::from(entries) * 8
    {
        return Err(invalid());
    }
    let mut intervals = Vec::new();
    intervals
        .try_reserve_exact(entries as usize)
        .map_err(|_| invalid())?;
    let mut reader = BufReader::new(file.take(u64::from(entries) * 8));
    let mut decoded = 0u32;
    for _ in 0..entries {
        let mut row = [0; 8];
        reader.read_exact(&mut row)?;
        let run = be(&row, 0);
        let next = decoded
            .checked_add(run)
            .filter(|&end| run != 0 && end <= count)
            .ok_or_else(invalid)?;
        let ticks = if header[0] == 0 {
            i128::from(be(&row, 4))
        } else {
            i128::from(be(&row, 4) as i32)
        };
        let adjusted = ticks - i128::from(media_start);
        if adjusted % i128::from(delta) != 0 {
            return Err(invalid());
        }
        let shift = adjusted / i128::from(delta);
        let start = i128::from(decoded) + shift;
        let end = i128::from(next) + shift;
        if start < 0 || end > i128::from(count) {
            return Err(invalid());
        }
        intervals.push((start as u32, end as u32));
        decoded = next;
    }
    if decoded != count {
        return Err(invalid());
    }

    // Each run covers adjacent presentation slots. Reordering must leave
    // exactly one display frame at every position on the constant-rate grid.
    intervals.sort_unstable();
    let mut cursor = 0;
    for (start, end) in intervals {
        if start != cursor {
            return Err(invalid());
        }
        cursor = end;
    }
    if cursor != count {
        return Err(invalid());
    }
    Ok(())
}

pub fn frames(path: &Path, info: ClipInfo) -> io::Result<u32> {
    let mut file = File::open(path)?;
    let root = Atom {
        kind: *b"root",
        start: 0,
        end: file.metadata()?.len(),
    };
    let top = root.children(&mut file)?;
    if top.iter().any(|atom| &atom.kind == b"moof") {
        return Err(invalid());
    }
    let movie = top
        .into_iter()
        .find(|atom| &atom.kind == b"moov")
        .ok_or_else(invalid)?;
    let (movie_scale, _) = movie.child(&mut file, b"mvhd")?.clock(&mut file)?;
    let mut frames = None;
    for track in movie
        .children(&mut file)?
        .into_iter()
        .filter(|atom| &atom.kind == b"trak")
    {
        let media = track.child(&mut file, b"mdia")?;
        if &media.child(&mut file, b"hdlr")?.read::<4>(&mut file, 8)? != b"vide" {
            continue;
        }
        if frames.is_some() {
            return Err(invalid());
        }
        let (scale, duration) = media.child(&mut file, b"mdhd")?.clock(&mut file)?;
        let table = media.child(&mut file, b"minf")?.child(&mut file, b"stbl")?;
        let composition = table
            .children(&mut file)?
            .into_iter()
            .find(|atom| &atom.kind == b"ctts");
        let codec = table.child(&mut file, b"stsd")?.read::<16>(&mut file, 0)?;
        if be(&codec, 4) != 1 || &codec[12..] != b"avc1" {
            return Err(invalid());
        }
        let timing = table.child(&mut file, b"stts")?.read::<16>(&mut file, 0)?;
        let (count, delta) = (be(&timing, 8), be(&timing, 12));
        let sizes = table.child(&mut file, b"stsz")?.read::<12>(&mut file, 0)?;
        if be(&timing, 4) != 1
            || count == 0
            || delta == 0
            || scale == 0
            || movie_scale == 0
            || be(&sizes, 8) != count
            || u64::from(count) * u64::from(delta) != duration
            || u64::from(delta) * u64::from(info.fps_num)
                != u64::from(scale) * u64::from(info.fps_den)
        {
            return Err(invalid());
        }
        let mut media_start = 0;
        if let Some(edits) = track
            .children(&mut file)?
            .into_iter()
            .find(|atom| &atom.kind == b"edts")
        {
            let edits = edits.child(&mut file, b"elst")?;
            let header = edits.read::<8>(&mut file, 0)?;
            if be(&header, 4) != 1 {
                return Err(invalid());
            }
            let (edited, origin) = match header[0] {
                0 => {
                    let data = edits.read::<12>(&mut file, 8)?;
                    if be(&data, 8) != 0x0001_0000 {
                        return Err(invalid());
                    }
                    (u64::from(be(&data, 0)), i64::from(be(&data, 4) as i32))
                }
                1 => {
                    let data = edits.read::<20>(&mut file, 8)?;
                    if be(&data, 16) != 0x0001_0000 {
                        return Err(invalid());
                    }
                    (
                        u64::from_be_bytes(data[..8].try_into().unwrap()),
                        i64::from_be_bytes(data[8..16].try_into().unwrap()),
                    )
                }
                _ => return Err(invalid()),
            };
            media_start = u64::try_from(origin).map_err(|_| invalid())?;
            if (u128::from(edited) * u128::from(scale))
                .abs_diff(u128::from(duration) * u128::from(movie_scale))
                >= u128::from(scale)
            {
                return Err(invalid());
            }
        }
        check_presentation(&mut file, composition, media_start, count, delta)?;
        frames = Some(count);
    }
    frames.ok_or_else(invalid)
}

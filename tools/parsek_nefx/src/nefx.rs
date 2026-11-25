use crate::fusion::{Confidence, Frame, render_rows};
use crate::lens::LensCorrection;
use crate::nef::{Ifd, Nef, WIDTH, slice};
use anyhow::{Context, Result, ensure};
use std::fs::File;
use std::io::{BufWriter, Read, Seek, SeekFrom, Write};
use std::path::Path;

const PREVIEW_WIDTH: usize = 640;
const PREVIEW_HEIGHT: usize = 424;
const PREVIEW_HEADER: usize = 16;
const PREVIEW_BYTES: usize = PREVIEW_HEADER + PREVIEW_WIDTH * PREVIEW_HEIGHT * 6;
const BAND_ROWS: usize = 32;

fn relocate(ifd: &mut Ifd, pointer: u16, start: usize, end: usize, growth: usize) -> Result<()> {
    ensure!(ifd.next == 0, "unsupported chained Nikon MakerNote IFD");
    for (&tag, field) in &mut ifd.fields {
        let size = field.size()?;
        if size > 4 || tag == pointer {
            let at = field.value as usize + 10;
            ensure!(
                at >= start && at + size <= end,
                "Nikon MakerNote offset outside its data"
            );
            field.value += growth as u32;
        }
    }
    Ok(())
}

fn output_note(nef: &mut Nef, width: usize, height: usize, mode: u8) -> Result<Vec<u8>> {
    let input = &nef.note;
    let ifd = &mut nef.note_ifd;
    let old_end = 18 + ifd.size();
    let preview_at = 10 + ifd.get(0x11)?.offset(4, 1)?;
    let mut preview = Ifd::read(input, preview_at)?;
    ensure!(
        !ifd.fields.contains_key(&0xe2d) && !ifd.fields.contains_key(&0xe2e),
        "expected a single exposure MakerNote"
    );
    ifd.fields.remove(&0x96); // The packed RGB strip has no NEF linearization table.
    let has_format = ifd.fields.contains_key(&0x51);
    let growth = (ifd.size() + 12 * (2 + usize::from(!has_format))) - (old_end - 18);
    relocate(ifd, 0x11, old_end, input.len(), growth)?;
    relocate(&mut preview, 0x201, old_end, input.len(), growth)?;

    let mut output = input[..18].to_vec();
    output.resize(old_end + growth, 0);
    output.extend_from_slice(&input[old_end..]);
    let preview_at = preview_at + growth;
    output[preview_at..preview_at + preview.size()].copy_from_slice(&preview.encode());
    let crop = 10 + ifd.get(0x45)?.offset(3, 4)?;
    output[crop..crop + 4].fill(0);
    output[crop + 4..crop + 6].copy_from_slice(&(width as u16).to_le_bytes());
    output[crop + 6..crop + 8].copy_from_slice(&(height as u16).to_le_bytes());
    ifd.set(0x93, 3, 1, 10);
    if !has_format {
        ifd.set(0x51, 7, 24, output.len() - 10);
        let mut format = [0; 24];
        format[..4].copy_from_slice(b"0102");
        output.extend_from_slice(&format);
    }
    let format = 10 + ifd.get(0x51)?.offset(7, 24)?;
    ensure!(
        matches!(slice(&output, format, 4)?, b"0101" | b"0102"),
        "unsupported Nikon RAW format block"
    );
    output[format + 10..format + 12].copy_from_slice(&10_u16.to_le_bytes());

    output.resize(output.len().next_multiple_of(2), 0);
    let result = output.len();
    ifd.set(0xe2d, 7, 16, result - 10);
    ifd.set(0xe2e, 7, PREVIEW_BYTES as u32, result + 16 - 10);
    output.extend_from_slice(&[
        b'0', b'1', b'0', b'0', 2, 0, mode, mode, 0, 0, 0, 0, 0, 0, 0, 0,
    ]);
    output[18..old_end + growth].copy_from_slice(&ifd.encode());
    Ok(output)
}

fn header(
    source: &Path,
    mut nef: Nef,
    mode: u8,
    width: usize,
    height: usize,
) -> Result<(Vec<u8>, usize)> {
    let mut bytes = vec![0; nef.strip];
    File::open(source)?.read_exact(&mut bytes)?;
    let note = output_note(&mut nef, width, height, mode)?;
    let note_at = bytes.len();
    let preview_at = note_at + note.len();
    nef.exif
        .set(0x927c, 7, (note.len() + PREVIEW_BYTES) as u32, note_at);
    bytes.extend_from_slice(&note);
    bytes.resize(preview_at + PREVIEW_BYTES, 0);

    let bits_at = bytes.len();
    bytes.extend_from_slice(&[14, 0, 14, 0, 14, 0]);
    if nef.root.fields.contains_key(&0x131) {
        let software = b"Parsek NEFX\0";
        nef.root.set(0x131, 2, software.len() as u32, bytes.len());
        bytes.extend_from_slice(software);
    }
    let raw_at = bytes.len().next_multiple_of(64);
    bytes.resize(raw_at, 0);
    for (tag, kind, count, value) in [
        (0x100, 4, 1, width),
        (0x101, 4, 1, height),
        (0x102, 3, 3, bits_at),
        (0x106, 3, 1, 2),
        (0x111, 4, 1, raw_at),
        (0x115, 3, 1, 3),
        (0x116, 4, 1, height),
        (0x117, 4, 1, width * height * 21 / 4),
    ] {
        nef.raw.set(tag, kind, count, value);
    }
    // Keep the reference JPEGs and other TIFF data at their original offsets.
    for (at, ifd) in [
        (nef.root_at, &nef.root),
        (nef.raw_at, &nef.raw),
        (nef.exif_at, &nef.exif),
    ] {
        for field in ifd.fields.values() {
            let size = field.size()?;
            if size > 4 {
                slice(&bytes, field.value as usize, size)?;
            }
        }
        bytes[at..at + ifd.size()].copy_from_slice(&ifd.encode());
    }
    Ok((bytes, preview_at))
}

pub struct Source<'a> {
    pub path: &'a Path,
    pub metadata: Nef,
    pub lens: Option<&'a LensCorrection>,
}

pub fn write(
    output: &Path,
    source: Source<'_>,
    mode: u8,
    frames: &[Frame],
    confidence: &Confidence,
    first_frame_motion: bool,
    mut report_rows: impl FnMut(usize),
) -> Result<()> {
    let (width, height) = crate::mode_dimensions(mode);
    let scale = width / WIDTH;
    let (mut header, preview_at) = header(source.path, source.metadata, mode, width, height)?;
    if let Some(lens) = source.lens {
        lens.update_profile(&mut header);
    }
    let parent = output
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."));
    let mut temp = tempfile::Builder::new()
        .prefix(".parsek-")
        .tempfile_in(parent)?;
    let mut file = BufWriter::with_capacity(1024 * 1024, temp.as_file_mut());
    file.write_all(&header)?;
    let mut preview = Vec::with_capacity(PREVIEW_BYTES);
    preview.extend_from_slice(b"0100");
    preview.extend_from_slice(&(PREVIEW_HEADER as u32).to_le_bytes());
    preview.extend_from_slice(&(PREVIEW_WIDTH as u16).to_le_bytes());
    preview.extend_from_slice(&(PREVIEW_HEIGHT as u16).to_le_bytes());
    preview.extend_from_slice(&[3, 14, 14, 14]);
    let mut preview_row = 0;
    let stride = width * 3;
    let margin = source.lens.map_or(0, |lens| lens.margin(height, scale));
    let mut raw = vec![0_u16; (BAND_ROWS + 2 * margin) * stride];
    let mut corrected = vec![0_u16; usize::from(source.lens.is_some()) * BAND_ROWS * stride];
    let (mut raw_first, mut raw_end) = (0, 0);
    let mut packed = Vec::with_capacity(BAND_ROWS * width * 21 / 4);
    for y in (0..height).step_by(BAND_ROWS) {
        let rows = BAND_ROWS.min(height - y);
        let first = y.saturating_sub(margin);
        let end = (y + rows + margin).min(height);
        // Keep the overlapping rows for the lens warp and only fuse new rows.
        raw.copy_within(
            (first - raw_first) * stride..(raw_end - raw_first) * stride,
            0,
        );
        render_rows(
            &mut raw[(raw_end - first) * stride..(end - first) * stride],
            raw_end,
            scale,
            frames,
            confidence,
            first_frame_motion,
        );
        (raw_first, raw_end) = (first, end);
        let band = if let Some(lens) = source.lens {
            let output = &mut corrected[..rows * stride];
            lens.correct(
                &raw[..(end - first) * stride],
                first,
                output,
                y,
                (width, height),
                scale,
            );
            &output[..]
        } else {
            &raw[..rows * stride]
        };
        while preview_row < PREVIEW_HEIGHT {
            let py = (2 * preview_row + 1) * height / (2 * PREVIEW_HEIGHT);
            if py >= y + rows {
                break;
            }
            for x in 0..PREVIEW_WIDTH {
                let px = (2 * x + 1) * width / (2 * PREVIEW_WIDTH);
                let start = ((py - y) * width + px) * 3;
                for sample in &band[start..start + 3] {
                    preview.extend_from_slice(&sample.to_le_bytes());
                }
            }
            preview_row += 1;
        }
        packed.clear();
        for values in band.chunks_exact(4) {
            let word = values.iter().enumerate().fold(0_u64, |word, (i, &value)| {
                word | (u64::from(value) << (14 * i))
            });
            packed.extend_from_slice(&word.to_le_bytes()[..7]);
        }
        file.write_all(&packed)?;
        if y.is_multiple_of(1024) || y + rows == height {
            report_rows(y + rows);
        }
    }
    ensure!(preview.len() == PREVIEW_BYTES, "incomplete NEFX preview");
    file.seek(SeekFrom::Start(preview_at as u64))?;
    file.write_all(&preview)?;
    file.flush()?;
    drop(file);
    temp.persist_noclobber(output)
        .with_context(|| format!("cannot save {}", output.display()))?;
    Ok(())
}

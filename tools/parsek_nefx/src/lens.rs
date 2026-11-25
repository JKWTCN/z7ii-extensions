use crate::nef::{Ifd, LEFT, Nef, RAW_HEIGHT, RAW_WIDTH, TOP, le32, read_at, slice};
use anyhow::{Result, ensure};
use rayon::prelude::*;
use std::fs::File;
use std::path::Path;

pub struct LensCorrection {
    coefficients: [[f32; 3]; 2],
    profile_at: usize,
    profile: Vec<u8>,
}

impl LensCorrection {
    pub fn read(path: &Path, nef: &Nef) -> Result<Option<Self>> {
        let Some(field) = nef.raw.fields.get(&0xc7d5) else {
            return Ok(None);
        };
        let (profile_at, size) = (field.value as usize, field.count as usize);
        ensure!(
            field.kind == 7 && size >= 24 && profile_at >= 8 && profile_at + size <= nef.strip,
            "Invalid Nikon lens profile field"
        );
        let mut profile = read_at(&mut File::open(path)?, profile_at, size)?;
        ensure!(
            profile.starts_with(b"Nikon\0\x01\x01\0\0II*\0"),
            "Unsupported Nikon lens profile header"
        );
        let directory = 10 + le32(&profile[14..18]) as usize;
        ensure!(directory >= 18, "Invalid Nikon lens profile directory");
        let mut ifd = Ifd::read(&profile, directory)?;
        let Some(field) = ifd.fields.remove(&7) else {
            return Ok(None);
        };
        let old_end = directory + ifd.size() + 12;
        let at = 10 + field.value as usize;
        ensure!(
            field.kind == 7 && field.count >= 88 && at >= old_end,
            "Invalid lateral CA profile field"
        );
        let data = slice(&profile, at, field.count as usize)?;
        ensure!(
            data.starts_with(b"0100"),
            "Unsupported lateral CA profile version"
        );

        // Match the camera merge's Nikon lens profile. Two polynomials map
        // corrected red and blue positions back into the uncorrected image.
        let mut coefficients = [[0.0; 3]; 2];
        for (dest, offsets) in coefficients.iter_mut().zip([[44, 28, 20], [80, 64, 56]]) {
            for (term, offset) in dest.iter_mut().zip(offsets) {
                let numerator = le32(&data[offset..offset + 4]) as i32;
                let denominator = le32(&data[offset + 4..offset + 8]);
                ensure!(
                    (-16384..=16384).contains(&numerator) && denominator == 1 << 20,
                    "Unsupported lateral CA coefficient"
                );
                *term = numerator as f32 / denominator as f32;
            }
        }
        if coefficients.iter().flatten().all(|&term| term == 0.0) {
            return Ok(None);
        }
        // Other lens adjustments still use their original values and offsets.
        let new_end = directory + ifd.size();
        profile[directory..new_end].copy_from_slice(&ifd.encode());
        profile[new_end..old_end].fill(0);
        Ok(Some(Self {
            coefficients,
            profile_at,
            profile,
        }))
    }

    pub fn update_profile(&self, prefix: &mut [u8]) {
        prefix[self.profile_at..self.profile_at + self.profile.len()]
            .copy_from_slice(&self.profile);
    }

    pub fn margin(&self, height: usize, scale: usize) -> usize {
        let center = (0.5 * (RAW_HEIGHT - 1) as f32 - TOP as f32) * scale as f32;
        let farthest = center.max((height - 1) as f32 - center);
        let max_factor = self
            .coefficients
            .iter()
            .map(|terms| terms.iter().map(|term| term.abs()).sum::<f32>())
            .fold(0.0, f32::max);
        // r² is at most 1 within the sensor. One additional row covers interpolation.
        (farthest * max_factor).ceil() as usize + 1
    }

    pub fn correct(
        &self,
        raw: &[u16],
        first_row: usize,
        output: &mut [u16],
        start_y: usize,
        (width, height): (usize, usize),
        scale: usize,
    ) {
        let scale = scale as f32;
        let center_x = (0.5 * (RAW_WIDTH - 1) as f32 - LEFT as f32) * scale;
        let center_y = (0.5 * (RAW_HEIGHT - 1) as f32 - TOP as f32) * scale;
        let radius2 = ((RAW_WIDTH as f32 * scale * 0.5).powi(2)
            + (RAW_HEIGHT as f32 * scale * 0.5).powi(2))
        .recip();
        let stride = width * 3;
        output
            .par_chunks_exact_mut(stride)
            .enumerate()
            .for_each(|(index, row)| {
                let y = start_y + index;
                let dy = y as f32 - center_y;
                let y_radius2 = dy * dy * radius2;
                for (x, pixel) in row.chunks_exact_mut(3).enumerate() {
                    let dx = x as f32 - center_x;
                    let r2 = dx * dx * radius2 + y_radius2;
                    pixel[1] = raw[(y - first_row) * stride + x * 3 + 1];
                    for (channel, [a, b, c]) in [0, 2].into_iter().zip(self.coefficients) {
                        let factor = a + r2 * (b + r2 * c);
                        let sx = (x as f32 + dx * factor).clamp(0.0, (width - 1) as f32);
                        let sy = (y as f32 + dy * factor).clamp(0.0, (height - 1) as f32);
                        let (left, right) = (sx as usize, sx.ceil() as usize);
                        let (top, bottom) =
                            (sy as usize - first_row, sy.ceil() as usize - first_row);
                        let (fx, fy) = (sx.fract(), sy.fract());
                        let blend_row = |row: usize| {
                            let a = raw[row * stride + left * 3 + channel] as f32;
                            let b = raw[row * stride + right * 3 + channel] as f32;
                            a + fx * (b - a)
                        };
                        let a = blend_row(top);
                        pixel[channel] = (a + fy * (blend_row(bottom) - a)).round() as u16;
                    }
                }
            });
    }
}

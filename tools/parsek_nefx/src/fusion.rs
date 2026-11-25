use crate::align::Warp;
use crate::nef::{CFA, HEIGHT, LEFT, RAW_HEIGHT, RAW_WIDTH, TOP, WIDTH};
use rayon::prelude::*;

const MAX_FRAMES: usize = 32;
const BINS: usize = 256;

pub struct Frame {
    pub samples: Vec<u16>,
    pub warp: Warp,
}

#[derive(Clone, Copy, Default)]
struct Sample {
    value: u16,
    distance: f32,
}

impl Frame {
    fn nearest(&self, x: f32, y: f32) -> Option<[Sample; 3]> {
        let warp = &self.warp;
        let fx = warp.cos * x - warp.sin * y + warp.x;
        let fy = warp.sin * x + warp.cos * y + warp.y;
        if !(0.0..(RAW_WIDTH - 1) as f32).contains(&fx)
            || !(0.0..(RAW_HEIGHT - 1) as f32).contains(&fy)
        {
            return None;
        }
        let (ix, iy) = (fx as usize, fy as usize);
        let (dx, dy) = (fx - ix as f32, fy - iy as f32);
        let xdist = [dx * dx, (1.0 - dx) * (1.0 - dx)];
        let ydist = [dy * dy, (1.0 - dy) * (1.0 - dy)];
        let mut result = [Sample {
            value: 0,
            distance: f32::INFINITY,
        }; 3];
        let at = iy * RAW_WIDTH + ix;
        for (row, &yd) in ydist.iter().enumerate() {
            for (col, &xd) in xdist.iter().enumerate() {
                let c = CFA[((iy + row) & 1) * 2 + ((ix + col) & 1)];
                let distance = xd + yd;
                if distance < result[c].distance {
                    result[c] = Sample {
                        value: self.samples[at + row * RAW_WIDTH + col],
                        distance,
                    };
                }
            }
        }
        Some(result)
    }
}

fn bin(value: u16) -> usize {
    (value as usize >> 6).min(BINS - 1)
}

pub struct Confidence(Vec<f32>);

impl Confidence {
    pub fn learn(frames: &[Frame]) -> Self {
        let mut endpoints = vec![0_i32; 3 * BINS * (BINS + 1)];
        for y in (TOP..TOP + HEIGHT).step_by(9) {
            for x in (LEFT..LEFT + WIDTH).step_by(9) {
                let channel = CFA[(y & 1) * 2 + (x & 1)];
                let mut bins = [0; MAX_FRAMES];
                let mut count = 0;
                for frame in frames {
                    if let Some(rgb) = frame.nearest(x as f32, y as f32) {
                        bins[count] = bin(rgb[channel].value);
                        count += 1;
                    }
                }
                for (i, &a) in bins[..count].iter().enumerate() {
                    for &b in &bins[..i] {
                        let (low, high) = (a.min(b), a.max(b));
                        // Mark the two edges of the box using range updates.
                        for column in [low, high] {
                            let offset = (channel * BINS + column) * (BINS + 1);
                            endpoints[offset + low] += 1;
                            endpoints[offset + high + 1] -= 1;
                        }
                    }
                }
            }
        }

        let mut histogram = vec![0.0; 3 * BINS * BINS];
        for c in 0..3 {
            for column in 0..BINS {
                let mut runs = 0;
                let start = (c * BINS + column) * (BINS + 1);
                for (row, &delta) in endpoints[start..start + BINS].iter().enumerate() {
                    runs += delta;
                    histogram[(c * BINS + row) * BINS + column] = runs as f32;
                }
            }
        }
        for (i, row) in histogram.chunks_exact_mut(BINS).enumerate() {
            let center = i % BINS;
            let (lower, upper) = row.split_at_mut(center + 1);
            let diagonal = lower[center];
            let mut total = 0.0;
            for weight in &mut lower[..center] {
                total += *weight;
                *weight = total;
            }
            let mut total_right = 0.0;
            for weight in upper.iter_mut().rev() {
                total_right += *weight;
                *weight = total_right;
            }
            let normalizer = (total + total_right + diagonal).max(1.0);
            for weight in row.iter_mut() {
                let confidence = *weight / normalizer;
                *weight = 0.01 + 0.99 * confidence * confidence;
            }
            row[center] = 1.0;
        }
        Self(histogram)
    }

    fn weight(&self, channel: usize, sample: u16, reference: u16) -> f32 {
        self.0[(channel * BINS + bin(sample)) * BINS + bin(reference)]
    }
}

pub fn render_rows(
    rows: &mut [u16],
    start_y: usize,
    scale: usize,
    frames: &[Frame],
    confidence: &Confidence,
    first_frame_motion: bool,
) {
    let inv_scale = 1.0 / scale as f32;
    let distance_limit = (0.5 * inv_scale).powi(2);
    rows.par_chunks_exact_mut(WIDTH * scale * 3)
        .enumerate()
        .for_each(|(dy, row)| {
            let y = TOP as f32 + (start_y + dy) as f32 * inv_scale;
            for (x, pixel) in row.chunks_exact_mut(3).enumerate() {
                let x = LEFT as f32 + x as f32 * inv_scale;
                let mut values = [[Sample::default(); MAX_FRAMES]; 3];
                let mut count = 0;
                for frame in frames {
                    if let Some(rgb) = frame.nearest(x, y) {
                        for (samples, sample) in values.iter_mut().zip(rgb) {
                            samples[count] = sample;
                        }
                        count += 1;
                    }
                }
                for (channel, (dest, samples)) in pixel.iter_mut().zip(&mut values).enumerate() {
                    // The identity-warp reference always covers the output crop.
                    let samples = &mut samples[..count];
                    let reference = if first_frame_motion {
                        samples[0].value
                    } else {
                        let middle = count / 2;
                        let (lower, upper_middle, _) =
                            samples.select_nth_unstable_by_key(middle, |s| s.value);
                        if count.is_multiple_of(2) {
                            let lower_middle = lower.iter().map(|s| s.value).max().unwrap();
                            lower_middle.midpoint(upper_middle.value)
                        } else {
                            upper_middle.value
                        }
                    };
                    let (mut sum, mut weights) = (0.0, 0.0);
                    for sample in samples {
                        let geometry = (distance_limit / sample.distance).min(1.0);
                        let weight = geometry * confidence.weight(channel, sample.value, reference);
                        sum += weight * sample.value as f32;
                        weights += weight;
                    }
                    *dest = (sum / weights).round().clamp(0.0, 16383.0) as u16;
                }
            }
        });
}

use crate::nef::{RAW_HEIGHT, RAW_WIDTH};
use rayon::prelude::*;

#[derive(Clone, Copy, Default)]
pub struct Transform {
    pub dx: f64,
    pub dy: f64,
    pub angle: f64,
}

const RADIUS: f64 = RAW_WIDTH as f64 / 2.0;

pub struct Warp {
    pub cos: f32,
    pub sin: f32,
    pub x: f32,
    pub y: f32,
}

impl Transform {
    pub fn warp(self) -> Warp {
        let (sin, cos) = self.angle.sin_cos();
        let cx = (RAW_WIDTH - 1) as f64 / 2.0;
        let cy = (RAW_HEIGHT - 1) as f64 / 2.0;
        Warp {
            cos: cos as f32,
            sin: sin as f32,
            x: (cx - cos * cx + sin * cy + self.dx) as f32,
            y: (cy - sin * cx - cos * cy + self.dy) as f32,
        }
    }
}

struct Gray {
    width: usize,
    height: usize,
    values: Vec<f32>,
}

pub struct Pyramid(Vec<Gray>);

impl Pyramid {
    pub fn new(raw: &[u16]) -> Self {
        let (width, height) = (RAW_WIDTH / 2, RAW_HEIGHT / 2);
        // Average each Bayer cell before alignment to suppress CFA periodicity.
        let values = (0..height)
            .into_par_iter()
            .flat_map_iter(|y| {
                (0..width).map(move |x| {
                    let i = 2 * y * RAW_WIDTH + 2 * x;
                    let sum = raw[i] as f32
                        + raw[i + 1] as f32
                        + raw[i + RAW_WIDTH] as f32
                        + raw[i + RAW_WIDTH + 1] as f32;
                    sum / (4.0 * 16384.0)
                })
            })
            .collect();
        let mut levels = vec![Gray {
            width,
            height,
            values,
        }];
        for _ in 0..3 {
            let prev = levels.last().unwrap();
            let (width, height) = (prev.width / 2, prev.height / 2);
            let values = (0..height)
                .into_par_iter()
                .flat_map_iter(|y| {
                    (0..width).map(move |x| {
                        let i = 2 * y * prev.width + 2 * x;
                        (prev.values[i]
                            + prev.values[i + 1]
                            + prev.values[i + prev.width]
                            + prev.values[i + prev.width + 1])
                            * 0.25
                    })
                })
                .collect();
            levels.push(Gray {
                width,
                height,
                values,
            });
        }
        Self(levels)
    }
}

impl Gray {
    fn sample(&self, x: f64, y: f64) -> (f64, f64, f64) {
        let i = y as usize * self.width + x as usize;
        let (fx, fy) = (x.fract(), y.fract());
        let (a, b) = (self.values[i] as f64, self.values[i + 1] as f64);
        let (c, d) = (
            self.values[i + self.width] as f64,
            self.values[i + self.width + 1] as f64,
        );
        let top = a + fx * (b - a);
        let bottom = c + fx * (d - c);
        (
            top + fy * (bottom - top),
            (1.0 - fy) * (b - a) + fy * (d - c),
            bottom - top,
        )
    }
}

struct Statistics {
    covariance: [[f64; 5]; 5],
}

impl Statistics {
    fn correlation(&self) -> f64 {
        let c = &self.covariance;
        let norm = (c[0][0] * c[1][1]).sqrt();
        if norm > 1e-10 { c[0][1] / norm } else { -1.0 }
    }

    fn correction(&self) -> Option<[f64; 3]> {
        let c = &self.covariance;
        let variance = c[1][1];
        if variance <= 1e-10 || c[0][0] <= 1e-10 {
            return None;
        }
        // Project the image gradients away from intensity and offset. The
        // remaining least-squares step maximizes normalized correlation.
        let gain = (variance / c[0][0]).sqrt();
        let mut system = [[0.0; 4]; 3];
        for (i, row) in system.iter_mut().enumerate() {
            for (j, dst) in row[..3].iter_mut().enumerate() {
                *dst = c[i + 2][j + 2] - c[i + 2][1] * c[j + 2][1] / variance;
            }
            row[3] = gain * (c[i + 2][0] - c[i + 2][1] * c[0][1] / variance);
        }
        solve(system)
    }
}

fn solve(mut matrix: [[f64; 4]; 3]) -> Option<[f64; 3]> {
    for col in 0..3 {
        let pivot =
            (col..3).max_by(|&a, &b| matrix[a][col].abs().total_cmp(&matrix[b][col].abs()))?;
        matrix.swap(col, pivot);
        let divisor = matrix[col][col];
        if divisor.abs() < 1e-12 {
            return None;
        }
        for entry in &mut matrix[col][col..] {
            *entry /= divisor;
        }
        for row in 0..3 {
            if row != col {
                let factor = matrix[row][col];
                for j in col..4 {
                    matrix[row][j] -= factor * matrix[col][j];
                }
            }
        }
    }
    Some([matrix[0][3], matrix[1][3], matrix[2][3]])
}

fn measure(reference: &Gray, image: &Gray, scale: f64, warp: Transform) -> Statistics {
    let (sin, cos) = warp.angle.sin_cos();
    let (cx, cy) = (
        (reference.width - 1) as f64 * 0.5,
        (reference.height - 1) as f64 * 0.5,
    );
    let (dx, dy) = (warp.dx / scale, warp.dy / scale);
    let (border, step) = ((112.0 / scale) as usize, (48.0 / scale) as usize);
    let mut count = 0.0;
    let mut sums = [0.0; 5];
    let mut products = [[0.0; 5]; 5];
    for y in (border..reference.height - border).step_by(step) {
        for x in (border..reference.width - border).step_by(step) {
            let (px, py) = (x as f64 - cx, y as f64 - cy);
            let (rx, ry) = (cos * px - sin * py, sin * px + cos * py);
            let (sx, sy) = (rx + cx + dx, ry + cy + dy);
            let (value, gx, gy) = image.sample(sx, sy);
            let values = [
                reference.values[y * reference.width + x] as f64,
                value,
                gx / scale,
                gy / scale,
                (-gx * ry + gy * rx) / RADIUS,
            ];
            count += 1.0;
            for (i, &a) in values.iter().enumerate() {
                sums[i] += a;
                for j in 0..=i {
                    products[i][j] += a * values[j];
                }
            }
        }
    }
    for i in 0..5 {
        for j in 0..=i {
            let centered = products[i][j] - sums[i] * sums[j] / count;
            products[i][j] = centered;
            products[j][i] = centered;
        }
    }
    Statistics {
        covariance: products,
    }
}

pub fn register(reference: &Pyramid, image: &Pyramid) -> (Transform, f64) {
    let mut warp = Transform::default();
    let mut score = measure(&reference.0[3], &image.0[3], 16.0, warp).correlation();
    for y in -4..=4 {
        for x in -4..=4 {
            let candidate = Transform {
                dx: f64::from(x) * 16.0,
                dy: f64::from(y) * 16.0,
                angle: 0.0,
            };
            let corr = measure(&reference.0[3], &image.0[3], 16.0, candidate).correlation();
            if corr > score {
                (warp, score) = (candidate, corr);
            }
        }
    }
    for level in (0..4).rev() {
        let (reference, image) = (&reference.0[level], &image.0[level]);
        let scale = (2 << level) as f64;
        let mut stats = measure(reference, image, scale, warp);
        for _ in 0..20 {
            let Some([dx, dy, angle_at_edge]) = stats.correction() else {
                break;
            };
            let step = dx.abs().max(dy.abs()).max(angle_at_edge.abs());
            if !step.is_finite() || step < 0.005 {
                break;
            }
            let limit = (scale / step).min(1.0);
            let improved = [1.0, 0.5, 0.25, 0.125].into_iter().find_map(|fraction| {
                let gain = fraction * limit;
                let candidate = Transform {
                    dx: (warp.dx + dx * gain).clamp(-80.0, 80.0),
                    dy: (warp.dy + dy * gain).clamp(-80.0, 80.0),
                    angle: (warp.angle + angle_at_edge * gain / RADIUS).clamp(-0.005, 0.005),
                };
                let new_stats = measure(reference, image, scale, candidate);
                (new_stats.correlation() > stats.correlation() + 1e-9)
                    .then_some((candidate, new_stats))
            });
            let Some((best, new_stats)) = improved else {
                break;
            };
            (warp, stats) = (best, new_stats);
        }
        score = stats.correlation();
    }
    (warp, score)
}

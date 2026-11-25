mod align;
mod fusion;
mod lens;
mod nef;
mod nefx;

use align::{Pyramid, Transform};
use anyhow::{Context, Result, ensure};
use fusion::{Confidence, Frame};
use nef::{HEIGHT, Nef, WIDTH};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

#[derive(Clone, Copy)]
pub enum Language {
    English,
    Chinese,
}

impl Language {
    pub fn text<'a>(self, english: &'a str, chinese: &'a str) -> &'a str {
        match self {
            Self::English => english,
            Self::Chinese => chinese,
        }
    }
}

pub enum Update {
    Status(u32, String),
    Log(String),
}

#[derive(Default)]
pub struct Options {
    pub reference_motion: bool,
    pub correct_ca: bool,
}

pub fn extension_is(path: &Path, extension: &str) -> bool {
    path.extension()
        .is_some_and(|ext| ext.eq_ignore_ascii_case(extension))
}

fn mode(count: usize) -> Option<u8> {
    match count {
        4 => Some(1),
        8 => Some(2),
        16 => Some(3),
        32 => Some(4),
        _ => None,
    }
}

pub fn dimensions(count: usize) -> Option<(usize, usize)> {
    mode(count).map(mode_dimensions)
}

fn mode_dimensions(mode: u8) -> (usize, usize) {
    let scale = if mode <= 2 { 1 } else { 2 };
    (WIDTH * scale, HEIGHT * scale)
}

pub fn can_calculate(count: usize) -> bool {
    (2..=32).contains(&count)
}

pub fn calculate_alignment(
    files: &[PathBuf],
    language: Language,
    mut report: impl FnMut(Update),
) -> Result<()> {
    let total = files.len();
    ensure!(
        can_calculate(total),
        "{} ({total})",
        language.text(
            "Select 2 to 32 NEF files. Received",
            "请选择 2 至 32 张 NEF。当前张数"
        )
    );
    let (_, accepted) = align_frames(files, language, 100, &mut report, drop)?;
    report(Update::Log(match language {
        Language::English => format!("Aligned {accepted}/{total} frames"),
        Language::Chinese => format!("配准完成，有效原片 {accepted}/{total} 张"),
    }));
    Ok(())
}

pub fn merge(
    files: &[PathBuf],
    output: &Path,
    options: Options,
    language: Language,
    mut report: impl FnMut(Update),
) -> Result<()> {
    let t = |english: &'static str, chinese: &'static str| language.text(english, chinese);
    let total = files.len();
    let mode = mode(total).with_context(|| {
        format!(
            "{} ({total})",
            t(
                "Select 4, 8, 16 or 32 NEF files. Received",
                "请选择 4、8、16 或 32 张 NEF。当前张数"
            )
        )
    })?;
    ensure!(
        extension_is(output, "nefx"),
        "{}",
        t("Output path must end in .NEFX", "输出文件名须以 .NEFX 结尾")
    );
    ensure!(
        !output.try_exists().with_context(|| format!(
            "{} {}",
            t("Cannot access output path", "无法访问输出路径"),
            output.display()
        ))?,
        "{} {}",
        t("Output already exists", "输出文件已存在"),
        output.display()
    );
    let mut frames = Vec::with_capacity(total);
    let (reference, _) =
        align_frames(files, language, 30, &mut report, |frame| frames.push(frame))?;
    let source = &files[0];
    let lens = if options.correct_ca {
        match lens::LensCorrection::read(source, &reference) {
            Ok(Some(lens)) => Some(lens),
            Ok(None) => {
                report(Update::Log(
                    t(
                        "No applicable lens profile for lateral chromatic aberration correction",
                        "镜头无可用的横向色差参数，已跳过矫正",
                    )
                    .into(),
                ));
                None
            }
            Err(error) => {
                report(Update::Log(format!(
                    "{} ({error:#})",
                    t(
                        "Skipped lateral chromatic aberration correction because the lens profile could not be used",
                        "镜头色差参数无法使用，已跳过矫正",
                    )
                )));
                None
            }
        }
    } else {
        None
    };
    report(Update::Status(
        30,
        format!(
            "{} {}/{total}",
            t(
                "Learning pixel confidence from",
                "正在估计像素置信度，有效原片"
            ),
            frames.len()
        ),
    ));
    let confidence = Confidence::learn(&frames);
    let (width, height) = mode_dimensions(mode);
    nefx::write(
        output,
        nefx::Source {
            path: source,
            metadata: reference,
            lens: lens.as_ref(),
        },
        mode,
        &frames,
        &confidence,
        options.reference_motion,
        |rows| {
            report(Update::Status(
                (40 + 59 * rows / height) as u32,
                match language {
                    Language::English => format!("Fusing {rows}/{height} rows"),
                    Language::Chinese => format!("正在融合 {rows}/{height} 行"),
                },
            ));
        },
    )
    .with_context(|| {
        format!(
            "{} {}",
            t("Cannot write NEFX", "无法写入 NEFX"),
            output.display()
        )
    })?;
    if lens.is_some() {
        report(Update::Log(
            t("Corrected lateral chromatic aberration", "已矫正横向色差").into(),
        ));
    }
    report(Update::Log(format!(
        "{} {} ({width} × {height})",
        t("Saved", "已保存"),
        output.display()
    )));
    Ok(())
}

fn align_frames(
    files: &[PathBuf],
    language: Language,
    progress_end: usize,
    report: &mut impl FnMut(Update),
    mut accept: impl FnMut(Frame),
) -> Result<(Nef, usize)> {
    let t = |english: &'static str, chinese: &'static str| language.text(english, chinese);
    let total = files.len();
    let mut paths = HashSet::new();
    for path in files {
        ensure!(
            extension_is(path, "nef"),
            "{} {}",
            t("Expected a .NEF file", "输入文件须为 .NEF"),
            path.display()
        );
        let canonical = path
            .canonicalize()
            .with_context(|| format!("{} {}", t("Cannot open", "无法打开"), path.display()))?;
        ensure!(
            paths.insert(canonical),
            "{} {}",
            t("Duplicate input", "输入文件重复"),
            path.display()
        );
    }

    let source = &files[0];
    let read = |path: &Path| {
        Nef::read(path).with_context(|| {
            format!(
                "{} {}",
                t("Cannot read NEF metadata for", "无法读取 NEF 元数据"),
                path.display()
            )
        })
    };
    let decode = |path: &Path| {
        nef::decode(path).with_context(|| {
            format!(
                "{} {}",
                t("Cannot decode NEF", "无法解码 NEF"),
                path.display()
            )
        })
    };
    let reading = |index: usize, path: &Path| {
        let name = path
            .file_name()
            .unwrap_or(path.as_os_str())
            .to_string_lossy();
        Update::Status(
            (progress_end * index / total) as u32,
            format!("{} {}/{total}  {name}", t("Reading", "正在读取"), index + 1),
        )
    };
    report(reading(0, source));
    let reference = read(source)?;
    let samples = decode(source)?;
    let pyramid = Pyramid::new(&samples);
    let mut accepted = 1;
    accept(Frame {
        samples,
        warp: Transform::default().warp(),
    });
    report(Update::Log(format!(
        "{} {}",
        t("Reference", "参考原片"),
        source.display()
    )));
    for (index, path) in files.iter().enumerate().skip(1) {
        let name = path
            .file_name()
            .unwrap_or(path.as_os_str())
            .to_string_lossy();
        report(reading(index, path));
        let current = read(path)?;
        ensure!(
            reference.orientation == current.orientation,
            "{} {name}",
            t("Camera orientation differs in", "相机方向不一致")
        );
        ensure!(
            reference.exposure == current.exposure,
            "{} {name}",
            t(
                "Shutter speed, aperture or ISO differs in",
                "快门速度、光圈或 ISO 不一致"
            )
        );
        let samples = decode(path)?;
        let (transform, score) = align::register(&pyramid, &Pyramid::new(&samples));
        if score < 0.85
            || transform.dx.abs() >= 79.99
            || transform.dy.abs() >= 79.99
            || transform.angle.abs() >= 0.00499
        {
            report(Update::Log(format!(
                "{} {name}  {} {score:.4}",
                t("Skipped", "已跳过"),
                t("unreliable alignment, correlation", "配准不可靠，相关系数")
            )));
            continue;
        }
        report(Update::Log(format!(
            "{name}  X {:+.3} px, Y {:+.3} px, {} {:+.4}°, {} {score:.4}",
            transform.dx,
            transform.dy,
            t("roll", "旋转"),
            transform.angle.to_degrees(),
            t("correlation", "相关系数")
        )));
        accepted += 1;
        accept(Frame {
            samples,
            warp: transform.warp(),
        });
    }
    ensure!(
        accepted >= 2,
        "{}",
        t(
            "Fewer than two frames could be aligned",
            "成功配准的原片少于两张"
        )
    );
    Ok((reference, accepted))
}

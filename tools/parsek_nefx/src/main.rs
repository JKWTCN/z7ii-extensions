use anyhow::{Context, Result, bail, ensure};
use parsek_nefx::{Language, Options, Update, merge};
use std::env;
use std::path::PathBuf;
use std::process::ExitCode;

const USAGE: &str =
    "Usage: parsek-nefx -o OUTPUT.NEFX [--reference-motion] [--correct-ca] FRAME1.NEF FRAME2.NEF ...

Merge 4, 8, 16 or 32 full-size Nikon Z7 II 14-bit NEFs of the same scene.
Inputs may be uncompressed or lossless compressed. The first frame anchors the result.
4/8 frames produce 8256 × 5504. 16/32 frames produce 16512 × 11008.
--reference-motion   Favor raw values from the first frame in moving areas.
--correct-ca         Correct lateral chromatic aberration using the lens profile.";

fn run() -> Result<()> {
    let mut args = env::args_os().skip(1);
    let mut files = Vec::new();
    let mut output = None;
    let mut options = Options::default();
    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("-h" | "--help") => {
                println!("{USAGE}");
                return Ok(());
            }
            Some("-o" | "--output") => {
                ensure!(output.is_none(), "specify the output only once");
                output = Some(PathBuf::from(args.next().context("missing output path")?));
            }
            Some("--reference-motion") => options.reference_motion = true,
            Some("--correct-ca") => options.correct_ca = true,
            Some("--") => {
                files.extend(args.map(PathBuf::from));
                break;
            }
            Some(value) if value.starts_with('-') => bail!("unknown option {value}"),
            _ => files.push(PathBuf::from(arg)),
        }
    }
    let output = output.context("specify -o OUTPUT.NEFX")?;
    merge(
        &files,
        &output,
        options,
        Language::English,
        |update| match update {
            Update::Status(_, message) | Update::Log(message) => eprintln!("{message}"),
        },
    )
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("Error: {error:#}");
            ExitCode::FAILURE
        }
    }
}

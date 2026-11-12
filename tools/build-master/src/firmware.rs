use crate::patch;
use sha2::{Digest, Sha256};
use std::{
    fs, io,
    io::Write,
    path::{Path, PathBuf},
};
use tempfile::NamedTempFile;

#[derive(Debug)]
pub enum BuildError {
    Io(io::Error),
    InvalidInput,
    InvalidContainer,
    InvalidMaster,
    InvalidPackage,
    OutputExists(PathBuf),
}

impl From<io::Error> for BuildError {
    fn from(error: io::Error) -> Self {
        Self::Io(error)
    }
}

#[derive(Clone, Copy)]
pub enum Stage {
    Reading,
    Checking,
    Patching,
    Packing,
    Verifying,
    Writing,
}

pub struct BuildReport {
    pub output: PathBuf,
    pub manifest: PathBuf,
    pub sha256: String,
}

struct Member<'a> {
    name: &'a [u8],
    bytes: &'a [u8],
}

fn sha256(data: &[u8]) -> String {
    format!("{:x}", Sha256::digest(data))
}

const fn crc_table() -> [u16; 256] {
    let mut table = [0; 256];
    let mut i = 0;
    while i < 256 {
        let mut crc = (i as u16) << 8;
        let mut bit = 0;
        while bit < 8 {
            crc = if crc & 0x8000 != 0 {
                (crc << 1) ^ 0x1021
            } else {
                crc << 1
            };
            bit += 1;
        }
        table[i] = crc;
        i += 1;
    }
    table
}

fn crc16(data: &[u8]) -> u16 {
    const TABLE: [u16; 256] = crc_table();
    data.iter().fold(0u16, |crc, byte| {
        (crc << 8) ^ TABLE[((crc >> 8) as u8 ^ byte) as usize]
    })
}

fn transform(payload: &[u8]) -> Vec<u8> {
    payload
        .iter()
        .enumerate()
        .map(|(i, byte)| {
            let position = i + 32;
            byte ^ patch::ENCODING_TABLES[position & 255]
                ^ patch::ENCODING_TABLES[256 + ((position >> 8) & 255)]
                ^ patch::ENCODING_TABLES[512 + ((position >> 16) & 255)]
        })
        .collect()
}

fn decode(package: &[u8]) -> Vec<u8> {
    let mut body = vec![b' '; 32];
    body.extend(transform(&package[64..]));
    body
}

fn word(bytes: &[u8], offset: usize) -> Result<usize, BuildError> {
    let value: [u8; 4] = bytes
        .get(offset..offset + 4)
        .ok_or(BuildError::InvalidContainer)?
        .try_into()
        .unwrap();
    Ok(u32::from_be_bytes(value) as usize)
}

fn members(body: &[u8]) -> Result<Vec<Member<'_>>, BuildError> {
    if body.len() < 48 || body[..32] != [b' '; 32] {
        return Err(BuildError::InvalidContainer);
    }
    let count = word(body, 32)?;
    let mut offset = word(body, 36)?;
    if !(1..=64).contains(&count)
        || offset != 48 + count * 32
        || word(body, 40)? != 0
        || word(body, 44)? != 0
        || offset > body.len()
    {
        return Err(BuildError::InvalidContainer);
    }
    let mut result = Vec::with_capacity(count);
    for index in 0..count {
        let row = 48 + index * 32;
        let start = word(body, row + 16)?;
        let size = word(body, row + 20)?;
        if start != offset
            || size < 2
            || word(body, row + 24)? != 0
            || word(body, row + 28)? != 0
            || size > body.len().saturating_sub(start + 16)
        {
            return Err(BuildError::InvalidContainer);
        }
        let bytes = body
            .get(start..start + size)
            .ok_or(BuildError::InvalidContainer)?;
        if crc16(bytes) != 0 {
            return Err(BuildError::InvalidContainer);
        }
        result.push(Member {
            name: &body[row..row + 16],
            bytes,
        });
        offset += size;
    }
    if body.len() != offset + 16
        || body[offset + 2..].iter().any(|byte| *byte != 0)
        || crc16(&body[..offset + 2]) != 0
    {
        return Err(BuildError::InvalidContainer);
    }
    Ok(result)
}

fn patched_master(stock: &[u8]) -> Result<Vec<u8>, BuildError> {
    if sha256(stock) != patch::STOCK_SHA {
        return Err(BuildError::InvalidMaster);
    }
    let mut master = stock[..stock.len() - 2].to_vec();
    for &(offset, replacement) in patch::PATCHES {
        let target = master
            .get_mut(offset..offset + replacement.len())
            .ok_or(BuildError::InvalidMaster)?;
        target.copy_from_slice(replacement);
    }
    master.extend_from_slice(patch::HOST_CODE);
    master.extend(crc16(&master).to_be_bytes());
    if sha256(&master) != patch::VERIFIED_MASTER_SHA {
        return Err(BuildError::InvalidMaster);
    }
    Ok(master)
}

fn pack(original: &[Member<'_>], target: usize, master: &[u8]) -> Vec<u8> {
    let mut body = vec![b' '; 32];
    let mut offset = 48 + original.len() * 32;
    for word in [original.len(), offset, 0, 0] {
        body.extend((word as u32).to_be_bytes());
    }
    for (index, member) in original.iter().enumerate() {
        let bytes = if index == target {
            master
        } else {
            member.bytes
        };
        body.extend_from_slice(member.name);
        for word in [offset, bytes.len(), 0, 0] {
            body.extend((word as u32).to_be_bytes());
        }
        offset += bytes.len();
    }
    for (index, member) in original.iter().enumerate() {
        body.extend_from_slice(if index == target {
            master
        } else {
            member.bytes
        });
    }
    body.extend(crc16(&body).to_be_bytes());
    body.extend([0; 14]);
    let mut package = patch::VERIFIED_HEADER.to_vec();
    package.extend([0; 12]);
    package.extend([b' '; 32]);
    package.extend(transform(&body[32..]));
    package
}

pub fn build(
    input: &Path,
    output: &Path,
    mut progress: impl FnMut(Stage),
) -> Result<BuildReport, BuildError> {
    let mut manifest_name = output.as_os_str().to_owned();
    manifest_name.push(".json");
    let manifest_path = PathBuf::from(manifest_name);
    for path in [output, &manifest_path] {
        if path.try_exists()? {
            return Err(BuildError::OutputExists(path.to_owned()));
        }
    }
    progress(Stage::Reading);
    // The exact C 1.70 package has this size; reject unrelated large files before reading.
    if fs::metadata(input)?.len() != 48_494_024 {
        return Err(BuildError::InvalidInput);
    }
    let stock = fs::read(input)?;
    progress(Stage::Checking);
    if sha256(&stock) != patch::STOCK_PACKAGE_SHA {
        return Err(BuildError::InvalidInput);
    }
    let body = decode(&stock);
    let original = members(&body)?;
    let targets: Vec<_> = original
        .iter()
        .enumerate()
        .filter(|(_, member)| member.name.starts_with(b"eg1840_mas_"))
        .map(|(index, _)| index)
        .collect();
    if targets.len() != 1 {
        return Err(BuildError::InvalidContainer);
    }
    let target = targets[0];
    progress(Stage::Patching);
    let master = patched_master(original[target].bytes)?;
    progress(Stage::Packing);
    let package = pack(&original, target, &master);
    progress(Stage::Verifying);
    let digest = sha256(&package);
    if digest != patch::VERIFIED_PACKAGE_SHA {
        return Err(BuildError::InvalidPackage);
    }
    let decoded = decode(&package);
    let rebuilt = members(&decoded)?;
    if rebuilt.len() != original.len()
        || rebuilt
            .iter()
            .zip(&original)
            .enumerate()
            .any(|(index, (a, b))| {
                a.name != b.name || a.bytes != if index == target { &master } else { b.bytes }
            })
    {
        return Err(BuildError::InvalidPackage);
    }

    progress(Stage::Writing);
    let parent = output
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."));
    let metadata = serde_json::json!({
        "stock_package_sha256": patch::STOCK_PACKAGE_SHA,
        "master_sha256": patch::VERIFIED_MASTER_SHA,
        "package_sha256": digest, "package_bytes": package.len(),
        "build_method": "rust_binary_patch", "only_master_changed": true,
        "roundtrip_verified": true, "matches_verified_release": true,
        "max_module_file_bytes": 1_048_576, "max_module_memory_bytes": 4_194_304
    });
    let mut binary_temp = NamedTempFile::new_in(parent)?;
    binary_temp.write_all(&package)?;
    binary_temp.as_file().sync_all()?;
    let mut metadata_temp = NamedTempFile::new_in(parent)?;
    serde_json::to_writer_pretty(&mut metadata_temp, &metadata).map_err(io::Error::other)?;
    metadata_temp.write_all(b"\n")?;
    metadata_temp.as_file().sync_all()?;
    let manifest_file = metadata_temp
        .persist_noclobber(&manifest_path)
        .map_err(|e| e.error)?;
    if let Err(error) = binary_temp.persist_noclobber(output) {
        drop(manifest_file);
        fs::remove_file(&manifest_path)?;
        return Err(error.error.into());
    }
    Ok(BuildReport {
        output: output.to_owned(),
        manifest: manifest_path,
        sha256: digest,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crc_and_container_validation() {
        assert_eq!(crc16(b"123456789"), 0x31c3);
        assert!(members(b"invalid").is_err());
        assert!(patched_master(b"invalid").is_err());
    }

    #[test]
    fn invalid_input_and_existing_output_are_preserved() {
        let temp = tempfile::tempdir().unwrap();
        let input = temp.path().join("原厂.bin");
        let output = temp.path().join("宿主.bin");
        fs::write(&input, b"wrong firmware").unwrap();
        assert!(matches!(
            build(&input, &output, |_| {}),
            Err(BuildError::InvalidInput)
        ));
        assert!(!output.exists());
        fs::write(&output, b"keep").unwrap();
        assert!(matches!(
            build(&input, &output, |_| {}),
            Err(BuildError::OutputExists(_))
        ));
        assert_eq!(fs::read(&output).unwrap(), b"keep");
    }
}

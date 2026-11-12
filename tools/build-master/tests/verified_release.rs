use build_master::firmware;
use std::{env, fs, path::PathBuf};

#[test]
#[ignore = "Requires Z7II_STOCK_FIRMWARE and Z7II_VERIFIED_FIRMWARE fixtures"]
fn builds_the_verified_release_without_external_tools() {
    let input = PathBuf::from(env::var_os("Z7II_STOCK_FIRMWARE").expect("Set Z7II_STOCK_FIRMWARE"));
    let reference =
        PathBuf::from(env::var_os("Z7II_VERIFIED_FIRMWARE").expect("Set Z7II_VERIFIED_FIRMWARE"));
    let temp = tempfile::tempdir().unwrap();
    let output = temp.path().join("宿主固件.bin");
    let original = fs::read(&input).unwrap();
    let report = firmware::build(&input, &output, |_| {}).unwrap();
    assert_eq!(fs::read(&output).unwrap(), fs::read(reference).unwrap());
    assert_eq!(fs::read(input).unwrap(), original);
    let manifest: serde_json::Value =
        serde_json::from_slice(&fs::read(report.manifest).unwrap()).unwrap();
    assert_eq!(manifest["package_sha256"], report.sha256);
    assert_eq!(manifest["roundtrip_verified"], true);
    assert_eq!(manifest["max_module_file_bytes"], 1_048_576);
    assert_eq!(manifest["max_module_memory_bytes"], 4_194_304);
    assert_eq!(fs::read_dir(temp.path()).unwrap().count(), 2);
}

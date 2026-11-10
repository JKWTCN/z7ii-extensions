# Z7II Extensions

English | [简体中文](README.zh-CN.md)

Native C extensions loaded automatically from an SD card for the Nikon Z 7II running C 1.70.

## Features

| Feature                             | Description                                                 | Status                                                 |
| ----------------------------------- | ----------------------------------------------------------- | ------------------------------------------------------ |
| [Framing grid](docs/framing-grid.md) | Off, 4×4, 3×3, and Golden ratio, with persistent settings | Hardware verified, including restoration after restart |
| [Histogram and virtual horizon](docs/histogram-horizon.md) | Live histogram and virtual horizon together on the rear screen in photo mode | Hardware verified |

## Risk disclaimer

**Flash at your own risk. Modified firmware and native modules may cause crashes, data loss, hardware damage, or an unbootable camera (bricking), potentially requiring professional repair.** Successful testing does not guarantee safety on every camera or in every situation.

Proceed with caution: back up your data, verify the camera model, firmware version, and file checksums, ensure sufficient battery charge, and never interrupt a firmware update. Do not proceed unless you understand and accept these risks. This project is provided without warranty; see the [MIT License](LICENSE).

## Installation

Requires the matching SD loader host to be installed first. Stock firmware cannot load this module directly. Cameras with the host already installed do not need another firmware update.

1. Wait for card access to finish, turn off the camera, and remove the battery for about 30 seconds.
2. Copy [Z72APP.BIN](releases/v0.1.0-preview.2/Z72APP.BIN) to the SD card root. Keep your existing `Z72APP.CFG`, if present.
3. Reinsert the card and battery, then turn on the camera.

Current release: `v0.1.0-preview.2` · Module size: 5,028 bytes · [SHA-256 checksum](releases/v0.1.0-preview.2/SHA256SUMS)

This is a preview release. Sleep/wake behavior and long-term stability remain unverified. Fully power down before replacing or removing the module, and only load trusted modules.

## Building

Requires Python 3.12 and LLVM’s `clang` and `ld.lld` on your PATH. Use `tools/module.py` to build a plugin; build commands are provided in each feature’s documentation. The output file must not already exist.

`host/` contains the loader source, `sdk/` provides module interfaces, and `plugins/` contains plugin implementations.

`tools/build.py` generates the complete firmware update package and a JSON verification report. It requires the exact original C 1.70 update package and LLVM `llvm-objcopy` and `llvm-nm`:

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/build.py --stock-package "path/to/original/Z7_2_0170.bin" --output artifacts/Z7_2_0170.bin
```

The script extracts the master internally; no separate master file or private packaging material is required. The builder reuses the public header digest from the verified release and requires exact SHA-256 matches for the inputs, built host, and complete output. It also decodes the result to check every member. Changed host builds are rejected because the fixed digest only applies to this release. Output files must not already exist. Original firmware and generated update packages are not included in the repository.

## License

[MIT](LICENSE) © 2026 JKWTCN.

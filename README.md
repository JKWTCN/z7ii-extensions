# Z7II Extensions

English | [简体中文](README.zh-CN.md)

Native C extensions loaded automatically from an SD card for the Nikon Z 7II running C 1.70.

## Features

| Feature                             | Description                                                 | Status                                                 |
| ----------------------------------- | ----------------------------------------------------------- | ------------------------------------------------------ |
| [Framing grid](docs/framing-grid.md) | Off, 2×2, 3×3, 4×4, and Golden ratio, with persistent settings | Hardware verified |
| [Histogram and virtual horizon](docs/histogram-horizon.md) | Live histogram and virtual horizon together on the rear screen and in the viewfinder in photo mode | Hardware verified |
| [Portrait display](docs/portrait-ui.md) | Rear-screen shooting information, i menu, and histogram rotate with the camera | Hardware verified |
| [Menu localization](docs/menu-localization.md) | English, Simplified Chinese, and Traditional Chinese labels for the extension settings | Hardware verified in English and Simplified Chinese |

## Known issues

- The framing grid may display incorrectly or disappear. If this happens, turn off the camera, remove and reinsert the battery, then turn the camera back on.

## Risk disclaimer

**Flash at your own risk. Modified firmware and native modules may cause crashes, data loss, hardware damage, or an unbootable camera (bricking), potentially requiring professional repair.** Successful testing does not guarantee safety on every camera or in every situation.

Proceed with caution: back up your data, verify the camera model, firmware version, and file checksums, ensure sufficient battery charge, and never interrupt a firmware update. Do not proceed unless you understand and accept these risks. This project is provided without warranty; see the [MIT License](LICENSE).

## Installation

Install this repository's SD loader host before using extensions. See [Building](#building) to generate the firmware update package.

1. Wait for card access to finish, turn off the camera, and remove the battery for about 30 seconds.
2. Copy [Z72APP.BIN](releases/v0.1.0-preview.6/Z72APP.BIN) to the SD card root. Keep your existing `Z72APP.CFG`, if present.
3. Reinsert the card and battery, then turn on the camera.

Current release `v0.1.0-preview.6` · 18,480 bytes · [SHA-256 checksum](releases/v0.1.0-preview.6/SHA256SUMS)

This is a preview release. Sleep/wake behavior and long-term stability remain unverified. Fully power down before replacing or removing the module, and only load trusted modules.

## Building

To build the combined SD module, use Python 3.12 with LLVM’s `clang` and `ld.lld` on your PATH. Choose an output file that does not already exist.

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/module.py plugins/combined.c --output artifacts/Z72APP.BIN
```

`host/` contains the loader source, `sdk/` provides module interfaces, and `plugins/` contains plugin implementations.

The host and module builder accept up to **1 MiB per `Z72APP.BIN` file** and **4 MiB for the image plus BSS**. The file limit includes the header and relocation table.

`tools/build.py` generates the complete firmware update package and a JSON verification report. It requires the exact original C 1.70 update package and LLVM `llvm-objcopy` and `llvm-nm`:

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/build.py --stock-package "path/to/original/Z7_2_0170.bin" --output artifacts/Z7_2_0170.bin
```

The script extracts the master internally; no separate master file or private packaging material is required. The builder reuses the public header digest from the verified release and requires exact SHA-256 matches for the inputs, built host, and complete output. It also decodes the result to check every member. Changed host builds are rejected because the fixed digest only applies to this release. Output files must not already exist. Original firmware and generated update packages are not included in the repository.

## License

[MIT](LICENSE) © 2026 JKWTCN.

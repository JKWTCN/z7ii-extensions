# Z7II Extensions

[English](README.md) | 简体中文

Nikon Z 7II C 1.70 的 SD 卡原生 C 扩展框架，开机自动加载卡上的模块。

## 当前功能

| 功能                                           | 说明                                                       | 状态       |
| ---------------------------------------------- | ---------------------------------------------------------- | ---------- |
| [取景网格](docs/framing-grid.md)                | 关闭、2×2、3×3、4×4、黄金分割，支持保存设置             | 已实机验证 |
| [直方图与水平仪同显](docs/histogram-horizon.md) | 照片模式下，后屏和电子取景器均可同时显示实时直方图与水平仪 | 已实机验证 |
| [后屏竖拍界面](docs/portrait-ui.md)             | 拍摄信息、i 菜单和直方图随相机方向转正                     | 已实机验证 |
| [照片拍摄加亮显示](docs/photo-highlights.md)    | LCD 与取景器的独立照片斑马纹，支持两种图案和八档阈值       | 已实机验证 |
| [自定义 DISP 页面](docs/custom-disp.md) | 照片 LCD 页面开关、四套预设、八项拍摄信息、原厂滚动列表及参数信息页 | 已实机验证 |
| [焦距显示](docs/focal-length.md) | 手动变焦镜头的实时焦距（需电子通信），支持独立 DISP 开关 | 已实机验证 |
| [电量百分比显示](docs/battery-percent.md) | 照片 LCD 电池框内透明底百分比，提供默认关闭的全局开关 | 已实机验证 |
| [AF 区域快捷切换](docs/af-area-shortcut.md) | 原厂自定义控制中为五个按键分配照片 AF 区域，按一次切换并保持 | 已实机验证 |
| [菜单本地化](docs/menu-localization.md)         | 扩展设置支持简体中文、繁体中文和英文标签                   | 已实机验证 |

## 已知问题

- 取景网格线有时会绘制错误或不显示,扩展框架功能没出现。遇到这种情况，请先关机，取出并重新装回电池，等待到 i 按键旁边的读取灯灭了再开机。

## 风险声明

**刷机有风险，请谨慎操作。修改版固件和原生模块可能导致死机、数据丢失、硬件损坏，甚至相机无法启动（变砖），可能需要专业维修。** 已有实机验证不代表所有相机和使用场景均安全。

操作前请备份数据，核对机型、固件版本及文件校验值，确保电量充足，切勿中断固件更新。如果无法理解或接受上述风险，请勿刷入或使用。使用风险由使用者自行承担；本项目不提供任何担保，详见 [MIT 许可证](LICENSE)。

## 使用

使用扩展前须安装本仓库提供的 SD 加载宿主。之后只需要更改 SD 卡中的 Z72APP.BIN 文件即可,无需再次刷入固件。

可以在 [Releases](https://github.com/JKWTCN/z7ii-extensions/releases/tag/build-master) 中找到 Windows 下的图形界面宿主固件生成器.

如果要自己构建固件升级包的生成方法见[构建](#构建)。

1. 等待存储卡访问结束，关机并取出存储卡。
2. 将 [Z72APP.BIN](releases/v0.1.0-preview.11/Z72APP.BIN) 放到 SD 卡根目录，已有 `Z72APP.CFG` 可保留。
3. 装回卡和电池，等待到 i 按键旁边的读取灯灭了再开机。

当前版本为 `v0.1.0-preview.11`，模块 50,140 字节。[SHA-256 校验](releases/v0.1.0-preview.11/SHA256SUMS)

这是试用版本，休眠唤醒和长期稳定性尚未单独确认。替换或移除模块请完整断电。只加载可信模块。

## 构建

构建组合 SD 模块需要 Python 3.12，以及 PATH 中的 LLVM `clang`、`ld.lld`。请选择尚未存在的输出文件。

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/module.py plugins/combined.c --output artifacts/Z72APP.BIN
```

`host/` 为宿主源码，`sdk/` 为模块接口，`plugins/` 为插件实现。

宿主和模块构建器支持最大 **1 MiB 的 `Z72APP.BIN` 文件**，镜像与 BSS 合计最大 **4 MiB**。文件上限包含文件头和重定位表。

独立 Windows 图形界面程序可直接下载 [build-master.exe](releases/build-master.exe)（[SHA-256 校验](releases/SHA256SUMS)）。程序按 Windows 显示语言自动选择中文或英文，提供原厂固件和输出位置选择，内置二进制补丁与升级包校验逻辑，生成完整升级包和 JSON 校验报告。Rust 源码位于 `tools/build-master`。

在 Windows 上使用 Rust 编译桌面程序：

```powershell
Push-Location tools/build-master
cargo build --release
Pop-Location
```

运行 `tools/build-master/target/release/build-master.exe`，选择原版 C 1.70 升级包和新的输出文件名，点击“生成宿主固件”。Windows x64 MSVC 构建使用静态 C 运行库。

使用 Python 3.12，`tools/build-master.py` 可直接对原版 C 1.70 升级包应用已验证宿主的二进制补丁，生成完整可刷入升级包及 JSON 校验报告：

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/build-master.py --stock-package "path/to/original/Z7_2_0170.bin" --output artifacts/Z7_2_0170.bin
```

此命令使用内置补丁复现当前发布的 1 MiB / 4 MiB 宿主，只依赖 Python 标准库。

编译宿主源码可使用 `tools/build.py`，生成相同的完整固件升级包。需要精确匹配的原版 C 1.70 升级包，以及 LLVM `clang`、`ld.lld`、`llvm-objcopy`、`llvm-nm`：

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/build.py --stock-package "path/to/original/Z7_2_0170.bin" --output artifacts/Z7_2_0170.bin
```

两个脚本都在内部提取主固件并复用已验证发布包的公开头部校验值，强制核对输入、生成的宿主及完整输出的 SHA-256，再重新解包核对所有成员。固定校验值仅适用于当前发布版本，因此宿主构建发生变化时会拒绝输出。输出文件必须不存在。仓库不包含原版固件或生成的升级包。

## 许可证

采用 [MIT 许可证](LICENSE)，版权所有 © 2026 JKWTCN。

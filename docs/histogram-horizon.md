# Histogram and virtual horizon / 直方图与水平仪同显

In photo mode, press DISP to select the virtual horizon page on the rear screen. The module adds the camera’s live histogram to that page and retains the framing grid and its saved settings.

在照片模式用后屏取景，按 DISP 切到水平仪页即可同时查看原厂实时直方图。组合版保留取景网格和已保存的网格设置。

Use the [combined SD module](../releases/v0.1.0-preview.3/Z72APP.BIN) with the existing Z 7II C 1.70 SD loader host. Follow the installation steps in the [README](../README.md).

已安装 Z 7II C 1.70 SD 加载宿主的相机可直接更换 [组合版 SD 模块](../releases/v0.1.0-preview.3/Z72APP.BIN)，无需更新宿主。安装步骤见 [中文 README](../README.zh-CN.md)。

## Hardware verification / 实机验证

On November 10, 2026, the tester confirmed simultaneous display on the rear screen. The original photo shows the histogram at lower right, the virtual horizon in the center, and the framing grid. EVF, video and HDMI were outside this test.

2026-11-10，测试者实机确认后屏同显通过。原图可见右下方直方图、中央水平仪及取景网格。EVF、录像及 HDMI 不在本次验证范围内。

![Histogram, virtual horizon, and framing grid on the Z 7II rear screen](images/z7ii-histogram-horizon-hardware-2026-11-10.jpg)

## Build / 构建

Run from the repository root with Python 3.12 and LLVM installed. / 安装 Python 3.12 和 LLVM 后，在仓库根目录执行。

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/module.py plugins/grid_histogram_horizon.c --output artifacts/Z72APP.BIN
```

The released module retains diagnostic export to `Z72HIST.LOG` on d12 confirmation. / 发布模块保留诊断记录，在 d12 确认网格选项时导出 `Z72HIST.LOG`。

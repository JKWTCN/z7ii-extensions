# Histogram and virtual horizon / 直方图与水平仪同显

In photo mode, press DISP to select the virtual horizon page on the rear screen or in the electronic viewfinder. The module adds the camera’s live histogram to that page and retains the framing grid and its saved settings.

在照片模式使用后屏或电子取景器，按 DISP 切到水平仪页即可同时查看原厂实时直方图。组合版保留取景网格和已保存的网格设置。

Use the combined SD module with the existing Z 7II C 1.70 SD loader host. [Module download](../releases/v0.1.0-preview.4/Z72APP.BIN) · [Installation](../README.md#installation)

已安装 Z 7II C 1.70 SD 加载宿主的相机可直接更换卡上的组合版模块。[模块下载](../releases/v0.1.0-preview.4/Z72APP.BIN) · [安装步骤](../README.zh-CN.md#使用)

The module limits the histogram refresh rate and skips extra draws while a display switch is pending or the renderer is busy.

模块限制直方图的刷新频率，并在等待切换显示器或渲染繁忙时跳过追加绘制。

## Hardware verification / 实机验证

On November 10, 2026, the tester confirmed the overlay on the rear screen and in the viewfinder in photo mode. The tester also passed the follow-up test of automatic switching between the two displays with the overlay active.

2026-11-10，测试者确认照片模式下的后屏和电子取景器均可同显，开启同显后两者之间的自动切换也通过了实机复测。

![Histogram, virtual horizon, and framing grid on the Z 7II rear screen](images/z7ii-histogram-horizon-hardware-2026-11-10.jpg)

![Histogram, virtual horizon, and framing grid in the Z 7II electronic viewfinder](images/z7ii-histogram-horizon-evf-hardware-2026-11-10.png)

## Build / 构建

Run from the repository root with Python 3.12 and LLVM installed. / 安装 Python 3.12 和 LLVM 后，在仓库根目录执行。

```powershell
New-Item -ItemType Directory -Force artifacts | Out-Null
python -B tools/module.py plugins/grid_histogram_horizon.c --output artifacts/Z72APP.BIN
```

The released module retains diagnostic export to `Z72HIST.LOG` on d12 confirmation. / 发布模块保留诊断记录，在 d12 确认网格选项时导出 `Z72HIST.LOG`。

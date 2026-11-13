# Custom DISP pages / 自定义 DISP 页面

[English README](../README.md) · [中文说明](../README.zh-CN.md)

In photo mode on the rear LCD, choose which pages participate in the DISP cycle and select each page's components. Disabled pages are skipped, and at least one page remains enabled. The Setup Menu entry **Custom DISP pages** follows the camera's language in English, Simplified Chinese, or Traditional Chinese.

照片模式的后屏 LCD 支持自定义 DISP 循环：选择参与循环的页面，并分别设置每页组件。禁用页面自动跳过，至少保留一页启用。设定菜单中的「自定义DISP页面」支持简体中文、繁体中文和英文。

| Page / 页面 | Default components / 默认组件 |
| --- | --- |
| Standard / 标准 | Shooting information and framing grid / 拍摄信息、取景网格 |
| Minimal / 极简 | Shutter speed, aperture, and ISO / 快门速度、光圈、ISO |
| Composition / 构图 | Framing grid and virtual horizon / 取景网格、水平仪 |
| Exposure / 曝光 | Shooting information, histogram, and photo highlights / 拍摄信息、直方图、照片加亮 |
| Information display / 参数信息页 | Native full information display, excluded by default / 原厂完整参数界面，默认不加入循环 |

**Custom pages** is the master switch. Turning it off restores the native DISP cycle. Each preset has its own page-enabled switch, shooting-information selection, framing-grid, histogram, virtual-horizon, and photo-highlight switches. **Reset preset** restores its default components while retaining its page-enabled state. The native information display has an independent enabled switch and can be the only enabled page; its contents and orientation remain native.

「自定义页面」是总开关，关闭后恢复原厂 DISP 循环。每套预设分别设置「页面启用」「拍摄信息」「取景网格」「直方图」「水平仪」「照片加亮」。「恢复预设」恢复该页默认组件，保留页面启用状态。「参数信息页」有独立开关，也可作为唯一启用的页面，内容和方向沿用原厂界面。

## Shooting information / 拍摄信息

**Shooting info** opens seven independent switches. Each preset saves its own selection; the parent row shows OFF when all seven are disabled. Minimal starts with the three exposure fields but can also display other groups when selected.

「拍摄信息」进入七项独立开关的子菜单，每套预设分别保存。全部关闭时，上一级显示 OFF。极简页初始开启前三项，也可手动加入其他信息组。

| Group / 信息组 | Display / 显示内容 |
| --- | --- |
| Shutter speed / 快门速度 | Shutter speed value / 快门速度 |
| Aperture / 光圈 | Aperture value / 光圈值 |
| ISO sensitivity / ISO感光度 | ISO and its automatic status / ISO 及自动状态 |
| Shooting mode / 拍摄模式 | P, A, S, M, and related modes / P、A、S、M 等模式 |
| Release mode / 释放模式 | Single-frame, continuous, and related release information / 单张、连拍等释放模式信息 |
| Focus information / 对焦信息 | Native focus-mode information / 原厂对焦模式信息 |
| Other information / 其他信息 | Remaining status widgets previously controlled by Shooting info / 原「拍摄信息」开关覆盖的其余状态控件 |

The AF frame layer, unidentified controls, and native warnings retain their original behavior. The component switches select visibility: framing-grid style still comes from d12, while highlight pattern and threshold still come from Photo shooting highlights. A component switch does not enable a globally disabled grid or highlight pattern.

对焦框层、未识别控件和原厂警告保留原有行为。组件开关控制显示资格：网格样式仍取自 d12，照片加亮图案和阈值仍取自「照片拍摄加亮显示」；不会强制打开全局已关闭的网格或加亮图案。

Rear-LCD live-view presets follow Auto rotate info. The viewfinder, movie mode, physical MENU screens, playback, and magnified live view retain their existing display policies. Both LCD visibility-mask banks used before and during the observed focus operation follow the selected information groups; histogram placement temporarily extends the corresponding LCD masks and restores them afterward.

后屏取景预设遵循「自动旋转信息显示」。取景器、视频、实体 MENU、回放及放大取景沿用原有显示规则。普通取景和已观察到的对焦操作所用的两组 LCD 显示掩码都遵循信息组选项；直方图布局临时扩展对应掩码，完成后恢复。

Preferences share `Z72APP.CFG` with the other extensions. Existing v1–v6 records retain their previous display behavior. Editing detailed information groups uses the 80-byte v7 format, and saving another extension's preferences preserves those selections. DISP presses do not write the current page to the card. The label **水平仪／水平儀** uses a small supplemental glyph set to avoid missing-character boxes.

设置与其他扩展共用 `Z72APP.CFG`。v1–v6 旧配置保留原有显示效果；编辑细分信息组后使用 80 字节的 v7 格式，修改其他扩展设置时也会保留这些选择。按 DISP 切页不会写卡保存当前页面。「水平仪／水平儀」使用小型补充字库，避免缺字方框。

## Verification / 验证

On November 13, 2026, the user confirmed this combined build passed testing on a Z 7II. The update includes the custom DISP cycle, native information-display option, seven information groups, corrected LCD focus-operation masks, and the horizon label.

2026-11-13，用户确认本次组合版本已通过 Z 7II 实机验证。此次更新包含自定义 DISP 循环、原厂参数信息页、七项拍摄信息、对焦期间 LCD 显示掩码修订及「水平仪」标签，状态为**已实机验证**。

Local checks execute native visibility and input instructions, including 153 legacy visibility cases, 168 detailed-information cases, both LCD mask banks, information-page transitions, and restoration outside photo LCD live view. Menu/configuration checks cover 44 selections, legacy records, v7 persistence, reset, and last-page protection. Language checks cover 36 banks, 828 labels, and 159 glyph outlines; photo-highlight regressions also pass. Hardware confirmation does not establish every component combination, every language, sleep/wake behavior, or long-term stability.

本地检查执行原厂可见性及按键指令，覆盖 153 个原有显示组合、168 个细分信息组合、两组 LCD 掩码、参数页往返及退出恢复；菜单和配置检查覆盖 44 次选择、旧记录、v7 保存、恢复预设和最后一页保护。语言检查覆盖 36 个语言表、828 条标签及 159 个字形轮廓，照片加亮回归也通过。实机确认不代表所有组件组合、所有语言、休眠唤醒及长期稳定性均已验证。

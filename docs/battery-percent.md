# Battery percentage display / 电量百分比显示

[English README](../README.md) · [中文说明](../README.zh-CN.md)

Show the camera-reported battery percentage inside the battery outline on the rear LCD in photo mode and movie standby or recording. Small centered digits replace the charge bars, with a transparent background that lets the live image show through. The value comes from the same native snapshot as Battery info, rather than an estimate based on the number of bars.

照片模式及视频待机、录制期间，后屏 LCD 均支持在电池框内显示机身报告的真实电量百分比。数字以小字号居中，替换原厂电量填充条，背景透明，可透出取景画面。读数来自原厂「电池信息」使用的同一电量快照，不按图标格数估算。

- **Custom DISP pages → Battery percentage display** is a global switch, placed immediately after **Information display**. It is **off by default**. The row shows ON/OFF and opens an Enable/Disable choice.
- The choice is independent of the four presets and the Custom pages master switch. It does not add a page to the DISP cycle. Turning it off restores the native battery icon.
- The percentage follows the native icon's visibility and the existing Auto rotate info layout. Native battery redraws, including focus-related redraws, refresh the percentage in the same pass. Old digits are cleared before painting; there is no solid background.
- Battery data is sampled once per second. Values outside 0–100 are hidden. Reporting depends on the battery supplying valid information to the camera.
- Photo and movie modes share the same global switch. Starting a recording keeps the percentage visible instead of restoring the native charge bars.
- The native information display, menus, playback, and viewfinder retain their existing battery presentation.

- 「自定义DISP页面」主菜单的「参数信息页」下方提供全局「电量百分比显示」，**默认关闭**。右侧显示 ON／OFF，点开选择启用／禁用。
- 此项独立于四套预设和「自定义页面」总开关，不参与 DISP 页面循环；关闭后恢复原厂电量图标。
- 随原厂电池图标显示或隐藏，并支持现有「自动旋转信息显示」布局。对焦等操作触发原厂电池重绘时，百分比在同一轮绘制中刷新。先清除旧数字再重画，保持透明底。
- 每秒检查一次读数，仅接受 0–100。能否获取有效读数取决于电池是否向机身报告电量。
- 照片和视频共用同一全局开关；开始录制后仍保持百分比，不再恢复原厂电量填充条。
- 原厂参数信息页、菜单、回放及取景器保留各自的电量显示方式。

The setting is saved with the other preferences in `Z72APP.CFG`. Enabling it uses the 80-byte v9 format when no AF-area shortcut is assigned and Wi-Fi region is Follow camera; shortcut assignments use v10. An explicit Wi-Fi region uses 128-byte v12. All preserve this setting. v1–v8 records remain readable and leave this option off. Other page and component selections are preserved. Labels are available in English, Simplified Chinese, and Traditional Chinese; a small supplemental font supplies missing Chinese glyphs.

设置与其他扩展共用 `Z72APP.CFG`。未分配 AF 区域快捷功能且 Wi-Fi 地区为「跟随机身」时，启用后采用 80 字节 v9；存在快捷分配时采用 v10；选择显式 Wi-Fi 地区时采用 128 字节 v12，均保留本开关。兼容读取 v1–v8 旧配置，该选项默认关闭，原页面及组件选择保留。菜单支持简体中文、繁体中文和英文，缺少的中文字形由模块的小型补充字库提供。

## Verification / 验证

### Movie recording / 视频录制

**Hardware verified.** On November 15, 2026, the user confirmed the 58,452-byte complete combined module keeps the percentage visible while recording. The supplied images show `90%` both in movie standby and during active recording, with the REC indicator, red recording border, and native audio meters visible. The public build is byte-for-byte identical to this tested module.

**已实机验证。** 2026-11-15，用户确认 58,452 字节完整组合模块在录制期间也能保持百分比。提供的照片中，视频待机与实际录制时均显示 `90%`；录制画面同时显示 REC 标记、红色录制边框及原厂双声道电平条。发布模块与本次实机测试包逐字节一致。

![Hardware-verified battery percentage during active movie recording / 已实机验证的录制中电量百分比](images/z7ii-battery-percentage-movie-recording.jpg)

![Hardware-verified battery percentage in movie standby / 已实机验证的视频待机电量百分比](images/z7ii-battery-percentage-movie-standby.jpg)

Local checks reproduce the earlier rejection of nonzero movie display states, then execute the native battery-visibility selector and test all four supported states, repeated redraws, and three orientations. Photo-mode exclusions, the global switch, and native-icon restoration remain covered. This confirmation does not establish every recording format, battery model, orientation, or long-term recording condition.

本地检查先复现此前拒绝非零视频显示状态的问题，再执行原厂电池可见性选择，检查四种合法状态、反复重绘及三种方向。照片模式的排除规则、全局开关和原厂图标恢复也保持覆盖。此次确认不代表所有录制格式、电池型号、方向及长期录制条件均已验证。

### Photo mode and menus / 照片模式与菜单

**Hardware verified.** On November 13, 2026, the user confirmed the 47,708-byte complete combined module passed camera testing. The photos show the Chinese and English global setting with its OFF indicator, and `82%` inside the transparent battery frame alongside the focal length, histogram, and virtual horizon.

**已实机验证。** 2026-11-13，用户确认 47,708 字节完整组合模块通过实机验证。照片展示中英文全局设置及 OFF 状态，以及透明电池框内的 `82%` 与焦距、直方图、水平仪同时显示。

![Hardware-verified Chinese global battery percentage setting / 已实机验证的中文电量百分比全局设置](images/z7ii-battery-percentage-menu-zh-cn.png)

![Hardware-verified English global battery percentage setting / 已实机验证的英文电量百分比全局设置](images/z7ii-battery-percentage-menu-en.png)

![Hardware-verified transparent battery percentage in photo live view / 已实机验证的照片取景透明底电量百分比](images/z7ii-battery-percentage-live-view.png)

Local checks cover default-off behavior, menu selection and cancellation, persistence and legacy configuration migration, independence from the DISP master, native icon restoration, transparent clearing, focus redraws, rotation geometry, and native glyph decoding. The public module is byte-for-byte identical to the hardware-tested build. Modeled file and display services do not establish every battery's compatibility, every orientation, sleep/wake behavior, or long-term stability.

本地检查覆盖默认关闭、菜单选择与取消、保存及旧配置迁移、独立于 DISP 总开关、恢复原厂图标、透明清除、对焦重绘、旋转位置及原厂字形解码。发布模块与实机测试版本逐字节一致。文件和最终显示服务中有替身；此次确认不代表所有电池兼容性、每种方向、休眠唤醒及长期稳定性均已验证。

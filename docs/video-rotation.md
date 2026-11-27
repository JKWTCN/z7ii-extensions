# 视频自动方向 / Automatic video orientation

简体中文 | [English](#english)

Nikon Z 7II C 1.70 安装 SD 加载宿主后，组合模块会自动为普通内录的 MOV／MP4 写入方向元数据。每个文件在开档时锁定机身姿态，录制中转动机身也会保持该方向。原厂另开分段文件时，会为新文件重新读取姿态。支持方向元数据的播放器会按视频轨的矩阵显示。

机内视频回放沿用原厂横向显示。

安装或升级请参见 [使用](../README.md#使用)。

## English

On the Nikon Z 7 II C 1.70 with the SD loader installed, the combined module automatically writes orientation metadata to standard in-camera MOV and MP4 recordings. The camera orientation is locked when each file opens and stays fixed as the camera turns during recording. A new file opened by the camera's native splitting process gets its own orientation. Players that support the video track's display matrix rotate the image at playback.

The camera's playback menu displays videos in its native landscape layout.

See [Installation](../README.en.md#installation) for the current module.

## 版本更新 / Version history

### v0.1.0-preview.36

- 为倒置录制的文件写入 180° 视频轨方向矩阵。MOV 和 MP4 封装均通过本地固件测试。
- Added a 180° track matrix to upside-down recordings. Both MOV and MP4 passed local firmware tests.

### v0.1.0-preview.25

- 首次支持在机内录制的 MOV 和 MP4 写入方向元数据。1080/60p MOV 的方向写入和电脑播放器竖放已实机验证。
- Added recording orientation metadata to in-camera MOV and MP4 files. A 1080/60p MOV recorded on the camera played upright in portrait orientation on a computer.

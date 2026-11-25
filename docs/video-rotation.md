# 视频自动方向 / Automatic video orientation

简体中文 | [English](#english)

Nikon Z 7II C 1.70 安装 SD 加载宿主后，组合模块会自动为普通内录的 MOV／MP4 写入方向元数据。每个文件在开档时锁定机身姿态，录制中转动机身也会保持该方向。原厂另开分段文件时，会为新文件重新读取姿态。支持方向元数据的播放器会按视频轨的矩阵显示。

`v0.1.0-preview.36` 增加了倒置录制的 180° 视频轨矩阵。MOV 和 MP4 的封装路径均通过本地固件测试。

**原厂回放菜单暂未实现视频方向跟随。** 机内视频回放仍沿用原厂横向显示。已用实机录制的 1080/60p MOV 样片确认方向写入和电脑播放器竖放。

安装或升级请参见 [使用](../README.md#使用)。

## English

On the Nikon Z 7 II C 1.70 with the SD loader installed, the combined module automatically writes orientation metadata to standard in-camera MOV and MP4 recordings. The camera orientation is locked when each file opens and stays fixed as the camera turns during recording. A new file opened by the camera's native splitting process gets its own orientation. Players that support the video track's display matrix rotate the image at playback.

`v0.1.0-preview.36` adds a 180° video track matrix for upside-down recordings. Both MOV and MP4 passed local firmware tests.

**Automatic orientation is currently unavailable in the camera's native playback menu.** In-camera video playback uses the original landscape display. Orientation metadata and portrait playback on a computer were verified with a 1080/60p MOV recorded on the camera.

See [Installation](../README.en.md#installation) for the current module.

# 倒置照片回放 / Upside-down photo playback

播放菜单在「旋转至竖直方向」下方提供「旋转至倒置方向」。该选项默认开启，与竖拍旋转独立设置，并保存到 SD 卡根目录的 `Z72APP.CFG`。按播放键进入回放后，倒置拍摄的 JPG 和 NEF 会在单张查看和放大时转正。

**Rotate inverted** appears just below **Rotate tall** in the Playback Menu. It defaults to **On**, works independently of **Rotate tall**, and is saved in `Z72APP.CFG` on the SD card. It corrects upside-down JPG and NEF photos in single-image playback and playback zoom.

## 版本更新 / Version history

### v0.1.0-preview.39

- 修复倒置照片单张回放和放大时顶部或右侧的杂色像素。预读布局和绘制共用一次方向判断，避免同一张照片两次回放时方向不一致。全图、留边和放大裁剪均通过本地固件测试。
- Fixed stray pixels along the top or right edge in single-image playback and playback zoom. Preview layout and rendering share one orientation decision for consistent playback. Full-frame, letterboxed, and zoomed playback passed local firmware tests.

### v0.1.0-preview.37

- 倒置拍摄的 JPG 和 NEF 写卡时使用标准 EXIF 方向值 3，支持 EXIF 的电脑查看器会自动转正。2026-11-25 在 Z 7 II 实机验证。
- Writes standard EXIF Orientation 3 to new upside-down JPG and NEF photos so desktop viewers that read EXIF orientation can display them upright. Verified on a Z 7 II on November 25, 2026.

### v0.1.0-preview.36

- 播放菜单加入「旋转至倒置方向」，支持倒置 JPG、NEF 的单张回放和放大转正。2026-11-25 在 Z 7 II 实机验证。
- Added Rotate inverted to the Playback Menu for single-image playback and zoom of upside-down JPG and NEF photos. Verified on a Z 7 II on November 25, 2026.
- 回放可按 EXIF 方向 3 转正。Z 7II 早期拍摄的照片若写入方向 1，也可通过机身拍摄记录的滚转角识别倒置。
- Playback recognizes EXIF Orientation 3 and can use the recorded camera roll to identify earlier Z 7II photos saved with Orientation 1.

![Z 7 II 播放菜单中，「旋转至倒置方向」位于「旋转至竖直方向」下方，状态为 ON](images/z7ii-rotate-inverted.png)

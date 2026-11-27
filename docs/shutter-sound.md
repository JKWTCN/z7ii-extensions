# Silent shutter sound / 静音拍摄快门声音

The Nikon Z 7II running C 1.70 can play a custom WAV through its speaker when silent photography is enabled in photo mode. Put one or more WAV files named `SHUTTER1.WAV` through `SHUTTER5.WAV` in the SD card root. Fully power the camera off and on after adding or replacing a file. Open **Setup Menu → Silent shutter sound** to select a file and preview it.

Nikon Z 7II C 1.70 在照片模式下开启静音拍摄后，可以通过机身扬声器播放自定义快门声。将一个或多个 WAV 文件放在 SD 卡根目录，文件名为 `SHUTTER1.WAV` 至 `SHUTTER5.WAV`。添加或替换音效后，请完整关机再开机。在「**设定菜单 → 静音拍摄快门声音**」中选择文件并试听。

| Setting / 设置 | Options / 选项 | Default / 默认 |
| --- | --- | --- |
| Custom shutter sound / 自定义快门声 | `SHUTTER1.WAV`–`SHUTTER5.WAV`, Off / 关闭 | Off / 关闭 |
| Continuous release / 连拍时播放 | First shot only, Every shot, Restart on every shot / 仅第一张、每张照片、每张从头播放 | Every shot / 每张照片 |
| Volume / 音量大小 | 1–5 | 5 |
| Preview current sound / 试听当前音效 | Plays the selected file at the current volume / 以当前音量播放所选文件 | |

The selected WAV, continuous-release option, and volume are saved in `Z72APP.CFG` on the SD card. If the selected WAV is unavailable at startup, playback stays off and the saved selection is restored when the file is available at a later startup.

所选音效、连拍播放方式和音量会保存在 SD 卡的 `Z72APP.CFG`，重开机后自动恢复。开机时若所选 WAV 不可用，快门声暂时关闭。文件恢复后再次开机，即可恢复先前选择。

Use 48 kHz, 16-bit PCM WAV files up to **1 second** long. Both mono and stereo are supported. Missing files and files outside these limits appear as unavailable. In **Every shot** mode, the camera skips the sound for a frame if the previous clip is still playing. Use a short clip to hear more of the frames in a fast burst.

使用最长 **1 秒**的 48 kHz、16 位 PCM WAV，支持单声道和双声道。文件缺失或不符合要求时，对应选项会显示「不可用」。「每张照片」模式下，如果前一段音效仍在播放，这一张会跳过音效。高速连拍时使用较短的音效，可以让更多帧有声音。

## Sample WAV files / 示例音效

The [release directory](../releases/) includes five sample WAV files named `SHUTTER1.WAV` through `SHUTTER5.WAV`. 发布目录附带 `SHUTTER1.WAV` 至 `SHUTTER5.WAV` 五个示例音效。

Use FFmpeg to convert a clip and keep its first second. 以下命令可转换音效并截取前 1 秒。

```powershell
ffmpeg -i input.wav -t 1 -ar 48000 -ac 1 -c:a pcm_s16le SHUTTER1.WAV
```

The supplemental Noto Sans SC menu glyphs are distributed under the SIL Open Font License. 菜单补充字形取自 Noto Sans SC，采用 SIL 开放字体许可证。

## 版本更新 / Version history

### v0.1.0-preview.43

- 选择「**每张从头播放**」后，连拍下一张时会打断仍在播放的自定义快门声，并从 WAV 开头重播。停止播放期间收到的多帧请求合为一次重播。SD 模块开机时恢复所选模式。连拍打断重播已通过实机验证。
- With **Restart on every shot**, the next shot in a burst stops any custom shutter sound still playing and starts the WAV from the beginning. The module combines restart requests received while the speaker stops into one restart. The combined module restores the selected mode on startup. Interruption and restart passed on-camera testing.

### v0.1.0-preview.42

- 快门声音只检查属于快门输入回调表的按键位，避免其他按键事件进入快门声音处理。
- Limits shutter-sound processing to input bits covered by the camera's shutter input callbacks.

### v0.1.0-preview.18

- 首次为静音拍摄加入自定义快门声。支持从 SD 卡加载五个 WAV 槽位、试听和调音量，连拍可选「仅第一张」或「每张照片」。
- Added custom sounds for silent photography with five WAV slots on the SD card, preview, volume control, and First shot only or Every shot options for bursts.
- 提供 1 至 5 档音量、五份示例 WAV 和开机恢复设置。支持最长 1 秒的 48 kHz、16 位单声道或双声道 PCM。
- Added five volume levels, five sample WAVs, and settings restoration at startup. Supports mono or stereo 48 kHz, 16-bit PCM clips up to one second.

# 对焦改进 / Focus improvements

## 遮挡后找回主体

在「设定菜单 → 对焦改进 → 遮挡后找回主体」选择启用或禁用。默认禁用，设置立即生效，与其他模块设置一起保存在 SD 卡根目录的 `Z72APP.CFG`，重启后保留。

在照片模式选择 **AF-C → 主体跟踪**，对准目标后半按快门或按住 AF-ON。启用后，模块在低置信度时用最近可靠的位置引导原厂匹配。该位置从最后一次可靠匹配起保留 400 毫秒，辅助匹配之间至少间隔 80 毫秒。禁用时，主体跟踪使用原厂匹配流程。 [主体跟踪操作](subject-tracking.md)

菜单、设置保存和匹配回退已通过离线检查。实机找回时间和跳背景次数仍待验证。可以用同一场景对比开关两种状态，分别记录主体露出到跟踪框稳定所需的时间，以及跟踪框跳到背景的轮次。

## Reacquire after occlusion

Use **Setup Menu → Focus improvements → Reacquire after occlusion** to enable or disable the feature. It is disabled by default. Changes take effect immediately and persist with the other module settings in `Z72APP.CFG` at the root of the SD card.

In photo mode, select **AF-C → Subject tracking**, aim at the subject, and hold a half-press or AF-ON. When enabled, the module uses the last reliable position to guide native matching during low-confidence frames. The position expires 400 ms after the last reliable match, and assisted matches are at least 80 ms apart. With the switch disabled, subject tracking uses the native matching path. [Subject tracking controls](subject-tracking.md)

Menu navigation, settings persistence, and matcher fallback passed offline checks. On-camera reacquisition time and background switches are still unverified. Compare both switch positions in the same scene and record the delay from reappearance to stable tracking alongside the number of trials that switch to the background.

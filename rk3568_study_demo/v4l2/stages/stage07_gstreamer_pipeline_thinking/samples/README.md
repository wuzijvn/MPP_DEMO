# Stage07 Samples

基础 demo 默认使用 `videotestsrc`，不需要外部样本。

后续可放入真实样本：

- `sample.h264`：用于 `filesrc ! h264parse ! <decoder> ! ...`
- `sample.hevc`：用于 `filesrc ! h265parse ! <decoder> ! ...`
- `sample.mp4`：用于 demux/decode pipeline

真实码流验证时必须记录：

- codec/profile/level/resolution/fps
- pipeline
- decoder element
- GST_DEBUG
- device node/dmesg/backend log
- CPU/fps/fallback 证据

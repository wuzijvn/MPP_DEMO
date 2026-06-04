# Stage07 Enterprise Samples

当前企业项目默认使用 `videotestsrc` 生成 raw video，不强依赖外部样本。

如果后续扩展真实文件解码，可以放入：

- H.264 Annex-B：用于 `h264parse ! <decoder> ! ...`
- MP4/MKV：用于 `filesrc ! qtdemux/matroskademux ! parse ! decode`

真实硬件解码验证必须额外记录后端 element、设备节点、dmesg、CPU/fps、是否 fallback。

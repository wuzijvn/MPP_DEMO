# Enterprise samples

当前企业项目默认模拟 V4L2 M2M queue loop，并可选执行 `/dev/videoX` 的 QUERYCAP 设备证据采集。
要扩展为真实 decode，需要加入 Annex B 码流读取、真实 OUTPUT QBUF payload、CAPTURE mmap buffer 和 DQBUF frame dump。

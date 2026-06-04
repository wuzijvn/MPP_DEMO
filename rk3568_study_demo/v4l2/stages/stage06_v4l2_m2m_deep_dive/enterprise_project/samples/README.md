# Enterprise samples

VM/vim2m 模式不需要 codec 码流样本，它用 raw pattern 写入 OUTPUT buffer 来验证 M2M queue 逻辑。

RK/RKMPP 模式如果要验证真实硬解，请把 H.264/H.265 elementary stream 或容器样本放到这里，然后运行：

```bash
MODE=rk-rkmpp INPUT=samples/sample.h264 DECODER=h264_rkmpp \
  ../scripts/run_07_enterprise_m2m_diagnostic_service.sh
```

没有样本时，RK 模式只采集板端证据，不声称硬解成功。

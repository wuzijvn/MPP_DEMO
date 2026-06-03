# RKMPP Hardware Path Report

- mode: rk-rkmpp
- ffmpeg_available: yes
- decoder: h264_rkmpp
- decoder_seen: no
- input: (not provided)
- decode_command_ok: no

## Boundary
This RK path does not call VM vim2m and does not force V4L2 M2M codec ioctls onto ISP/camera nodes.
Hardware proof should come from RKMPP/FFmpeg logs, decoder listing, dmesg and board device evidence.

## Evidence Files
- uname.txt
- device_nodes.txt
- v4l2_list_devices.txt
- ffmpeg_decoders.txt
- dmesg_media_hints.txt
- ffmpeg_rkmpp_decode.log

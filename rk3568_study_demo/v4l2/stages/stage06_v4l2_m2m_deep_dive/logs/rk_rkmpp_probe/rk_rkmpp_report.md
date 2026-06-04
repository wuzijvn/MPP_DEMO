# RKMPP Hardware Path Report

- ffmpeg_available: yes
- decoder: h264_rkmpp
- decoder_seen: yes
- input: (not provided)
- decode_command_ok: no

## Evidence Files
- uname.txt
- device_nodes.txt
- ffmpeg_decoders.txt
- dmesg_media_hints.txt
- ffmpeg_rkmpp_decode.log

## Driver Shadow Line
RKMPP path may use vendor user-space middleware and kernel/media/VPU drivers. Do not infer V4L2 M2M codec support from vim2m or ISP nodes.

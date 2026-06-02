#include <stdio.h>

/*
 * demo07：benchmark 命令模板。
 *
 * 这里输出的是“可复制命令 + 观测点提醒”，用于规范测试口径：
 * 1) 先跑软件基线；
 * 2) 再跑硬件路径；
 * 3) 用 debug 日志确认是否发生 fallback。
 */
int main() {
    printf("[demo07] benchmark command notebook\n");
    printf("[demo07] SW baseline example:\n");
    printf("  ffmpeg -benchmark -i input.mp4 -f null -\n");

    printf("[demo07] RKMPP HW decode example for RK3568:\n");
    printf("  ffmpeg -benchmark -c:v h264_rkmpp -i input.mp4 -f null -\n");

    printf("[demo07] explicit hwdevice experiment only when backend is known usable:\n");
    printf("  ffmpeg -benchmark -hwaccel drm -i input.mp4 -f null -\n");

    printf("[demo07] check fallback with debug logs:\n");
    printf("  ffmpeg -v debug ...\n");

    printf("[demo07] observation focus: fps/cpu + frame type evidence + backend negotiation logs\n");
    printf("[demo07] PASS\n");
    return 0;
}

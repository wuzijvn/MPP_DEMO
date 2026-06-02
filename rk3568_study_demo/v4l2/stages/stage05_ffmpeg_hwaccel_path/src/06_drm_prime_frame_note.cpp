#include <stdio.h>

/*
 * demo06：DRM PRIME 概念说明（可执行笔记）。
 *
 * 本 demo 不直接调用 DRM API，而是明确学习边界：
 * 1) 硬件帧有机会通过 dma-buf / drm prime fd 跨组件共享；
 * 2) 仅写 -hwaccel 参数并不能证明零拷贝成立；
 * 3) 必须有 frame 格式、导入路径、同步链路的证据。
 */
int main() {
    printf("[demo06] DRM PRIME note\n");
    printf("[demo06] hw frame can be exported as dma-buf/drm prime fd in specific backend path.\n");
    printf("[demo06] zero-copy is not proven by command options alone; check frame format + import path evidence.\n");
    printf("[demo06] driver_shadow: verify /dev/dri/renderD* and dmesg(drm/iommu/dma) when PRIME import/export fails.\n");
    printf("[demo06] PASS\n");
    return 0;
}

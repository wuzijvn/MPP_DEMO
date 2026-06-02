#include <stdio.h>

/*
 * demo08：硬件回退检测清单。
 *
 * 目的：避免“命令看起来像硬解，但实际上走软件”的误判。
 */
int main() {
    printf("[demo08] fallback detection checklist\n");
    printf("[demo08] 1. confirm selected decoder, e.g. h264_rkmpp/hevc_rkmpp on RK3568\n");
    printf("[demo08] 2. confirm packets and frames are produced without software decoder fallback\n");
    printf("[demo08] 3. explicit hwdevice mode only: confirm hw device and hw pix fmt\n");
    printf("[demo08] 4. confirm no hidden transfer-to-cpu before sink when claiming zero-copy\n");
    printf("[demo08] 5. collect dmesg/backend logs\n");
    printf("[demo08] 6. compare with software baseline to isolate framework vs driver issues\n");
    printf("[demo08] PASS\n");
    return 0;
}

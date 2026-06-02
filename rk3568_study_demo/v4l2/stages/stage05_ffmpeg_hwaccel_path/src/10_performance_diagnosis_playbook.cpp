#include <cstdio>

int main() {
    /*
     * 这个 demo 不是做解码，而是告诉学习者“性能优化从哪里下手”。
     * 核心目标：先拿到可复现证据，再决定是否做零拷贝/RGA/线程流水线改造。
     */
    printf("[demo10] performance diagnosis playbook\n");
    printf("[demo10] step1 baseline: compare decoder=h264 vs decoder=h264_rkmpp\n");
    printf("[demo10] step2 evidence: collect verdict + frame counters from demo04 summary\n");
    printf("[demo10] step3 timing: collect real/user/sys/cpu_pct/maxrss by /usr/bin/time\n");
    printf("[demo10] step4 kernel shadow: collect dmesg media snapshot for each run\n");
    printf("[demo10] step5 ranking: optimize only after bottleneck is proven\n");
    printf("[demo10] PASS\n");
    return 0;
}

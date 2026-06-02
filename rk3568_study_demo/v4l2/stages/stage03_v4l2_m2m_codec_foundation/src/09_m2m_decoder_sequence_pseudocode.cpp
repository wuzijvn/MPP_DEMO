#include <stdio.h>

/*
 * 函数作用：09 demo 入口。
 * 知识边界：
 * - 该文件是“可运行伪代码打印器”，只训练完整状态机顺序记忆；
 * - 不执行真实 ioctl。
 */
int main() {
    printf("[demo09] m2m decoder sequence pseudocode (runnable printer)\n");
    printf("open -> QUERYCAP -> ENUM_FMT -> S_FMT(OUTPUT/CAPTURE)\n");
    printf("REQBUFS -> QUERYBUF -> MMAP -> QBUF(CAPTURE/OUTPUT)\n");
    printf("STREAMON both -> poll -> DQBUF/QBUF loop\n");
    printf("handle SOURCE_CHANGE -> CAPTURE reconfigure\n");
    printf("handle EOS/drain -> STREAMOFF -> munmap -> close\n");
    printf("[demo09] PASS\n");
    return 0;
}

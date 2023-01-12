#include "printk.h"
#include "sbi.h"
#include "defs.h"
#include "proc.h"

extern void test();

int start_kernel()
{
    printk("\x1b[0;36m[S] 2022 Hello RISCV!\x1b[0;37m\n");

    schedule();
    test(); // DO NOT DELETE !!!

    return 0;
}

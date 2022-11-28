#include "printk.h"
#include "sbi.h"
#include "defs.h"
#include "proc.h"

extern void test();

int start_kernel()
{
    printk("\x1b[0;36m[S-Mode] 2022 Hello RISCV!\x1b[0;37m\n");

    // TEST ONLY (in lab3)
    // get the size of members in `task_struct`
    // printk("%d %d\n",sizeof(struct thread_info*), sizeof(uint64));

    // DEBUG:
    // sbi_ecall(0x1, 0x0, 0x30, 0, 0, 0, 0, 0);

    // DEBUG (ONLY used in lab1):
    // puts("read_csr (sstatus) test: ");
    // puti(csr_read(sstatus));
    // puts("\n");

    // DEBUG (ONLY used in lab1):
    // puts("write_csr (sscratch) test: \n");
    // puti(csr_read(sscratch));
    // puts("\n");
    // csr_write(sscratch, 114514);
    // puti(csr_read(sscratch));
    // puts("\n");

    schedule();

    test(); // DO NOT DELETE !!!

    return 0;
}

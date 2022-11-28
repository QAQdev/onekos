// trap.c
#include "printk.h"
#include "clock.h"
#include "proc.h"
#include "syscall.h"

void u_mode_call_handler(struct pt_regs *regs)
{

    uint64 *a0 = &regs->x[10];
    uint64 a1 = regs->x[11];
    uint64 a2 = regs->x[12];
    uint64 a7 = regs->x[17];

    // printk("%d\n",a7);

    switch (a7)
    {
    case SYS_WRITE:
        sys_write(*a0, a1, a2);
        break;
    case SYS_GETPID:
        *a0 = sys_getpid();
        break;
    default:
        break;
    }

    regs->sepc += (uint64)0x4; // set pc to next instruction
}

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs)
{
    // 通过 `scause` 判断trap类型
    // 如果是 interrupt 判断是否是 timer interrupt
    // 如果是 timer interrupt 则打印输出相关信息, 并通过 `clock_set_next_event()` 设置下一次时钟中断
    // `clock_set_next_event()` 见 4.5 节
    // 其他 interrupt / exception 可以直接忽略

    switch (scause)
    {
    case 0x8000000000000005: // S-timer interrupt
        // printk("[S] Supervisor Mode Timer Interrupt\n");
        clock_set_next_event();
        do_timer();
        break;
    case 0:
        printk("Instruction address misaligned\n");
        break;
    case 1:
        printk("Instruction access fault\n");
        break;
    case 3:
        printk("Breakpoint\n");
        break;
    case 4:
        printk("Load address misaligned\n");
        break;
    case 5:
        printk("Load access fault\n");
        break;
    case 6:
        printk("Store/AMO address misaligned\n");
        break;
    case 7:
        printk("Store/AMO access fault\n");
        break;
    case 8:
        // printk("Environment call from U-mode\n");
        u_mode_call_handler(regs);
        break;
    case 9:
        printk("Environment call from S-mode\n");
        break;
    case 11:
        printk("Environment call from M-mode\n");
        break;
    case 12:
        printk("Instruction page fault\n");
        while (1)
            ;
        break;
    case 13:
        printk("Load page fault\n");
        while (1)
            ;
        break;
    case 15:
        printk("Store/AMO page fault\n");
        while (1)
            ;
        break;
    default:
        // printk("Unhandled exception\n");
        // while (1)
        // {
        //     /* code */
        // }

        break;
    }
}

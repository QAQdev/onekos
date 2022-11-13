// trap.c
#include "printk.h"
#include "clock.h"
#include "proc.h"

void trap_handler(unsigned long scause, unsigned long sepc)
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
    case 0x0:
        printk("Instruction address misaligned\n");
        break;
    case 0x1:
        printk("Instruction access fault\n");
        break;
    case 0x3:
        printk("Breakpoint\n");
        break;
    case 0x4:
        printk("Load address misaligned\n");
        break;
    case 0x5:
        printk("Load access fault\n");
        break;
    case 0x6:
        printk("Store/AMO address misaligned\n");
        break;
    case 0x7:
        printk("Store/AMO access fault\n");
        break;
    case 0x8:
        printk("Environment call from U-mode\n");
        break;
    case 0x9:
        printk("Environment call from S-mode\n");
        break;
    case 0x11:
        printk("Environment call from M-mode\n");
        break;
    case 0x12:
        printk("Instruction page fault\n");
        break;
    case 0x13:
        printk("Load page fault\n");
        break;
    case 0x15:
        printk("Store/AMO page fault\n");
        break;
    default:
        break;
    }
}

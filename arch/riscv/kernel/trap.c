// trap.c
#include "printk.h"
#include "clock.h"
#include "defs.h"
#include "proc.h"
#include "mm.h"
#include "syscall.h"
#include "string.h"

extern struct task_struct *current;
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
extern void uapp_start(), uapp_end();

static void u_mode_call_handler(struct pt_regs *regs)
{

    uint64 *a0 = &regs->x[10];
    uint64 a1 = regs->x[11];
    uint64 a2 = regs->x[12];
    uint64 a7 = regs->x[17];

    switch (a7)
    {
    case SYS_WRITE:
        sys_write(*a0, a1, a2);
        break;
    case SYS_GETPID:
        *a0 = sys_getpid();
        break;
    case SYS_CLONE:
        *a0 = sys_clone(regs);
        break;
    default:
        break;
    }

    regs->sepc += (uint64)0x4; // set pc to next instruction
}

// add in lab6
void do_page_fault(struct pt_regs *regs)
{
    /*
     1. 通过 stval 获得访问出错的虚拟内存地址（Bad Address）
     2. 通过 find_vma() 查找 Bad Address 是否在某个 vma 中
     3. 分配一个页，将这个页映射到对应的用户地址空间
     4. 通过 (vma->vm_flags | VM_ANONYM) 获得当前的 VMA 是否是匿名空间
     5. 根据 VMA 匿名与否决定将新的页清零或是拷贝 uapp 中的内容
    */

    printk("[S] Supervisor Page Fault, scause: %lx, stval: %lx, sepc: %lx\n", csr_read(scause), csr_read(stval), csr_read(sepc));

    uint64_t bad_addr = csr_read(stval);
    struct vm_area_struct *p_vma = find_vma(current, bad_addr);

    if (p_vma != NULL)
    {
        uint64_t page = alloc_page();
        p_vma->alloc_flag = 1;

        if (!(p_vma->vm_flags & VM_ANONYM)) // not anonymous, need to copy
        {
            uint64_t load_addr = (uint64_t)uapp_start + p_vma->vm_content_offset_in_file;
            uint64_t load_size, ofs;
            // within single page
            if (bad_addr - p_vma->vm_start < PGSIZE)
            {
                ofs = bad_addr - PGROUNDDOWN(bad_addr);
                load_size = PGSIZE - ofs < p_vma->vm_end - p_vma->vm_start ? PGSIZE - ofs : p_vma->vm_end - p_vma->vm_start;
                memcpy((void *)(page + ofs), (void *)load_addr, load_size);
            }
            // over single page
            else
            {
                ofs = bad_addr - p_vma->vm_start;
                load_size = PGSIZE < p_vma->vm_end - bad_addr ? PGSIZE : p_vma->vm_end - bad_addr;
                memcpy((void *)page, (void *)(load_addr + ofs), load_size);
            }
            // do mapping
            create_mapping(((uint64_t *)((uint64_t)current->pgd + PA2VA_OFFSET)),
                           bad_addr,
                           page - PA2VA_OFFSET,
                           PGSIZE,
                           (p_vma->vm_flags) | 0b10001);
        }
        else
        {
            // do mapping
            create_mapping(((uint64_t *)((uint64_t)current->pgd + PA2VA_OFFSET)),
                           bad_addr,
                           page - PA2VA_OFFSET,
                           PGSIZE,
                           (p_vma->vm_flags) | 0b10001);
        }
    }
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
        u_mode_call_handler(regs);
        break;
    case 9:
        printk("Environment call from S-mode\n");
        break;
    case 11:
        printk("Environment call from M-mode\n");
        break;
    case 12:
        // printk("Instruction page fault\n");
        do_page_fault(regs);
        break;
    case 13:
        // printk("Load page fault\n");
        do_page_fault(regs);
        break;
    case 15:
        // printk("Store/AMO page fault\n");
        do_page_fault(regs);
        break;
    default:
        printk("Unhandled exception\n");
        while (1)
            ;

        break;
    }
}

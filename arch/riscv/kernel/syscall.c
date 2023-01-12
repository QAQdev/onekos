#include "syscall.h"
#include "stdint.h"
#include "string.h"
#include "defs.h"
#include "printk.h"
#include "mm.h"

extern struct task_struct *current;
extern void putc(char c);

int sys_write(unsigned int fd, const char *buf, size_t count)
{
    int write_size = 0;
    switch (fd)
    {
    case 1: // stdout
        for (int i = 0; i < count; i++)
        {
            putc(buf[i]);
            write_size++;
        }
        break;

    default:
        break;
    }

    return write_size;
}

uint64_t sys_getpid()
{
    return current->pid;
}

extern struct task_struct *task[];
extern uint64_t __ret_from_fork;
extern unsigned long swapper_pg_dir[512];
extern void create_mapping(uint64_t *pgtbl, uint64_t va, uint64_t pa, uint64_t sz, int perm);

static int find_empty_process()
{
    // find the first empty task
    int child_pid = 1;
    while (task[child_pid] && child_pid < NR_TASKS)
    {
        child_pid++;
    }
    return child_pid;
}

uint64_t sys_clone(struct pt_regs *regs)
{
    /*
    1. 参考 task_init 创建一个新的 task, 将的 parent task 的整个页复制到新创建的
    task_struct 页上(这一步复制了哪些东西?）。将 thread.ra 设置为
    __ret_from_fork, 并正确设置 thread.sp
    (仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推)

    2. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
    并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)
    */

    int child_pid = find_empty_process(); // get empty PCB

    uint64_t child_addr = alloc_page(); // child PCB
    task[child_pid] = (struct task_struct *)child_addr;
    memcpy((void *)task[child_pid], (void *)current, PGSIZE); // copy parent's PCB to child's PCB
    task[child_pid]->pid = child_pid;
    task[child_pid]->thread.ra = (uint64_t)(&__ret_from_fork); // set return adddress to `__ret_from_fork`

    uint64_t pt_regs_ofs = (uint64_t)regs - PGROUNDDOWN((uint64_t)regs);
    struct pt_regs *child_regs = (struct pt_regs *)(child_addr + pt_regs_ofs); // find `pt_regs` start
    task[child_pid]->thread.sp = (uint64_t)child_regs;
    task[child_pid]->thread.sscratch = regs->sscratch;

    child_regs->x[10] = 0;                         // child `fork()` return 0
    child_regs->x[2] = task[child_pid]->thread.sp; // child sp should be changed
    child_regs->sepc = regs->sepc + (uint64_t)0x4; // next instruction after `fork()`
    child_regs->sscratch = regs->sscratch;         // set sscratch

    /*
     3. 为 child task 申请 user stack, 并将 parent task 的 user stack
     数据复制到其中。
         3.1. 同时将子 task 的 user stack 的地址保存在 thread_info->
         user_sp 中，如果你已经去掉了 thread_info，那么无需执行这一步

    */

    uint64_t child_stack = alloc_page();
    uint64_t stack_ofs = regs->sscratch - PGROUNDDOWN(regs->sscratch);
    memcpy((void *)(child_stack + stack_ofs), (void *)regs->sscratch, USER_END - regs->sscratch);

    /*
    4. 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射
    */

    uint64_t *child_page_tbl = (pagetable_t)alloc_page();
    task[child_pid]->pgd = (uint64_t)child_page_tbl - PA2VA_OFFSET; // child page table
    // copy kernel page table
    for (int i = 0; i < PGTBL_ENTRIES; i++)
    {
        child_page_tbl[i] = swapper_pg_dir[i];
    }

    // do user stack mapping
    create_mapping(
        child_page_tbl,
        USER_END - PGSIZE,
        child_stack - PA2VA_OFFSET,
        PGSIZE,
        0b10111);

    /*
    5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存

    6. 返回子 task 的 pid
    */

    struct vm_area_struct *vma = NULL;
    for (uint64_t i = 0; i < current->vma_cnt; i++)
    {
        vma = &(current->vmas[i]);
        // copy those vma that has been allocated
        if (vma->alloc_flag)
        {
            for (uint64_t copy_addr = vma->vm_start; copy_addr < vma->vm_end; copy_addr += PGSIZE)
            {
                uint64_t child_page = alloc_page();
                memcpy((void *)child_page, (void *)PGROUNDDOWN(copy_addr), PGSIZE);
                create_mapping(
                    child_page_tbl,
                    PGROUNDDOWN(copy_addr),
                    (uint64_t)child_page - PA2VA_OFFSET,
                    PGSIZE,
                    vma->vm_flags | 0b10001);
            }
        }
    }

    printk("\x1b[0;33m[S] New task: %d\x1b[0;37m\n", child_pid);
    return child_pid;
}
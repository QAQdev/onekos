#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "defs.h"
#include "printk.h"
#include "string.h"
#include "elf.h"

extern void __dummy();

// add in lab5
extern unsigned long swapper_pg_dir[512];
extern void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
extern void uapp_start(), uapp_end();

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

// add in lab5, but replaced by `load_program` to enable elf
static uint64 *setup_user_pgtbl()
{
    uint64 *u_pgtbl = (uint64 *)alloc_page();

    // user root page table contains every kernel root page table entry
    for (int i = 0; i < PGTBL_ENTRIES; i++)
    {
        u_pgtbl[i] = swapper_pg_dir[i];
    }

    // number of pages needed to copy uapp
    uint64 uapp_pages = PGROUNDUP((uint64)uapp_end - (uint64)uapp_start) / PGSIZE;
    uint64 uapp_start_cpy = alloc_pages(uapp_pages);
    uint64 uapp_span = (uint64)uapp_end - (uint64)uapp_start;

    // do copy
    memcpy((uint64 *)uapp_start_cpy, uapp_start, uapp_span);

    // map user stack
    create_mapping(u_pgtbl,
                   USER_END - PGSIZE,
                   (uint64)alloc_page() - PA2VA_OFFSET, // user stack
                   PGSIZE,
                   0b1011);

    // map user space
    create_mapping(u_pgtbl,
                   USER_START,
                   uapp_start_cpy - PA2VA_OFFSET,
                   uapp_span,
                   0b1111);

    return u_pgtbl;
}

static uint64_t load_program(struct task_struct *task)
{

    uint64 *u_pgtbl = (uint64 *)alloc_page();
    // user root page table contains every kernel root page table entry
    for (int i = 0; i < PGTBL_ENTRIES; i++)
    {
        u_pgtbl[i] = swapper_pg_dir[i];
    }

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)uapp_start;

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr *phdr;
    int load_phdr_cnt = 0;

    uint64 pa = USER_START;
    for (int i = 0; i < phdr_cnt; i++)
    {
        phdr = (Elf64_Phdr *)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD)
        {
            // number of pages the segment need to have
            uint64 uapp_pages = PGROUNDUP(phdr->p_memsz) / PGSIZE;
            uint64 uapp_start_cpy = alloc_pages(uapp_pages);                      // allocate pages
            uint64 load_addr = (uint64)uapp_start + phdr->p_offset;               // find load address
            memcpy((uint64 *)uapp_start_cpy, (uint64 *)load_addr, phdr->p_memsz); // do copys
            create_mapping(u_pgtbl,
                           USER_START,
                           uapp_start_cpy - PA2VA_OFFSET,
                           phdr->p_memsz,
                           phdr->p_flags | 0b1000);
        }
    }

    // allocate user stack and do mapping
    // map user stack
    create_mapping(u_pgtbl,
                   USER_END - PGSIZE,
                   (uint64)alloc_page() - PA2VA_OFFSET, // user stack
                   PGSIZE,
                   0b1011);

    // following code has been written for you
    // set user stack
    // task->thread_info->user_sp = USER_END;
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    csr_read(sstatus);
    // SUM: 18, SPP: 8, SPIE: 5
    task->thread.sstatus |= 0x00040020; // set SPIE and SUM
    // user stack for user program
    task->thread.sscratch = USER_END;

    printk("!\n");

    return (uint64)u_pgtbl;
}

void task_init()
{
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    idle = (struct task_struct *)kalloc(); // allocate a physical page
    idle->state = TASK_RUNNING;
    idle->counter = idle->priority = 0;
    idle->pid = 0;
    current = task[0] = idle;

    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, counter 为 0, priority 使用 rand() 来设置, pid 为该 线程在线程数组中的下标。
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    for (uint64 i = 1; i < NR_TASKS; i++)
    {
        task[i] = (struct task_struct *)kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand();
        task[i]->pid = i;

        // return address is set to _dummy
        task[i]->thread.ra = (uint64)__dummy;
        // stack pointer is set to high address
        task[i]->thread.sp = (uint64)task[i] + PGSIZE;

        /* add in lab5 */
        // create user page table
        // task[i]->pgd = (uint64 *)((uint64)setup_user_pgtbl() - PA2VA_OFFSET); // 这个很逆天。。。。。。指针+数字类似下标访问

        // // printk("%lx\n",task[i]->pgd);

        // task[i]->thread.sepc = USER_START;

        // task[i]->thread.sstatus = csr_read(sstatus);
        // // SUM: 18, SPP: 8, SPIE: 5
        // task[i]->thread.sstatus |= 0x00040020; // set SPIE and SUM
        // task[i]->thread.sscratch = USER_END;   // set sscratch

        printk("load_program begins\n");

        task[i]->pgd = (uint64 *)(load_program(task[i]) - PA2VA_OFFSET);
    }

    printk("...proc_init done!\n");
}

void dummy()
{
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while (1)
    {
        if (last_counter == -1 || current->counter != last_counter)
        {
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            // printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            printk("[PID = %d] is running, thread space begin at %lx\n", current->pid, current);
        }
    }
}

extern void __switch_to(struct task_struct *prev, struct task_struct *next);

void switch_to(struct task_struct *next)
{
    if (next == current)
    {
        return;
    }
    else
    {
        printk("\x1b[0;34m[PID = %d PRIORITY = %d COUNTER = %d] switches to [PID = %d PRIORITY = %d COUNTER = %d]\x1b[0;37m\n",
               current->pid, current->priority, current->counter,
               next->pid, next->priority, next->counter);

        struct task_struct *temp = current;
        current = next; // update `current`
        __switch_to(temp, next);
    }
}

void do_timer(void)
{
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行 调度

    if (current == idle)
    {
        printk("\x1b[0;31midle is running and prepares to schedule!\x1b[0;37m\n");
        schedule();
    }
    else
    {
        current->counter--;
        // printk("[PID = %d PRIORITY = %d COUNTER = %d]\n", current->pid, current->priority, current->counter);
        if (current->counter <= 0)
        {
            printk("\x1b[0;31m[PID = %d] ends and prepares to schedule!\x1b[0;37m\n", current->pid);
            schedule();
        }
    }
}

void reallocate()
{
    // check if all tasks are at their end
    int all_zero = 1;
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->counter > 0)
        {
            all_zero = 0;
            break;
        }
    }

    if (all_zero == 1)
    {
        printk("\x1b[0;33mall tasks' counter are 0, reallocate...\x1b[0;37m\n");
        for (int i = 1; i < NR_TASKS; i++)
        {
            task[i]->counter = rand();
            printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", i, task[i]->priority, task[i]->counter);
        }
        printk("\x1b[0;33mreallocation done...\x1b[0;37m\n");
    }
}

#ifdef SJF

void schedule(void)
{

    reallocate();

    struct task_struct *next = task[1];
    uint64 min = 0x7fffffff;

    // find the task has minimum running time
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0 && task[i]->counter < min)
        {
            next = task[i];
            min = task[i]->counter;
        }
    }

    switch_to(next);
}

#endif

#ifdef PRIORITY

void schedule(void)
{
    reallocate();

    struct task_struct *next = task[1];
    uint64 max = 0;

    // find the task has highest priority
    for (int i = 1; i < NR_TASKS; i++)
    {
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0 && task[i]->priority >= max)
        {
            // when priority is same, choose the one having less run time
            if (task[i]->priority == max)
            {
                next = (task[i]->counter < next->counter) ? task[i] : next;
                max = (task[i]->counter < next->counter) ? task[i]->priority : next->priority;
            }
            else
            {
                next = task[i];
                max = task[i]->priority;
            }
        }
    }

    switch_to(next);
}

#endif
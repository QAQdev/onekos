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
            uint64_t pages = ((phdr->p_vaddr - PGROUNDDOWN(phdr->p_vaddr)) + phdr->p_memsz - 1) / PGSIZE + 1;
            do_mmap(
                task,
                phdr->p_vaddr,
                pages * PGSIZE,
                phdr->p_flags << 1 | VM_X_MASK,
                phdr->p_offset,
                phdr->p_filesz);
        }
    }

    do_mmap(
        task,
        USER_END - PGSIZE,
        PGSIZE,
        VM_R_MASK | VM_W_MASK | VM_ANONYM,
        0,
        0);

    // following code has been written for you
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    csr_read(sstatus);
    // SUM: 18, SPP: 8, SPIE: 5
    task->thread.sstatus |= 0x00040020; // set SPIE and SUM
    // user stack for user program
    task->thread.sscratch = USER_END;

    return (uint64)u_pgtbl;
}

// add in lab6
void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
             uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file)
{
    // code
    struct vm_area_struct *vma = &(task->vmas[task->vma_cnt]);
    task->vma_cnt++;

    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
    vma->vm_flags = flags;
    vma->alloc_flag = 0;
}

// add in lab6
struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr)
{

    for (uint64_t i = 0; i < task->vma_cnt; i++)
    {
        if (addr >= task->vmas[i].vm_start && addr < task->vmas[i].vm_end)
        {
            return &task->vmas[i];
        }
    }

    return NULL;
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

    // modified in lab6, only initialize 1 task

    // for (int i = 1; i < NR_TASKS; i++)
    // {
    //     task[i] = (struct task_struct *)kalloc();
    //     task[i]->state = TASK_RUNNING;
    //     task[i]->counter = 0;
    //     task[i]->priority = rand();
    //     task[i]->pid = i;

    //     // return address is set to _dummy
    //     task[i]->thread.ra = (uint64)__dummy;
    //     // stack pointer is set to high address
    //     task[i]->thread.sp = (uint64)task[i] + PGSIZE;
    //     // lab6
    //     task[i]->pgd = (uint64 *)(load_program(task[i]) - PA2VA_OFFSET);
    // }

    task[1] = (struct task_struct *)kalloc();
    task[1]->state = TASK_RUNNING;
    task[1]->counter = 0;
    task[1]->priority = rand();
    task[1]->pid = 1;

    // return address is set to _dummy
    task[1]->thread.ra = (uint64)__dummy;
    // stack pointer is set to high address
    task[1]->thread.sp = (uint64)task[1] + PGSIZE;
    // lab6
    task[1]->pgd = (uint64 *)(load_program(task[1]) - PA2VA_OFFSET);

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
        if (task[i] != NULL && task[i]->counter > 0)
        {
            all_zero = 0;
            break;
        }
    }

    if (all_zero == 1)
    {
        // printk("\x1b[0;33mall tasks' counter are 0, reallocate...\x1b[0;37m\n");
        for (int i = 1; i < NR_TASKS; i++)
        {
            if (task[i] == NULL)
            {
                continue;
            }
            task[i]->counter = rand() % 7 + 1;
            printk("\x1b[0;33mSET [PID = %d PRIORITY = %d COUNTER = %d]\x1b[0;37m\n", i, task[i]->priority, task[i]->counter);
        }
        // printk("\x1b[0;33mreallocation done...\x1b[0;37m\n");
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
        if (task[i] != NULL && task[i]->state == TASK_RUNNING && task[i]->counter > 0 && task[i]->counter < min)
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
        if (task[i] != NULL && task[i]->state == TASK_RUNNING && task[i]->counter > 0 && task[i]->priority >= max)
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
#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "defs.h"
#include "printk.h"

extern void __dummy();

struct task_struct *idle;           // idle process
struct task_struct *current;        // 指向当前运行线程的 `task_struct`
struct task_struct *task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

void task_init()
{
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    printk("proc_init start...\n");

    idle = (struct task_struct *)kalloc(); // allocate a physical page
    idle->state = TASK_RUNNING;
    idle->counter = idle->priority = 0;
    idle->pid = 0;
    current = task[0] = idle;

    printk("idle init done...\n");

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
        printk("[PID = %d PRIORITY = %d COUNTER = %d]\n", current->pid, current->priority, current->counter);
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
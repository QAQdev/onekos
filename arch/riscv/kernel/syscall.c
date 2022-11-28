#include "syscall.h"

#include "stddef.h"
#include "defs.h"
#include "proc.h"
#include "printk.h"

extern struct task_struct *current;
void putc(char c);

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
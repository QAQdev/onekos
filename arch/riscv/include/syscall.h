#pragma once

#include "stdint.h"
#include "stddef.h"
#include "proc.h"

#define SYS_WRITE 64
#define SYS_GETPID 172

#define SYS_MUNMAP 215
#define SYS_CLONE 220 // fork
#define SYS_MMAP 222
#define SYS_MPROTECT 226

int sys_write(unsigned int fd, const char *buf, size_t count);

uint64_t sys_getpid();

uint64_t sys_clone(struct pt_regs *regs);
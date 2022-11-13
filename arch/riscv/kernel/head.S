    .extern start_kernel
    .extern task_init
    .extern setup_vm
    .extern mm_init
    .extern setup_vm_final
    .extern early_pgtbl
    #include "defs.h"

    .section .text.init         # 将 _start 放入.text.init section
    .globl _start
_start:
    # ------------------
    # - your code here -
    # ------------------

    # lab5: because the symbols in System.map is already set to virtual address,
    # but here we have not set up vm yet, so minus PA2VA_OFFSET is necessary
    la t1, boot_stack_top
    li t2, PA2VA_OFFSET
    sub t1, t1, t2
    mv sp, t1 # lab1: set stack pointer to the right place!!!
 
    call setup_vm # lab4
    call relocate # lab4

    call mm_init # lab3: initialize memory management!!!
    call setup_vm_final # lab4
    call task_init # lab3: initialize tasks

    la t1, _traps # lab2: load the address of `_traps`
    csrw stvec, t1 # lab2: write the address of `_traps` into stvec, DIRECT mode
    li t1, 32 # bitmask
    csrs sie, t1 # lab2: set sie[STIE] = 1, enable clock interrupt

    # lab2: set the first timer interrupt
    rdtime t1
    li t2, 10000000 # load TIMECLOCK: 10000000
    add t1, t1, t2 # t1 <- t1 + t2

    # arguments passing
    mv a0, t1
	mv a1, x0
	mv a2, x0
	mv a3, x0
	mv a4, x0
	mv a5, x0
    mv a7, x0
    mv a6, x0
    ecall
    
    li t1, 2
    csrs sstatus, t1 # lab2: set sstatus[SIE] = 1 

    call start_kernel # lab1: call `start_kernel`

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)

    ###################### 
    #   YOUR CODE HERE   #
    ######################

    li t0, PA2VA_OFFSET # PA2VA_OFFSET
    add ra, ra, t0 # set ra = ra + PA2VA_OFFSET
    add sp, sp, t0 # set sp = sp + PA2VA_OFFSET

    # set satp with early_pgtbl

    la t1, early_pgtbl
    sub t1, t1, t0 # early_pgtbl - PA2VA_OFFSET
    srli t1, t1, 12 # PPN = PA >> 12

    # set `satp`
    li t2,  0x8000000000000000; # MODE:Sv39(8) ASID: 0 PPN: PPN of early_pgtbl
    or t2, t1, t2
    csrw satp, t2

    ###################### 
    #   YOUR CODE HERE   #
    ######################
    
    # flush tlb
    sfence.vma zero, zero

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    # 4KB
    .space  0x1000 # <-- change to your stack size

    .globl boot_stack_top
boot_stack_top:
#include "types.h"
#include "defs.h"
#include "mm.h"
#include "printk.h"
#include <string.h>

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
unsigned long early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void)
{
    /*
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间 9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */

    // OPENSBI -> early_pgtbl[2], XWRV -> 1111
    // identical mapping
    early_pgtbl[PHY_START >> 30] = ADD_PTE(PHY_START, 0xf);
    // VM_START -> early_pgtbl[384], XWRV -> 1111
    // linear mapping
    early_pgtbl[(VM_START >> 30) & 0x1FF] = ADD_PTE(PHY_START, 0xf);

    printk("[DEBUG]setup_vm done\n");
}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
unsigned long swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));

/* 创建多级页表映射关系 */
void create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    /*
    pgtbl 为根页表的基地址
    va, pa 为需要映射的虚拟地址、物理地址
    sz 为映射的大小
    perm 为映射的读写权限

    创建多级页表的时候可以使用 kalloc() 来获取一页作为页表目录
    可以使用 V bit 来判断页表项是否存在
    */

    uint64 *cur_pte, *cur_ptes;
    int vpn2, vpn1, vpn0;

    for (uint64 cur_va = va, cur_pa = pa; cur_va <= va + sz; cur_pa += PGSIZE, cur_va += PGSIZE)
    {
        cur_pte = NULL;
        cur_ptes = pgtbl;

        // get vpn[2], vpn[1], vpn[0]
        vpn2 = (cur_va >> 30) & 0x1ff;
        vpn1 = (cur_va >> 21) & 0x1ff;
        vpn0 = (cur_va >> 12) & 0x1ff;

        // printk("[DEBUG]vpn2: %d, vpn1: %d, vpn0: %d\n", vpn2, vpn1, vpn0);

        // second page table
        cur_pte = cur_ptes + vpn2;
        if ((*cur_pte & 1) == 0) // NOT VALID
        {
            // allocate one index page
            cur_ptes = (uint64 *)kalloc();
            memset(cur_ptes, 0x0, PGSIZE);
            // RWXV -> 0001
            *cur_pte = ADD_PTE((uint64)cur_ptes - PA2VA_OFFSET, (uint64)0x1);
            // printk("[DEBUG]init-second pgtbl: cur_pte: %lx, *cur_pte: %lx", cur_pte, *cur_pte);
        }
        else // VALID
        {
            // find entry for the second page table
            // (cur_pte >> 10) << 12 actually equals to (cur_pte >> 10)*PGSIZE,
            // then add PA2VA_OFFSET to convert it to virtual address of the second page table
            cur_ptes = (uint64 *)(((*cur_pte >> 10) << 12) + PA2VA_OFFSET);
            // printk("[DEBUG]valid-second pgtbl: cur_pte: %lx, *cur_pte: %lx", cur_pte, *cur_pte);
        }
        // leaf page table
        cur_pte = cur_ptes + vpn1;
        if (((*cur_pte) & 1) == 0) // NOT VALID
        {
            // allocate one index page
            cur_ptes = (uint64 *)kalloc();
            memset(cur_ptes, 0x0, PGSIZE);
            // RWXV -> 0001
            *cur_pte = ADD_PTE((uint64)cur_ptes - PA2VA_OFFSET, (uint64)0x1);
            // printk("[DEBUG]init-leaf pgtbl: cur_pte: %lx, *cur_pte: %lx", cur_pte, *cur_pte);
        }
        else // VALID
        {
            // find entry for the leaf page table
            cur_ptes = (uint64 *)(((*cur_pte >> 10) << 12) + PA2VA_OFFSET);
            // printk("[DEBUG]valid-leaf pgtbl: cur_pte: %lx, *cur_pte: %lx", cur_pte, *cur_pte);
        }
        // leaf page table, RWXV -> perm << 1 | 1
        cur_ptes[vpn0] = ADD_PTE(cur_pa, (perm << 1) | 1);
        // printk("[DEBUG]leaf pgtbl: cur_pte: %lx, *cur_pte: %lx", cur_pte, *cur_pte);
    }
}

// kernel section
extern void _stext(), _srodata(), _sdata(), _ebss(), _skernel();

void setup_vm_final(void)
{
    memset(swapper_pg_dir, 0x0, PGSIZE);

    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    // permission: XWRV -> b'1011
    create_mapping(swapper_pg_dir, (uint64)_stext, PHY_START + OPENSBI_SIZE,
                   _srodata - _skernel, 0b101);
    printk("[DEBUG].text section mapped\n");

    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64)_srodata,
                   PHY_START + OPENSBI_SIZE + _srodata - _skernel,
                   _sdata - _srodata, 0b001);
    printk("[DEBUG].rodata section mapped\n");

    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, (uint64)_sdata,
                   (uint64)_sdata - PA2VA_OFFSET,
                   PHY_END + PA2VA_OFFSET - (uint64)_sdata,
                   0b011);
    printk("[DEBUG]other section mapped\n");

    // set satp with swapper_pg_dir
    // physical page number, so need to minus PA2VA_OFFSET
    uint64 ppn = ((uint64)swapper_pg_dir - PA2VA_OFFSET) >> 12; // get PPN of `swapper_pg_dir`

    printk("[DEBUG]swapper_pg_dir PPN: %lx\n", ppn);

    csr_write(satp, ((uint64)0x000fffffffffff & ppn) | (uint64)0x8000000000000000); // set MODE to Sv39
    printk("[DEBUG]satp: %lx\n", csr_read(satp));
    printk("[DEBUG]scause: %lx\n", csr_read(scause));
    // flush TLB
    asm volatile("sfence.vma zero, zero");

    return;
}
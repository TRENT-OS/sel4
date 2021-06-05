/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include <util.h>

#if CONFIG_PT_LEVELS == 3

/*
 * Physical address space layout
 *
 *                                          +-------------+
 *                                          |     not     |
 *                                          |    kernel   |
 *                                          | addressable |
 *       PADDR_TOP (= PPTR_TOP - PPTR_BASE) +-------------+ 254 GiB <-+
 *                                          |             |           |
 * KDEV_BASE - KERNEL_ELF_BASE + PADDR_LOAD +-------------+           |
 *                                          |  Kernel ELF |           | PSpace
 *                    KERNEL_ELF_PADDR_BASE +-------------+           |
 *                                          |             |           |
 *                                          +-------------+ 0  <------+
 *
 * Virtual address space layout
 *
 * The RISC-V SV39 MMU model splits the top level page table in the middle, the
 * entries 0 to 255 addresse the virtual memory from 0 to 256 GiB, the entries
 * 256 to 511 the are from 2^64 - 256 GiB to 2^64 GiB. The common unsage is,
 * that the low addresses range is used for user mapping, the high address range
 * is for kernel mappings. seL4 also follows this concept.
 * The entries 256 - 509 are used to map the physical address space directly
 * with a 1 GiB granularity, so the physical address 0 - 254 GiB is at available
 * to the kernel at the virtual address 0xfffffffc000000000. The remaining 2
 * page table entries 510 and 511 contain a 2nd level page  table reference. The
 * top 1 GiB window is used for kernel device mappings, the 1 GiB below this
 * (starting at KERNEL_ELF_BASE) is the kernel image window, which is used for
 * running the actual kernel.
 * The kernel code segment is supposed to be linked to a 2 MiB aligned address
 * and the kernel size is less then 2 MiB. Thus it can be mapped in a singe
 * 2 MiB page. This allows sharing the same 2nd level page table for the kernel
 * image window and the kernel device mapping. The remaining 511 free entries
 * there can be used for device mappings or other purposes.
 *
 *                              +----------------+ 2^64
 *                              | Kernel Devices |
 *          +-------> KDEV_BASE +----------------+ 2^64 - 1 GiB
 *          |                   |                |
 *          |                   +----------------+
 *          |                   |     Kernel     |
 *        +-+-> KERNEL_ELF_BASE +----------------+ 2^64 - 2 GiB + (KERNEL_ELF_PADDR_BASE % 1GiB)
 * Shared | |                   |                |
 * 1GiB   | +-----------------> +----------------+ 2^64 - 2 GiB
 * table  |                     |     PSpace     |
 * entry  |                     |      with      |
 *        |                     |     direct     |
 *        +-------------------> |     kernel     |
 *                              |    mappings    |
 *                    PPTR_BASE +----------------+ 2^64 - 2^b
 *                              |################|
 *                              : not accessible :
 *                              |################|
 *                     USER_TOP +----------------+ 2^c
 *                              |      User      |
 *                   PADDR_BASE +----------------+ 0
 *
 *  c = one less than number of bits the page tables can translate
 *    = sign extension bit for canonical addresses
 *    = 38 on RV64/SV39
 *    = 47 on RV64/SV48
 *  b = The number of bits used by kernel mapping.
 *    = 38 (half of the 1 level page table) on RV64/SV39
 *    = 39 (entire second level page table) on RV64/SV48
 *
 * Support running the kernel from ROM:
 * - create different mappings for the the kernel code segment and the RAM. This
 *   can easily be achieved when using another 2 MiB page mapping for the kernel
 *   RAM.
 * - can't recycle kernel boot code segment for RAM
 */

/* last accessible virtual address in user space */
#define USER_TOP seL4_UserTop

/* The first physical address to map into the kernel's physical memory
 * window */
#define PADDR_BASE UL_CONST(0x0)

/* The base address in virtual memory to use for the 1:1 physical memory
 * mapping */
#define PPTR_BASE UL_CONST(0xFFFFFFC000000000)

/* Top of the physical memory window */
#define PPTR_TOP UL_CONST(0xFFFFFFFF80000000)

/* The physical memory address to use for mapping the kernel ELF */
/* This represents the physical address that the kernel image will be linked to. This needs to
 * be on a 1gb boundary as we currently require being able to creating a mapping to this address
 * as the largest frame size */
#define KERNEL_ELF_PADDR_BASE (physBase + UL_CONST(0x4000000))

/* The base address in virtual memory to use for the kernel ELF mapping */
#define KERNEL_ELF_BASE (PPTR_TOP + (KERNEL_ELF_PADDR_BASE & MASK(30)))

/* The base address in virtual memory to use for the kernel device
 * mapping region. These are mapped in the kernel page table. */
#define KDEV_BASE UL_CONST(0xFFFFFFFFC0000000)

/* Place the kernel log buffer at the end of the kernel device page table */
#define KS_LOG_PPTR UL_CONST(0XFFFFFFFFFFE00000)

#else
#error Only PT_LEVELS == 3 is supported
#endif

#define LOAD  ld
#define STORE sd

#ifndef __ASSEMBLER__

#include <stdint.h>

static inline uint64_t riscv_read_time(void)
{
    uint64_t n;
    asm volatile(
        "rdtime %0"
        : "=r"(n));
    return n;
}

static inline uint64_t riscv_read_cycle(void)
{
    uint64_t n;
    asm volatile(
        "rdcycle %0"
        : "=r"(n));
    return n;
}

#endif /* __ASSEMBLER__ */


/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

/*
 *
 * Copyright 2016, 2017 Hesham Almatary, Data61/CSIRO <hesham.almatary@data61.csiro.au>
 * Copyright 2015, 2016 Hesham Almatary <heshamelmatary@gmail.com>
 */

#include <config.h>
#include <model/statedata.h>
#include <arch/fastpath/fastpath.h>
#include <arch/kernel/traps.h>
#include <machine/debug.h>
#include <api/syscall.h>
#include <util.h>
#include <arch/machine/hardware.h>

#include <benchmark/benchmark_track.h>
#include <benchmark/benchmark_utilisation.h>

//#include "arch/riscv/print_page_table.h"

//#include <stdio.h>

#define PTES_PER_PT 512

static void print_nibble(char *buf, char x)
{
    x = x & 0xf;

    if (x >= 10)
    {
        *buf = 'A' + (x - 10);
    }
    else
    {
        *buf = '0' + x;
    }
}

static void print_to_hex(char *buf, uint64_t x)
{
    int leadingZeros = 1;

    for (int k = 0; k < 16; k++)
    {
        char n = x >> ((4 * (15 - k))) & 0x0f;

        if (leadingZeros)
        {
            leadingZeros = n == 0;
        }

        if (!leadingZeros)
        {
            print_nibble(buf++, n);
        }
    }

    *buf = 0;
}

static char *print_hex(int width, uint64_t x)
{
    enum {NUM_BUF = 32, BUF_LENGTH = 64};
    static char rawBuf[NUM_BUF][64];
    static unsigned int index = 0;

    index = (index + 1) % NUM_BUF;

    for (int k = 0; k < BUF_LENGTH / 2; ++k) 
    {
        rawBuf[index][k] = '0';
        rawBuf[index][k + (BUF_LENGTH / 2)] = 0;
    }

    print_to_hex(rawBuf[index] + (BUF_LENGTH / 2), x);

    int len = 0;
    while (rawBuf[index][len + (BUF_LENGTH / 2)] != 0)
    {
        ++len;
    }

    return rawBuf[index] + (BUF_LENGTH / 2) - width + len;
}

static int printPageTableIsValid(unsigned long entry)
{
    return (entry & 0x01) != 0;
}

static int printPageTableIsLeaf(unsigned long entry)
{
    return (entry & 0x0e) != 0;
}

static uint64_t printPageTableMapEntryToPhysicalAddress(unsigned long x)
{
    uint64_t ppn2 = (x >> (9 + 9 + 2 + 8)) & ((1ul << 26) - 1);
    uint64_t ppn1 = (x >> (0 + 9 + 2 + 8)) & ((1ul <<  9) - 1);
    uint64_t ppn0 = (x >> (0 + 0 + 2 + 8)) & ((1ul <<  9) - 1);

    return ((ppn2 << 18) + (ppn1 << 9) + ppn0) << 12;
}

static void print_page_entry(unsigned int i, uint64_t x, uint64_t v_addr)
{
    if ((v_addr & (1ull << 38)) != 0)
    {
        //printf("%llx\n", v_addr);
        v_addr = v_addr | ((0x1ffffffull) << 39);
        //printf("%llx\n", v_addr);
    }

    printf("pt[%s] = %s (PPN[2]:%s PPN[1]:%s PPN[0]:%s RSW:%s D:%s A:%s G:%s U:%s XWR:%s V:%s : v_addr:%s -> phys_addr:%s)\n",
        print_hex(3, i),
        print_hex(16, x),
        print_hex(3, (x >> (9 + 9 + 2 + 8)) & ((1ul << 26) - 1)), /* PPN[2] */
        print_hex(3, (x >> (0 + 9 + 2 + 8)) & ((1ul <<  9) - 1)), /* PPN[1] */
        print_hex(3, (x >> (0 + 0 + 2 + 8)) & ((1ul <<  9) - 1)), /* PPN[0] */
        print_hex(1, (x >> (0 + 0 + 0 + 8)) & ((1ul <<  2) - 1)), /* RSW */
        print_hex(1, (x >> (0 + 0 + 0 + 7)) & ((1ul <<  1) - 1)), /* D */
        print_hex(1, (x >> (0 + 0 + 0 + 6)) & ((1ul <<  1) - 1)), /* A */
        print_hex(1, (x >> (0 + 0 + 0 + 5)) & ((1ul <<  1) - 1)), /* G */
        print_hex(1, (x >> (0 + 0 + 0 + 4)) & ((1ul <<  1) - 1)), /* U */
        print_hex(1, (x >> (0 + 0 + 0 + 1)) & ((1ul <<  3) - 1)), /* XWR */
        print_hex(1, (x >> (0 + 0 + 0 + 0)) & ((1ul <<  1) - 1)), /* V */
        print_hex(16, v_addr),
        print_hex(16, printPageTableMapEntryToPhysicalAddress(x)));
}

static void printPageTableLevel(unsigned long *pt, uint64_t v_addr, unsigned int level)
{
    printf("\n\npt: %s - level: %s\n", print_hex(16, (uint64_t)pt), print_hex(1, level));

    if (((uint64_t)pt < 0x40000000) || ((uint64_t)pt >= 0x40800000))
    {
        pt = (unsigned long *)(paddr_to_kpptr((paddr_t)pt));
    }
    else
    {
        uint64_t tmp = (uint64_t)pt;
        tmp += 0xFFFFFFC000000000;
        pt = (unsigned long *)tmp;
    }

    printf("pt: %s - level: %s\n", print_hex(16, (uint64_t)pt), print_hex(1, level));

    uint64_t v_addr_step;
    if (level == 1)
    {
        v_addr_step = 1024 * 1024 * 1024;
    }
    else if (level == 2)
    {
        v_addr_step = 2 * 1024 * 1024;
    }
    else
    {
        v_addr_step = 4 * 1024;
    }

    unsigned int i;
#if 0    
    unsigned int leafCount = 0;
#endif    
    for (i = 0; i < PTES_PER_PT; i++)
    {
        unsigned long x = pt[i];
#if 0
        if (printPageTableIsLeaf(x))
        {
            leafCount++;

            if (leafCount > 2)
            {
                if (leafCount == 3)
                {
                    printf("...\n");
                }

                continue;
            }
        }
        else
        {
            leafCount = 0;
        }
#endif        
        //if (printPageTableIsValid(x))
        {
            print_page_entry(i, x, v_addr + i * v_addr_step);
        }
    }

    if (level < 3)
    {
        for (i = 0; i < PTES_PER_PT; i++)
        {
            unsigned long x = pt[i];
            
            if (!printPageTableIsLeaf(x) && printPageTableIsValid(x))
            {
                printPageTableLevel(
                    (unsigned long *)(printPageTableMapEntryToPhysicalAddress(x)),
                    v_addr + i * v_addr_step,
                    level + 1);                
            }
        }
    }
}

static void printPageTablePage(unsigned long *pt)
{
    printPageTableLevel(
        pt,
        0,
        1);                
}



/** DONT_TRANSLATE */
void VISIBLE NORETURN restore_user_context(void)
{
    word_t cur_thread_reg = (word_t) NODE_STATE(ksCurThread)->tcbArch.tcbContext.registers;

    c_exit_hook();

    NODE_UNLOCK_IF_HELD;

    asm volatile(
        "mv t0, %[cur_thread]       \n"
        LOAD_S " ra, (0*%[REGSIZE])(t0)  \n"
        LOAD_S "  sp, (1*%[REGSIZE])(t0)  \n"
        LOAD_S "  gp, (2*%[REGSIZE])(t0)  \n"
        /* skip tp */
        /* skip x5/t0 */
        LOAD_S "  t2, (6*%[REGSIZE])(t0)  \n"
        LOAD_S "  s0, (7*%[REGSIZE])(t0)  \n"
        LOAD_S "  s1, (8*%[REGSIZE])(t0)  \n"
        LOAD_S "  a0, (9*%[REGSIZE])(t0) \n"
        LOAD_S "  a1, (10*%[REGSIZE])(t0) \n"
        LOAD_S "  a2, (11*%[REGSIZE])(t0) \n"
        LOAD_S "  a3, (12*%[REGSIZE])(t0) \n"
        LOAD_S "  a4, (13*%[REGSIZE])(t0) \n"
        LOAD_S "  a5, (14*%[REGSIZE])(t0) \n"
        LOAD_S "  a6, (15*%[REGSIZE])(t0) \n"
        LOAD_S "  a7, (16*%[REGSIZE])(t0) \n"
        LOAD_S "  s2, (17*%[REGSIZE])(t0) \n"
        LOAD_S "  s3, (18*%[REGSIZE])(t0) \n"
        LOAD_S "  s4, (19*%[REGSIZE])(t0) \n"
        LOAD_S "  s5, (20*%[REGSIZE])(t0) \n"
        LOAD_S "  s6, (21*%[REGSIZE])(t0) \n"
        LOAD_S "  s7, (22*%[REGSIZE])(t0) \n"
        LOAD_S "  s8, (23*%[REGSIZE])(t0) \n"
        LOAD_S "  s9, (24*%[REGSIZE])(t0) \n"
        LOAD_S "  s10, (25*%[REGSIZE])(t0)\n"
        LOAD_S "  s11, (26*%[REGSIZE])(t0)\n"
        LOAD_S "  t3, (27*%[REGSIZE])(t0) \n"
        LOAD_S "  t4, (28*%[REGSIZE])(t0) \n"
        LOAD_S "  t5, (29*%[REGSIZE])(t0) \n"
        LOAD_S "  t6, (30*%[REGSIZE])(t0) \n"
        /* Get next restored tp */
        LOAD_S "  t1, (3*%[REGSIZE])(t0)  \n"
        /* get restored tp */
        "add tp, t1, x0  \n"
        /* get sepc */
        LOAD_S "  t1, (34*%[REGSIZE])(t0)\n"
        "csrw sepc, t1  \n"

        /* Write back sscratch with cur_thread_reg to get it back on the next trap entry */
        "csrw sscratch, t0         \n"

        LOAD_S "  t1, (32*%[REGSIZE])(t0) \n"
        "csrw sstatus, t1\n"

        LOAD_S "  t1, (5*%[REGSIZE])(t0) \n"
        LOAD_S "  t0, (4*%[REGSIZE])(t0) \n"
        "sret"
        : /* no output */
        : [REGSIZE] "i"(sizeof(word_t)),
        [cur_thread] "r"(cur_thread_reg)
        : "memory"
    );

    UNREACHABLE();
}

void VISIBLE NORETURN c_handle_interrupt(void)
{
    NODE_LOCK_IRQ;

    c_entry_hook();

    handleInterruptEntry();

    restore_user_context();
    UNREACHABLE();
}

void VISIBLE NORETURN c_handle_exception(void)
{
    NODE_LOCK_SYS;

    c_entry_hook();

    word_t scause = read_scause();
    word_t stval = read_stval();
    word_t satp = read_satp();
    printf("scause: %lx\n", scause);
    printf("stval: %lx\n", stval);
    printf("satp: %lx\n", satp);

    if (scause == 0x0c)
    {
        printPageTablePage((unsigned long *)((satp & 0xfffffffffff) << 12));
    }

    switch (scause) {
    case RISCVInstructionAccessFault:
    case RISCVLoadAccessFault:
    case RISCVStoreAccessFault:
    case RISCVLoadPageFault:
    case RISCVStorePageFault:
    case RISCVInstructionPageFault:
        handleVMFaultEvent(scause);
        break;
    default:
        handleUserLevelFault(scause, 0);
        break;
    }

    restore_user_context();
    UNREACHABLE();
}

void NORETURN slowpath(syscall_t syscall)
{
    /* check for undefined syscall */
    if (unlikely(syscall < SYSCALL_MIN || syscall > SYSCALL_MAX)) {
        handleUnknownSyscall(syscall);
    } else {
        handleSyscall(syscall);
    }

    restore_user_context();
    UNREACHABLE();
}

void VISIBLE NORETURN c_handle_syscall(word_t cptr, word_t msgInfo, word_t unused1, word_t unused2, word_t unused3,
                                       word_t unused4, word_t unused5, syscall_t syscall)
{
    NODE_LOCK_SYS;

    c_entry_hook();

#ifdef CONFIG_FASTPATH
    if (syscall == (syscall_t)SysCall) {
        fastpath_call(cptr, msgInfo);
        UNREACHABLE();
    } else if (syscall == (syscall_t)SysReplyRecv) {
        fastpath_reply_recv(cptr, msgInfo);
        UNREACHABLE();
    }
#xxx    
#endif /* CONFIG_FASTPATH */
    slowpath(syscall);
    UNREACHABLE();
}

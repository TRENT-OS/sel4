/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * SiFive U54/U74 PLIC handling (HiFive Unleashed/Unmatched, Polarfire)
 */

#pragma once

/* This is a check that prevents using this driver blindly. Extend the list if
 * this driver is confirmed to be working on other platforms.
 */
#if !defined(CONFIG_PLAT_HIFIVE) && !defined(CONFIG_PLAT_POLARFIRE)
#error "This code supports the SiFive U54/U74 PLIC only."
#endif

#include <util.h>
#include <plat/machine/devices_gen.h>
#include <arch/model/smp.h>
#include <arch/machine/plic.h>

/* All global interrupts are edge triggered, so this driver does not define
 * HAVE_SET_TRIGGER and has no implementation for plic_irq_set_trigger().
 */

/* Interrupt 0 is a dummy, the real interrupts are 1 - PLIC_MAX_IRQ. */
#define PLIC_NUM_INTERUPTS          (PLIC_MAX_IRQ + 1)
#define PLIC_NUM_U32_INTR_BITMAPS   ((PLIC_NUM_INTERUPTS + 31) / 32)


/* The memory map is based on the PLIC section in
 * https://static.dev.sifive.com/U54-MC-RVCoreIP.pdf
 */
typedef volatile struct {
    uint32_t priority[PLIC_NUM_INTERUPTS];              /* 0x000000 */
    uint32_t _gap1[1024 - PLIC_NUM_INTERUPTS];
    uint32_t pending[PLIC_NUM_U32_INTR_BITMAPS];        /* 0x001000 */
    uint32_t _gap2[1024 - PLIC_NUM_U32_INTR_BITMAPS];
    struct {                                            /* 0x002000 */
        /* Core 0 has M-Mode only, cores 1-4 have both M-Mode and  S-Mode, so
         * there are these register banks of 128 byte each:
         *   0x002000: Hart 0, M-Mode
         *   0x002080: Hart 1, M-Mode
         *   0x002100: Hart 1, S-Mode
         *   0x002180: Hart 2, M-Mode
         *   0x002200: Hart 2, S-Mode
         *   0x002280: Hart 3, M-Mode
         *   0x002300: Hart 3, S-Mode
         *   0x002380: Hart 4, M-Mode
         *   0x002400: Hart 4, S-Mode
         */
        uint32_t enable[PLIC_NUM_U32_INTR_BITMAPS];
        uint32_t _gap[32 - PLIC_NUM_U32_INTR_BITMAPS];
    } hart_enable[9];
    uint32_t _gap3[(0x200000 - 0x2480) / 4];
    struct {                                            /* 0x200000 */
        /* Core 0 has M-Mode only, cores 1-4 have both M-Mode and  S-Mode, so
         * there are these register banks of 4096 byte each:
         *   0x200000: Hart 0, M-Mode
         *   0x201000: Hart 1, M-Mode
         *   0x202000: Hart 1, S-Mode
         *   0x203000: Hart 2, M-Mode
         *   0x204000: Hart 2, S-Mode
         *   0x205000: Hart 3, M-Mode
         *   0x206000: Hart 3, S-Mode
         *   0x207000: Hart 4, M-Mode
         *   0x208000: Hart 4, S-Mode
         */
        uint32_t threshold;
        uint32_t claim;
        uint32_t _gap[1022];
    } hart_regs[9];
} plic_t;

SEL4_COMPILE_ASSERT(
    ERROR_field_pending_for_plic_cfg_t,
    0x1000 == SEL4_OFFSETOF(plic_t, pending));

SEL4_COMPILE_ASSERT(
    ERROR_field_hart_enable_for_plic_cfg_t,
    0x2000 == SEL4_OFFSETOF(plic_t, hart_enable));

SEL4_COMPILE_ASSERT(
    ERROR_field_hart_enable_1_for_plic_cfg_t,
    0x2080 == SEL4_OFFSETOF(plic_t, hart_enable[1]));

SEL4_COMPILE_ASSERT(
    ERROR_invalid_hart_regs_for_plic_cfg_t,
    0x200000 == SEL4_OFFSETOF(plic_t, hart_regs));

SEL4_COMPILE_ASSERT(
    ERROR_invalid_hart_regs_1_for_plic_cfg_t,
    0x201000 == SEL4_OFFSETOF(plic_t, hart_regs[1]));


static inline int plic_get_current_hart_s_mode_idx(void)
{
    word_t hart_id = SMP_TERNARY(cpuIndexToID(getCurrentCPUIndex()),
                                 CONFIG_FIRST_HART_ID);

    /* Get the S-Mode register bank index for the current core. Core 0 only has
     * only M-Mode, Cores 1-4 have both M-Mode and S-Mode.
     */
    if (unlikely((hart_id < 1) || (hart_id > 4))) {
        printf("ERROR: invalid hart id %"SEL4_PRIu_word"\n", hart_id);
        halt();
        UNREACHABLE();
    }

    return 2 * hart_id;
}

static inline irq_t plic_get_claim(void)
{
    plic_t * const plic = ((plic_t *)PLIC_PPTR);
    word_t idx = plic_get_current_hart_s_mode_idx();
    assert(idx < ARRAY_SIZE(plic->hart_regs));

    /* Read the claim register for our HART interrupt context */
    return plic->hart_regs[idx].claim;
}

static inline void plic_complete_claim(irq_t irq)
{
    plic_t * const plic = ((plic_t *)PLIC_PPTR);
    word_t idx = plic_get_current_hart_s_mode_idx();
    assert(idx < ARRAY_SIZE(plic->hart_regs));

    /* Complete the IRQ claim by writing back to the claim register. */
    // uint32_t pre[2] = { plic->pending[0], plic->pending[1] };
    plic->hart_regs[idx].claim = irq;
    // uint32_t post[2] = { plic->pending[0], plic->pending[1] };
    // if (pre[0] | pre[1] | post[0] | post[1]) {
    //     printf("plic_complete_claim() irq %d, pre %04x'%04x'%04x'%04x, post %04x'%04x'%04x'%04x\n",
    //         (int)irq,
    //         pre[1] >> 16, pre[1] & 0xffff, pre[0] >> 16,  pre[0] & 0xffff,
    //         post[1] >> 16, post[1] & 0xffff, post[0] >> 16,  post[0] & 0xffff);
    // }
}

static inline void plic_mask_irq(bool_t disable, irq_t irq)
{
    plic_t * const plic = (plic_t *)PLIC_PPTR;

    /* The threshold is configured to 0, thus setting priority 0 masks an
     * interrupt and setting 1 will unmask it.
     */
    // uint32_t pre[2] = { plic->pending[0], plic->pending[1] };
    plic->priority[irq] = disable ? 0 : 1;
    // uint32_t post[2] = { plic->pending[0], plic->pending[1] };
    // if (pre[0] | pre[1] | post[0] | post[1]) {
    //     printf("%smask irq %d, pre %04x'%04x%04x%04x, post %04x'%04x%04x%04x\n",
    //            disable?"":"un", (int)irq,
    //         pre[1] >> 16, pre[1] & 0xffff, pre[0] >> 16,  pre[0] & 0xffff,
    //         post[1] >> 16, post[1] & 0xffff, post[0] >> 16,  post[0] & 0xffff);
    // }
}

static inline void plic_enable_irq(bool_t enable, irq_t irq)
{
    printf("%sable irq %d\n", enable?"en":"dis", (int)irq);
    plic_t * const plic = (plic_t *)PLIC_PPTR;
    word_t idx = plic_get_current_hart_s_mode_idx();
    assert(idx < ARRAY_SIZE(plic->hart_enable));

    uint32_t mask = BIT(irq % 32);
    volatile uint32_t * const reg = &plic->hart_enable[idx].enable[irq / 32];
    if (enable) {
        *reg |= mask;
    } else {
        *reg &= ~mask;
    }
}

static inline void plic_init_hart(void)
{
    plic_t * const plic = (plic_t *)PLIC_PPTR;
    word_t idx = plic_get_current_hart_s_mode_idx();
    assert(idx < ARRAY_SIZE(plic->hart_regs));

    /* Set threshold to 0, this masks all interrupts with priority 0. */
    plic->hart_regs[idx].threshold = 0;
}

static inline void plic_init_controller(void)
{
    printf("init PLIC, interrupts 1 - %d\n", PLIC_MAX_IRQ);

    plic_t * const plic = (plic_t *)PLIC_PPTR;

    /* Set the priority of each interrupt to 0, then enable it. The priority of
     * 0 masks it. Interrupt 0 is a dummy, interrupts 1 to PLIC_MAX_IRQ the the
     * real interrupts that need to be configured.
     */
    for (word_t irq = 1; irq <= PLIC_MAX_IRQ; irq++) {
        plic->priority[irq] = 0;
        plic_enable_irq(true, irq);
    }

    /* Now that all interrupts are masked, clean any that are still pending. */
    for (;;) {
        uint32_t irq = plic_get_claim();
        if (0 == irq) {
            break;
        }
        printf("drop pending interrupt %d\n", irq);
        plic_complete_claim(irq);
    }
}

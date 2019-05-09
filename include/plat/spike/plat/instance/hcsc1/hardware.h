/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */

#ifndef __PLAT_INSTANCE_HARDWARE_H
#define __PLAT_INSTANCE_HARDWARE_H

#define PLIC_MAX_NUM_INT   24
#define HAVE_SET_TRIGGER 1

#define PLIC_BASE 0x00200000
#define PLIC_PPTR_BASE 0xFFFFFFFFC0200000

// #include <plat/machine/peripherals_plic.h>
struct plic {
    uint64_t config;
    uint32_t el;
    uint32_t priority0;
    uint32_t priority1;
    uint32_t priority2;
    uint32_t ietarget0;
    uint32_t ietarget1;
    uint32_t threshold0;
    uint32_t threshold1;
    uint32_t idtarget0;
    uint32_t idtarget1;
};
extern volatile struct plic *const plic;
static inline interrupt_t plic_get_claim(void)
{
    return plic->idtarget0;
}

static inline void plic_complete_claim(interrupt_t irq) {
    plic->idtarget0 = irq;
}

static inline void plic_mask_irq(bool_t disable, interrupt_t irq) {
    if(disable) {
        plic->ietarget0 &= ~BIT(irq-1);
        plic_complete_claim(irq);
    } else {
        plic->ietarget0 |= BIT(irq-1);
    }
}


static inline void plic_irq_set_trigger(irq_t irq, bool_t edge_triggered) {
    if (edge_triggered) {
        plic->el |= BIT(irq-1);
    } else {
        plic->el &= ~BIT(irq-1);
    }

}

static inline void plic_init_controller(void) {
    volatile uint32_t *priorities = (uint32_t *)&plic->priority0;
    for (int i = 1; i<= PLIC_MAX_NUM_INT; i++) {
        plic_mask_irq(true, i);
        int index = (i-1) / 8;
        int bit = 4* ((i-1) % 8);
        priorities[index] |= BIT(bit);
    }
    plic->el = 0;
    plic->threshold0 = 0;

}

/* Available physical memory regions on platform (RAM minus kernel image). */
/* NOTE: Regions are not allowed to be adjacent! */
static const p_region_t BOOT_RODATA avail_p_regs[] = {
     { /*.start = */ 0x1e13000, /* .end = */ 0x2100000}
};

static const p_region_t BOOT_RODATA dev_p_regs[] = {
    { 0x00404000, 0x00405000 }, /* UART0 */
    { 0x00405000, 0x00406000 }, /* UART1 */
    { 0x00406000, 0x00407000 }, /* UART2 */
    { 0x00408000, 0x00409000 }, /* UART2 */
};

#endif

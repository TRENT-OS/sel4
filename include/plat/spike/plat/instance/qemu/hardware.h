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

#define PLIC_MAX_NUM_INT   0

struct plic {

};
extern volatile struct plic *const plic;



static inline interrupt_t plic_get_claim(void)
{
}

static inline void plic_complete_claim(interrupt_t irq) {
}

static inline void plic_mask_irq(bool_t disable, interrupt_t irq) {
}

static inline void plic_init_controller(void) {

}
/* Available physical memory regions on platform (RAM minus kernel image). */
/* NOTE: Regions are not allowed to be adjacent! */
static p_region_t BOOT_DATA avail_p_regs[] = {
    /* The first 2MB are reserved for the SBI in the BBL */
#if defined(CONFIG_ARCH_RISCV64)
    { /*.start = */ 0x80200000, /* .end = */ 0x17FF00000}
#elif defined(CONFIG_ARCH_RISCV32)
    { /*.start = */ 0x80200000, /* .end = */ 0xFD000000}
#endif
};


static const p_region_t BOOT_RODATA dev_p_regs[] = {
};


#endif


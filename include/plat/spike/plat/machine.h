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

#ifndef __PLAT_MACHINE_H
#define __PLAT_MACHINE_H

#ifndef __ASSEMBLER__

typedef uint32_t interrupt_t;
typedef uint32_t irq_t;

#if defined(CONFIG_BUILD_SPIKE_QEMU)
#include <plat_instance/hardware.h>
#elif defined(CONFIG_BUILD_ROCKET_CHIP_ZEDBOARD)
#include <plat_instance/rocket-chip/hardware.h>
#elif defined(CONFIG_BUILD_HI_FIVE_UNLEASHED)
#include <plat/instance/hifive/hardware.h>
#elif defined(CONFIG_BUILD_HCSC1)
#include <plat/instance/hcsc1/hardware.h>
#else
#error "Unsupported spike platform chosen"
#endif

enum IRQConstants {
    PLIC_IRQ_OFFSET = 0,
    PLIC_MAX_IRQ = PLIC_IRQ_OFFSET + PLIC_MAX_NUM_INT,
    INTERRUPT_CORE_TIMER,
    maxIRQ = INTERRUPT_CORE_TIMER,
} platform_interrupt_t;

#define KERNEL_TIMER_IRQ INTERRUPT_CORE_TIMER

enum irqNumbers {
    irqInvalid = 0
};

#endif /* !__ASSEMBLER__ */
#endif

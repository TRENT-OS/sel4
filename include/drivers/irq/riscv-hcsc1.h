/*
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: BSD-2-Clause and GPL-2.0-or-later
 */

#pragma once

#define HAVE_SET_TRIGGER    1

#include <plat/machine/devices_gen.h>

/* We have PLIC_MAX_IRQ interrupts, the actual IDs are 1 to PLIC_MAX_IRQ. The
 * interrupt 0 is not actively used, it is only use to indicate the "no
 * interrupt pending" condition.
 */

#define MIGV_PLIC_TARGET_M_MODE     0
#define MIGV_PLIC_TARGET_S_MODE     1

typedef volatile struct {
    uint64_t config;
    uint32_t trigger_mode; /* 0 is level, 1 is edge */
    uint32_t priority[3]; /* 4 bit per interrupt */
    uint32_t ie_target[2]; /* 0 is M-Mode, 1 is S-Mode */
    uint32_t threshold[2]; /* 0 is M-Mode, 1 is S-Mode */
    uint32_t claim[2]; /* 0 is M-Mode, 1 is S-Mode */
} migv_plic_t;

#define MIGV_PLIC   ((migv_plic_t *)PLIC_PPTR)

/* The priory registers must provide space for all interrupts. MiG-V 1.0 has
 * 24 interrupts, so 3 registers of 32-bit each are sufficient. MiG-V 1.1 will
 * have 30 interrupts, so 4 registers will be needed. */
SEL4_COMPILE_ASSERT(
    ERROR_numer_of_intrrupts_exceed_priority_register_space,
    4 * PLIC_MAX_IRQ <= 32 * ARRAY_SIZE(MIGV_PLIC->priority));


static inline irq_t plic_get_claim(void)
{
    return MIGV_PLIC->claim[MIGV_PLIC_TARGET_S_MODE];
}

static inline void plic_complete_claim(irq_t irq)
{
    MIGV_PLIC->claim[MIGV_PLIC_TARGET_S_MODE] = irq;
}

static inline void plic_mask_irq(bool_t disable, irq_t irq)
{
    /* Interrupt 0 is only used to indicate "not interrupt pending, thus it
     * does not exist in the mask register.
     */
    if (irq > 0)
    {
        uint32_t mask = BIT(irq - 1);
        volatile uint32_t *reg = &MIGV_PLIC->ie_target[MIGV_PLIC_TARGET_S_MODE];

        if(disable) {
            *reg &= ~mask; /* clear */
        } else {
            *reg |= mask; /* set */
        }
    }
}

static inline void plic_irq_set_trigger(irq_t irq, bool_t edge_triggered)
{
    /* Interrupt 0 is only used to indicate "not interrupt pending, thus it
     * does not exist in the trigger mode register.
     */
    if (irq > 0)
    {
        uint32_t mask = BIT(irq - 1);
        volatile uint32_t *reg = &MIGV_PLIC->trigger_mode;

        if (edge_triggered) {
            *reg |= mask; /* set */
        } else {
            *reg &= ~mask; /* clear */
        }
    }
}

static inline void plic_init_hart(void)
{
    /* Nothing to do. */
}

static inline void plic_init_controller(void)
{
    /* Interrupt 0 is only used to indicate "not interrupt pending, thus it
     * cannot be masked. Mask interrupts 1 to PLIC_MAX_IRQ
     */
    for (word_t i = 1; i <= PLIC_MAX_IRQ; i++) {
        plic_mask_irq(true, i);
    }
    /* Set all interrupt to priority 1 by default. */
    for (word_t i = 0; i < ARRAY_SIZE(MIGV_PLIC->priority); i++) {
        MIGV_PLIC->priority[i] = 0x11111111;
    }
    /* Make all interrupts edge-triggered by default. */
    MIGV_PLIC->trigger_mode = 0xFFFFFFFF;
    /* Disable threshold. */
    MIGV_PLIC->threshold[MIGV_PLIC_TARGET_S_MODE] = 0;
}

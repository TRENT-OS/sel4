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

#include <types.h>
#include <util.h>
#include <machine/io.h>
#include <kernel/vspace.h>
#include <arch/machine.h>
#include <arch/kernel/vspace.h>
#include <plat/machine.h>
#include <linker.h>
#include <plat/machine/devices.h>
#include <plat/machine/hardware.h>
#include <arch/sbi.h>

#define MAX_AVAIL_P_REGS 2

#define STIMER_IP 5
#define STIMER_IE 5
#define STIMER_CAUSE 5
#define SEXTERNAL_IP 9
#define SEXTERNAL_IE 9
#define SEXTERNAL_CAUSE 9

#define RESET_CYCLES ((CONFIG_SPIKE_CLOCK_FREQ / MS_IN_S) * CONFIG_TIMER_TICK_MS)

#define IS_IRQ_VALID(X) (((X)) <= maxIRQ && (X)!= irqInvalid)
volatile struct plic *const plic = (void*)PLIC_PPTR_BASE;

static const kernel_frame_t BOOT_RODATA kernel_devices[] = {
    {
        /* Plic0 */
        0x00000000,
        0xFFFFFFFFC0000000lu
    }
};


BOOT_CODE int get_num_avail_p_regs(void)
{
    return sizeof(avail_p_regs) / sizeof(p_region_t);
}

BOOT_CODE p_region_t get_avail_p_reg(word_t i)
{
    return avail_p_regs[i];
}

BOOT_CODE int get_num_dev_p_regs(void)
{
    return sizeof(dev_p_regs) / sizeof(p_region_t);
}

BOOT_CODE p_region_t get_dev_p_reg(word_t i)
{
    return dev_p_regs[i];
}

BOOT_CODE void map_kernel_devices(void)
{
    for (int i = 0; i < ARRAY_SIZE(kernel_devices); i++) {
        map_kernel_frame(kernel_devices[i].paddr,
                kernel_devices[i].pptr,
                VMKernelOnly);
    }
}

static interrupt_t getNewActiveIRQ(void)
{

    uint64_t scause = read_scause();
    if (!(scause & BIT(CONFIG_WORD_SIZE - 1))) {
        return irqInvalid;
    }

    uint32_t irq = scause & 0xF;

    /* External IRQ */
    if (irq == SEXTERNAL_CAUSE) {
        return plic_get_claim();
    } else if (irq == STIMER_CAUSE) {
        // Supervisor timer interrupt
        return INTERRUPT_CORE_TIMER;

    } else {
        printf("Got invalid irq scause exception code: 0x%x\n", irq);
        halt();
    }

}

static uint32_t active_irq = irqInvalid;

interrupt_t getActiveIRQ(void)
{

    uint32_t irq;
    if (!IS_IRQ_VALID(active_irq)) {
        active_irq = getNewActiveIRQ();
    }

    if (IS_IRQ_VALID(active_irq)) {
        irq = active_irq;
    } else {
        irq = irqInvalid;
    }

    return irq;
}

#ifdef HAVE_SET_TRIGGER
void setIRQTrigger(irq_t irq, bool_t edge_triggered)
{
    plic_irq_set_trigger(irq, edge_triggered);
}
#endif

/* Check for pending IRQ */
bool_t isIRQPending(void)
{
    word_t sip = read_sip();
    return (sip & (BIT(STIMER_IP) | BIT(SEXTERNAL_IP)));
}

/* Enable or disable irq according to the 'disable' flag. */
/**
   DONT_TRANSLATE
*/
void maskInterrupt(bool_t disable, interrupt_t irq)
{
    assert(IS_IRQ_VALID(irq));
    if (irq == INTERRUPT_CORE_TIMER) {
        if (disable) {
            clear_sie_mask(BIT(STIMER_IE));
        } else {
            set_sie_mask(BIT(STIMER_IE));
        }
    } else {
        plic_mask_irq(disable, irq);
    }
}


void ackInterrupt(irq_t irq)
{
    assert(IS_IRQ_VALID(irq));
    active_irq = irqInvalid;

    if (irq == INTERRUPT_CORE_TIMER) {
        /* Reprogramming the timer has cleared the interrupt. */
        return;
    }
    /* We have masked PLIC interrupts so there is nothing to ack */

}

static inline uint64_t get_cycles(void)
#if __riscv_xlen == 32
{
    uint32_t nH, nL;
    asm volatile(
        "rdtimeh %0\n"
        "rdtime  %1\n"
        : "=r"(nH), "=r"(nL));
    return ((uint64_t)((uint64_t) nH << 32)) | (nL);
}
#else
{
    uint64_t n;
    asm volatile(
        "rdtime %0"
        : "=r"(n));
    return n;
}
#endif

static inline int read_current_timer(unsigned long *timer_val)
{
    *timer_val = get_cycles();
    return 0;
}

void resetTimer(void)
{
    uint64_t target;
    // repeatedly try and set the timer in a loop as otherwise there is a race and we
    // may set a timeout in the past, resulting in it never getting triggered
    do {
        target = get_cycles() + RESET_CYCLES;
        sbi_set_timer(target);
    } while (get_cycles() > target);
}

/**
   DONT_TRANSLATE
 */
BOOT_CODE void initTimer(void)
{
    sbi_set_timer(get_cycles() + RESET_CYCLES);
}

void plat_cleanL2Range(paddr_t start, paddr_t end)
{
}
void plat_invalidateL2Range(paddr_t start, paddr_t end)
{
}

void plat_cleanInvalidateL2Range(paddr_t start, paddr_t end)
{
}

/**
   DONT_TRANSLATE
 */
BOOT_CODE void initL2Cache(void)
{
}

/** DONT_TRANSLATE
 */
BOOT_CODE void initIRQController(void)
{
    printf("Initialing PLIC...\n");

    plic_init_controller();
    set_sie_mask(BIT(9));
}

void handleSpuriousIRQ(void)
{
    /* Do nothing */
    printf("Superior IRQ!! \n");
}

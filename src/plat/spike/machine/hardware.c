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

#define PLIC_HARTID 2

#define PLIC_PRIO            0x0
#define PLIC_PRIO_PER_ID     0x4

#define PLIC_EN         0x2000
#define PLIC_EN_PER_HART    0x80

#define PLIC_THRES      0x200000
#define PLIC_THRES_PER_HART 0x1000
#define PLIC_THRES_CLAIM    0x4


#define PLIC_BASE           0x0C000000
#define PLIC_PPTR_BASE      0xFFFFFFFFCC000000
#define IS_IRQ_VALID(X) (((X)) <= maxIRQ && (X)!= irqInvalid)

static const kernel_frame_t BOOT_RODATA kernel_devices[] = {
    {
        /* Plic0 */
        0x00000000,
        0xFFFFFFFFC0000000lu
    }
};

/* Available physical memory regions on platform (RAM minus kernel image). */
/* NOTE: Regions are not allowed to be adjacent! */

static p_region_t BOOT_DATA avail_p_regs[] = {
    /* The first 2MB are reserved for the SBI in the BBL */
#if defined(CONFIG_BUILD_ROCKET_CHIP_ZEDBOARD)
    { /*.start = */ 0x0, /* .end = */ 0x10000000}
#elif defined(CONFIG_ARCH_RISCV64)
    // { /*.start = */ 0x80200000, /* .end = */ 0x17FF00000}
    { /*.start = */ 0x80200000, /* .end = */ 0xa0000000}
#elif defined(CONFIG_ARCH_RISCV32)
    { /*.start = */ 0x80200000, /* .end = */ 0xFD000000}
#endif
};

static const p_region_t BOOT_RODATA dev_p_regs[] = {
    { 0x10010000, 0x10011000 }, /* UART0 */
    { 0x10011000, 0x10012000 }, /* UART1 */
    { 0x10020000, 0x10021000 }, /* PWM0 */
    { 0x10021000, 0x10022000 }, /* PWM1 */
    { 0x10060000, 0x10061000 }, /* GPIO */
    { 0x10090000, 0x10091000 }, /* ETH */
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

static inline uint32_t readl(const volatile uint64_t addr)
{
    uint32_t val;
    asm volatile("lw %0, 0(%1)" : "=r"(val) : "r"(addr));

    return val;
}

static inline void writel(uint32_t val, volatile uint64_t addr)
{
    asm volatile("sw %0, 0(%1)" : : "r"(val), "r"(addr));
}

static interrupt_t plic_get_claim(void)
{
    return readl(PLIC_PPTR_BASE + PLIC_THRES + PLIC_THRES_PER_HART * PLIC_HARTID +
           PLIC_THRES_CLAIM);
}

void plic_complete_claim(interrupt_t irq) {
    /*completion */
    writel(irq, PLIC_PPTR_BASE + PLIC_THRES + PLIC_THRES_PER_HART * PLIC_HARTID +
       PLIC_THRES_CLAIM);
}

static void plic_mask_irq(bool_t disable, interrupt_t irq) {
    uint64_t addr = 0;
    uint32_t val = 0;

    if (disable) {
        if (irq >= 32) {
            irq -= 32;
            addr = 0x4;
        }

        addr += PLIC_PPTR_BASE + PLIC_EN;
        val = readl(addr + PLIC_EN_PER_HART * PLIC_HARTID);
        val &= ~BIT(irq);
        writel(val, addr + PLIC_EN_PER_HART * PLIC_HARTID);

    } else {
        /* Account for external and PLIC interrupts */
        if (irq >= 32) {
            irq -= 32;
            addr = 0x4;
        }

        addr += PLIC_PPTR_BASE + PLIC_EN;
        val = readl(addr + PLIC_EN_PER_HART * PLIC_HARTID);
        val |= BIT(irq);
        writel(val, addr + PLIC_EN_PER_HART * PLIC_HARTID);

        // Clear any pending claims as they won't be raised again.
        plic_complete_claim(irq);
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

    uint32_t pending;

    /* Clear all pending bits */
    pending = readl(PLIC_PPTR_BASE + 0x1000);
    for (int i = 0; i < 32 ; i++) {
        if (pending & (1 << i)) {
                readl(PLIC_PPTR_BASE + PLIC_THRES +
                PLIC_THRES_PER_HART * PLIC_HARTID +
                    PLIC_THRES_CLAIM);
        }
    }
    pending = readl(PLIC_PPTR_BASE + 0x1004);
    for (int i = 0; i < 22 ; i++) {
        if (pending & (1 << i)) {
                readl(PLIC_PPTR_BASE + PLIC_THRES +
                PLIC_THRES_PER_HART * PLIC_HARTID +
                    PLIC_THRES_CLAIM);
        }
    }

    /* Disable interrupts */
    writel(0, PLIC_PPTR_BASE + PLIC_EN + PLIC_EN_PER_HART * PLIC_HARTID);
    writel(0, PLIC_PPTR_BASE + PLIC_EN + PLIC_EN_PER_HART * PLIC_HARTID + 0x4);

    /* Set threshold to zero */
    writel(1, (PLIC_PPTR_BASE + PLIC_THRES + PLIC_THRES_PER_HART * PLIC_HARTID));

    /* Set the priorities of all interrupts to 1 */
    for (int i = 1; i <= PLIC_MAX_NUM_INT + 1; i++) {
        writel(2, PLIC_PPTR_BASE + PLIC_PRIO + PLIC_PRIO_PER_ID * i);
    }

    set_sie_mask(BIT(9));
}

void handleSpuriousIRQ(void)
{
    /* Do nothing */
    printf("Superior IRQ!! \n");
}

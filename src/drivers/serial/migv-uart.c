/*
 * Copyright 2020, Hensoldt Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <config.h>
#include <stdint.h>
#include <util.h>
#include <machine/io.h>
#include <plat/machine/devices_gen.h>

//------------------------------------------------------------------------------
// UART definitions
//------------------------------------------------------------------------------

typedef struct {
    uint32_t RBR_DLL_THR; // Receiver Buffer Register (Read Only)
                          // Divisor Latch (LS)
                          // Transmitter Holding Register (Write Only)
    uint32_t DLM_IER;     // Divisor Latch (MS)
                          // Interrupt Enable Register
    uint32_t IIR_FCR;     // Interrupt Identity Register (Read Only)
                          // FIFO Control Register (Write Only)
    uint32_t LCR;         // Line Control Register
    uint32_t MCR;         // MODEM Control Register
    uint32_t LSR;         // Line Status Register
    uint32_t MSR;         // MODEM Status Register
    uint32_t SCR;         // Scratch Register
} migv_uart_t;

#define UART_LCR_DLAB    (1u << 7)   // DLAB bit in LCR reg
#define UART_IER_ERBFI   (1u << 0)   // ERBFI bit in IER reg
#define UART_IER_ETBEI   (1u << 1)   // ETBEI bit in IER reg

#define UART_LSR_RX      (1u << 0)   //UART_LSR_DR_MASK
#define UART_LSR_TX      (1u << 5)   // UART_LSR_TEMT_MASK


//------------------------------------------------------------------------------
// UART device
//------------------------------------------------------------------------------

#define migv_uart0  ( (volatile migv_uart_t *)UART_PPTR )


//------------------------------------------------------------------------------
// UART functions
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
#if defined(CONFIG_DEBUG_BUILD) || defined(CONFIG_PRINTING)
void putDebugChar(unsigned char c)
{
    while (0 == (migv_uart0->LSR & UART_LSR_TX))
    {
        // loop
    }

    migv_uart0->RBR_DLL_THR = c;
}

#endif // defined(CONFIG_DEBUG_BUILD) || defined(CONFIG_PRINTING)


//------------------------------------------------------------------------------
#if defined(CONFIG_DEBUG_BUILD)
unsigned char getDebugChar(void)
{
    if (0 == (migv_uart0->LSR & UART_LSR_RX))
    {
       return -1;
    }

    return migv_uart0->RBR_DLL_THR;
}
#endif // defined(CONFIG_DEBUG_BUILD)

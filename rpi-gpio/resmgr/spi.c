/*
 * $QNXLicenseC:
 * Copyright 2019, QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this software,
 * or any work that includes all or part of this software.   Free development
 * licenses are available for evaluation and non-commercial purposes.  For more
 * information visit http://licensing.qnx.com or email licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

/**
 * @file    spi.c
 * @brief   SPI support
 */

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/rpi_gpio.h>
#include <aarch64/rpi_gpio.h>
#include "rpi_gpio_priv.h"

static volatile uint32_t    *spi_regs;

enum
{
    REG_CS = 0,
    REG_FIFO,
    REG_CLK,
    REG_DLEN,
    REG_LTOH,
    REG_DC
};

enum
{
    CS_0 =          0x0,
    CS_1 =          0x1,
    CS_2 =          0x2,
    CS_MANUAL =     0x3,
    CS_MASK =       0x3
};

#define CS_CLEAR_TX (1u << 4)
#define CS_CLEAR_RX (1u << 5)
#define CS_TA       (1u << 7)
#define CS_DONE     (1u << 16)
#define CS_RXD      (1u << 17)
#define CS_TXD      (1u << 18)

/**
 * Wait for a register to have a specific value in the specified bits.
 * @param   reg     Register number
 * @param   mask    Bit mask to test
 * @param   value   Expected value
 * @param   timeout Number of times to loop
 * @return  1 if successful, 0 on time out
 */
static int
wait_reg(
    uint32_t const              reg,
    uint32_t const              mask,
    uint32_t const              value,
    unsigned const              timeout
)
{
    if (verbose > 1) {
        printf("SPI Wait: Register: %u mask=%x value=%x cur=%x\n", reg, mask,
               value, spi_regs[reg]);
    }

    for (unsigned i = 0; i < timeout; i++) {
        if ((spi_regs[reg] & mask) == value) {
            return 1;
        }
        asm volatile("yield");
    }

    if (verbose > 0) {
        printf("SPI Timeout: Register: %u mask=%x value=%x cur=%x\n", reg, mask,
               value, spi_regs[reg]);
    }

    return 0;
}

/**
 * Initialize the SPI module.
 * @return  EOK if successful, error code otherwise
 */
int
spi_init(rpi_gpio_spi_t const * const msg)
{
    if (verbose > 0) {
        printf("spi_init() begin clkdiv=%u\n", msg->clkdiv);
    }

    // Map the SPI registers.
    spi_regs = mmap(0, __PAGESIZE,
                    PROT_READ | PROT_WRITE | PROT_NOCACHE,
                    MAP_SHARED | MAP_PHYS,
                    -1, base_paddr + 0x204000);
    if (spi_regs == MAP_FAILED) {
        perror("Failed to map SPI registers");
        return ENOMEM;
    }

    // Set GPIO 9 to ALT 0 function (MISO).
    // Set GPIO 10 to ALT 0 function (MOSI).
    // Set GPIO 11 to ALT 0 function (SCLK).
    rpi_gpio_set_select(9, 4);
    rpi_gpio_set_select(10, 4);
    rpi_gpio_set_select(11, 4);

    spi_regs[REG_CS] = 0;

    // Clear FIFOs.
    spi_regs[REG_CS] = CS_CLEAR_TX | CS_CLEAR_RX;

    // Set clock divider.
    spi_regs[REG_CLK] = msg->clkdiv;

    if (verbose > 0) {
        printf("spi_init() end\n");
    }

    return EOK;
}

/**
 * Write data to and then read data back from the SPI interface.
 * @param   msg         A RPI_GPIO_SPI_WRITE_READ message
 * @param   srclen      Length of the message, including header and payload
 * @param   dstlen      Length of the expected reply
 * @param   replylenp   Holds the number of read bytes, on successful return
 * @return  EOK if successful, error code otherwise
 */
int
spi_write_read(
    rpi_gpio_spi_t * const  msg,
    unsigned const          srclen,
    unsigned const          dstlen,
    unsigned * const        replylenp
)
{
    if ((msg->cs & ~CS_MASK) != 0) {
        if (verbose > 0) {
            printf("SPI: Invalid chip select %u\n", msg->cs);
        }
        return EINVAL;
    }

    // Calculate payload length.
    unsigned const  inlen = srclen - offsetof(rpi_gpio_spi_t, data);
    unsigned        outlen = 0;
    if (dstlen > offsetof(rpi_gpio_spi_t, data)) {
        outlen = dstlen - offsetof(rpi_gpio_spi_t, data);
    }

    if (verbose > 0) {
        printf("SPI: Writing %u bytes, reading %u bytes \n", inlen,
               outlen);
    }

    // Set chip select, clear FIFOs.
    uint32_t const  reg_cs = spi_regs[REG_CS] & ~CS_MASK;
    spi_regs[REG_CS] = reg_cs | msg->cs | CS_CLEAR_TX | CS_CLEAR_RX;

    // Start transfer.
    spi_regs[REG_CS] |= CS_TA;

    for (unsigned i = 0; i < inlen; i++) {
        if (!wait_reg(REG_CS, CS_TXD, CS_TXD, 10000)) {
            if (verbose > 0) {
                printf("Timeout waiting for TX FIFO\n");
            }
            return ETIMEDOUT;
        }

        // Write the data.
        spi_regs[REG_FIFO] = msg->data[i];

        if (verbose > 1) {
            printf("SPI: Wrote %x\n", msg->data[i]);
        }
    }

    for (unsigned i = 0; i < outlen; i++) {
        // Wait for data in the RX fifo.
        if (!wait_reg(REG_CS, CS_RXD, CS_RXD, 10000)) {
            if (verbose > 0) {
                printf("Timeout waiting for TX FIFO\n");
            }
            return ETIMEDOUT;
        }

        msg->data[i] = spi_regs[REG_FIFO];

        if (verbose > 1) {
            printf("SPI: Read %x\n", msg->data[i]);
        }
    }

    // Wait for the done signal.
    if (!wait_reg(REG_CS, CS_DONE, CS_DONE, 10000)) {
        if (verbose > 0) {
            printf("Timeout waiting for done signal\n");
        }
        return ETIMEDOUT;
    }

    // Finish transfer.
    spi_regs[REG_CS] &= ~CS_TA;

    *replylenp = outlen;
    return EOK;
}

/*
 * $QNXLicenseC:
 * Copyright 2021 QNX Software Systems. All Rights Reserved.
 *
 * You must obtain a written license from and pay applicable license fees to QNX
 * Software Systems before you may reproduce, modify or distribute this
 * software, or any work that includes all or part of this software.   Free
 * development licenses are available for evaluation and non-commercial
 * purposes.  For more information visit http://licensing.qnx.com or email
 * licensing@qnx.com.
 *
 * This file may contain contributions from others.  Please review this entire
 * file for other proprietary rights or license notices, as well as the QNX
 * Development Suite License Guide at http://licensing.qnx.com/license-guide/
 * for other information.
 * $
 */

#ifndef __RPI_GPIO_H__
#define __RPI_GPIO_H__

#include <inttypes.h>
#include <stdbool.h>
#include <time.h>
#include <sys/mman.h>

#define RPI_3_PERIPHERALS   0x3f000000
#define RPI_4_PERIPHERALS   0xfe000000

/**
 * RaspberryPi GPIO registers.
 */
enum
{
    RPI_GPIO_REG_GPSET0 = 7,
    RPI_GPIO_REG_GPCLR0 = 10,
    RPI_GPIO_REG_GPLEV0 = 13,
    RPI_GPIO_REG_GPEDS0 = 16,
    RPI_GPIO_REG_GPEDS1 = 17,
    RPI_GPIO_REG_GPREN0 = 19,
    RPI_GPIO_REG_GPREN1 = 20,
    RPI_GPIO_REG_GPFEN0 = 22,
    RPI_GPIO_REG_GPFEN1 = 23,
    RPI_GPIO_REG_GPHEN0 = 25,
    RPI_GPIO_REG_GPHEN1 = 26,
    RPI_GPIO_REG_GPLEN0 = 28,
    RPI_GPIO_REG_GPLEN1 = 29,
    // RaspberryPi 3
    RPI_GPIO_REG_GPPUD = 37,
    RPI_GPIO_REG_GPPUDCLK1 = 38,
    RPI_GPIO_REG_GPPUDCLK2 = 39,
    // RaspberryPi 4
    RPI_GPIO_REG_GPPUD0 = 57,
};

/**
 * Pull up/down values.
 */
enum
{
    RPI_GPIO_PUD_OFF = 0,
    RPI_GPIO_PUD_DOWN = 1,
    RPI_GPIO_PUD_UP = 2
};

/**
 * Pin function.
 */
enum
{
    RPI_GPIO_FUNC_IN = 0,
    RPI_GPIO_FUNC_OUT = 1,
    RPI_GPIO_FUNC_ALT_0 = 4,
    RPI_GPIO_FUNC_ALT_1 = 5,
    RPI_GPIO_FUNC_ALT_2 = 6,
    RPI_GPIO_FUNC_ALT_3 = 7,
    RPI_GPIO_FUNC_ALT_4 = 3,
    RPI_GPIO_FUNC_ALT_5 = 2,
};

#ifndef __RPI_GPIO_REGS
#define __RPI_GPIO_REGS rpi_gpio_regs
#endif

/**
 * Mapped GPIO registers.
 * An executable including this header is responsible for defining this global
 * in a source file.
 */
extern uint32_t volatile   *__RPI_GPIO_REGS;

/**
 * Programs the FSEL register for the given GPIO.
 * The value is one of the RPI_GPIO_FUNC_* constants.
 * @param   gpio    Pin number
 * @param   value   New pin function
 */
static inline void
rpi_gpio_set_select(uint32_t const gpio, uint32_t const value)
{
    uint32_t const  reg = (gpio / 10);
    uint32_t const  off = (gpio % 10) * 3;
    __RPI_GPIO_REGS[reg] &= ~(7 << off);
    __RPI_GPIO_REGS[reg] |= (value << off);
}

/**
 * Reads the FSEL register for the given GPIO.
 * @param   gpio    Pin number
 * @return  Current pin function
 */
static inline uint32_t
rpi_gpio_get_select(uint32_t const gpio)
{
    uint32_t const  reg = gpio / 10;
    uint32_t const  off = (gpio % 10) * 3;
    return (rpi_gpio_regs[reg] >> off) & 7;
}

/**
 * Turns on a pin in output mode.
 * @param   gpio    Pin number
 */
static inline void
rpi_gpio_set(uint32_t const gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPSET0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    __RPI_GPIO_REGS[reg] = (1 << off);
}

/**
 * Turns off a pin in output mode.
 * @param   gpio    Pin number
 */
static inline void
rpi_gpio_clear(uint32_t gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPCLR0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    __RPI_GPIO_REGS[reg] = (1 << off);
}

/**
 * Sets or clears a pin.
 * @param   gpio    Pin number
 * @param   value   0 to clear, any other value to set
 */
static inline void
rpi_gpio_write(uint32_t const gpio, uint32_t const value)
{
    if (value) {
        rpi_gpio_set(gpio);
    } else {
        rpi_gpio_clear(gpio);
    }
}

/**
 * Reads the level of a pin in input mode.
 * @param   gpio    Pin number
 * @return  0 if the pin is low, 1 if high
 */
static inline uint32_t
rpi_gpio_read(uint32_t const gpio)
{
    uint32_t const  reg = RPI_GPIO_REG_GPLEV0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    return (__RPI_GPIO_REGS[reg] & (1 << off)) >> off;
}

/**
 * Detect rising edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_rising_edge(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPREN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect falling edge events on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_falling_edge(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPFEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect a high level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_level_high(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPHEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Detect a low level on the given pin.
 * @param   gpio    Pin number
 * @param   enable  true to enable, false to disable
 */
static inline void
rpi_gpio_detect_level_low(uint32_t const gpio, bool const enable)
{
    uint32_t const  reg = RPI_GPIO_REG_GPLEN0 + (gpio / 32);
    uint32_t const  off = gpio % 32;
    if (enable) {
        __RPI_GPIO_REGS[reg] |= (1 << off);
    } else {
        __RPI_GPIO_REGS[reg] &= ~(1 << off);
    }
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * BCM2835 version (used in RaspberryPi 3).
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_set_pud_bcm2835(uint32_t const gpio, uint32_t const pud)
{
    switch (pud) {
    case RPI_GPIO_PUD_OFF:
    case RPI_GPIO_PUD_UP:
    case RPI_GPIO_PUD_DOWN:
        break;
    default:
        return false;
    }

    static int const pud_value_bcm2835[] = {
        [RPI_GPIO_PUD_OFF]  = 0,
        [RPI_GPIO_PUD_UP]   = 2,
        [RPI_GPIO_PUD_DOWN] = 1
    };

    // Set the PUD value.
    uint32_t const  value = pud_value_bcm2835[pud];
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] &= ~3;
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] |= value;

    // Manual says to wait 150 cycles. Assuming a cycle is at most one
    // nanosecond.
    nanospin_ns(150);

    // Enable the PUD clock for the GPIO.
    uint32_t const  clock_reg = RPI_GPIO_REG_GPPUDCLK1 + (gpio / 32);
    uint32_t const  clock_off = gpio % 32;
    __RPI_GPIO_REGS[clock_reg] = (1 << clock_off);

    nanospin_ns(150);

    // Turn off the PUD signal and the clock.
    __RPI_GPIO_REGS[RPI_GPIO_REG_GPPUD] &= ~3;
    __RPI_GPIO_REGS[clock_reg] = 0;

    return true;
}

/**
 * Enables pull-up/pull-down detection on the given GPIO.
 * BCM2711 version (used in RaspberryPi 4).
 * @param   gpio    GPIO number
 * @param   pud     One of the RPI_GPIO_PUD_* constants
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_set_pud_bcm2711(uint32_t const gpio, uint32_t const pud)
{
    switch (pud) {
    case RPI_GPIO_PUD_OFF:
    case RPI_GPIO_PUD_UP:
    case RPI_GPIO_PUD_DOWN:
        break;
    default:
        return false;
    }

    static int pud_value_bcm2711[] = {
        [RPI_GPIO_PUD_OFF]  = 0,
        [RPI_GPIO_PUD_UP]   = 1,
        [RPI_GPIO_PUD_DOWN] = 2
    };

    uint32_t const  reg = RPI_GPIO_REG_GPPUD0 + (gpio / 16);
    uint32_t const  off = (gpio % 16) * 2;
    uint32_t const  value = pud_value_bcm2711[pud];
    __RPI_GPIO_REGS[reg] &= ~(3 << off);
    __RPI_GPIO_REGS[reg] |= (value << off);
    return true;
}

/**
 * Map the GPIO registers into the process' address space.
 * @param   base_paddr  Base physical address of the I/O space
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_map_regs(uintptr_t const base_paddr)
{
    if (__RPI_GPIO_REGS != NULL) {
        return true;
    }

    void * const ptr = mmap(0, __PAGESIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE,
                            MAP_PHYS | MAP_SHARED, NOFD, base_paddr + 0x200000);
    if (ptr == MAP_FAILED) {
        return false;
    }

    __RPI_GPIO_REGS = ptr;
    return true;
}

/**
 * Unmap the GPIO registers.
 * @return  true if successful, false otherwise
 */
static inline bool
rpi_gpio_unmap_regs(void)
{
    if (__RPI_GPIO_REGS == NULL) {
        return true;
    }

    return (munmap((void *)__RPI_GPIO_REGS, __PAGESIZE) == 0);
}

#endif

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
 * @file    rpi_gpio.h
 * @brief   Private header for the resource manager implementation
 */

#ifndef RPI_GPIO_PRIV_H
#define RPI_GPIO_PRIV_H

#include <stdint.h>
#include "sys/rpi_gpio.h"

extern uint64_t base_paddr;
extern int      verbose;

enum {
    RPI_VER_UNKNOWN,
    RPI_VER_3,
    RPI_VER_4
};

static inline int
rpi_version(void)
{
    switch (base_paddr) {
    case 0x3f000000:
        return RPI_VER_3;
    case 0xfe000000:
        return RPI_VER_4;
    default:
        return RPI_VER_UNKNOWN;
    }
}

int     event_init(unsigned priority, int intr);
int     event_add(rcvid_t rcvid, rpi_gpio_event_t const *msg);
void    event_remove_rcvid(rcvid_t rcvid);
int     pwm_init(void);
int     pwm_setup(rcvid_t rcvid, rpi_gpio_pwm_t const *msg);
int     pwm_set_duty_cycle(rcvid_t rcvid, unsigned gpio, unsigned duty);
void    pwm_remove_rcvid(rcvid_t rcvid);
void    pwm_debug(unsigned gpio);
int     spi_init(rpi_gpio_spi_t const *msg);
int     spi_write_read(rpi_gpio_spi_t *msg, unsigned srclen, unsigned dstlen,
                       unsigned *replylenp);

#endif

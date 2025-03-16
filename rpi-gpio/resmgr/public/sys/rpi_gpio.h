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
 * @brief   Public header defining the interface to the resource manager
 */

#ifndef RPI_GPIO_H
#define RPI_GPIO_H

#include <sys/iomsg.h>
#include <sys/iomgr.h>
#include <aarch64/rpi_gpio.h>

#define RPI_GPIO_IOMGR  (_IOMGR_PRIVATE_BASE + 35)

#define RPI_GPIO_NUM    54

/**
 * _IO_MSG subtypes used as commands passed to the 'msg' node.
 */
enum
{
    /** Select GPIO configuration (input/output) */
    RPI_GPIO_SET_SELECT,
    /** Read GPIO configuration (input/output) */
    RPI_GPIO_GET_SELECT,
    /** Turn GPIO PIN on/off */
    RPI_GPIO_WRITE,
    /** Read GPIO PIN level */
    RPI_GPIO_READ,
    /** Report on a GPIO event asynchronously */
    RPI_GPIO_ADD_EVENT,
    /** Set up PWM with frequency and range */
    RPI_GPIO_PWM_SETUP,
    /** Set PWM duty cycke */
    RPI_GPIO_PWM_DUTY,
    /** Set pull-up/pull-down */
    RPI_GPIO_PUD,
    /** Initialize the SPI module. */
    RPI_GPIO_SPI_INIT,
    /** Write/read data to/from the SPI interface. */
    RPI_GPIO_SPI_WRITE_READ,
};

/**
 * Event detection types.
 */
enum
{
    RPI_EVENT_NONE          = 0,
    RPI_EVENT_EDGE_RISING   = 0x1,
    RPI_EVENT_EDGE_FALLING  = 0x2,
    RPI_EVENT_LEVEL_HIGH    = 0x4,
    RPI_EVENT_LEVEL_LOW     = 0x8
};

/**
 * PWM channel operation mode.
 */
enum
{
    RPI_PWM_MODE_PWM = 0,
    RPI_PWM_MODE_MS = 1
};

/**
 * Message structure used to communicate with the resource manager over the
 * 'msg' node. The hdr.type field must be set to _IO_MSG, and the hdr.subtype
 * field to one of the RPI_GPIO_* command constants. The gpio field is the
 * numeric GPIO ID and the value field is command specific:
 * RPI_GPIO_SET_SELECT: [in] new configuration value [out] previous value
 * RPI_GPIO_GET_SELECT: [out] current configuration value
 * RPI_GPIO_SET: Ignored
 * RPI_GPIO_CLEAR: Ignored
 * RPI_GPIO_LEVEL: [out] PIN state
 */
typedef struct
{
    struct _io_msg  hdr;
    unsigned        gpio;
    unsigned        value;
} rpi_gpio_msg_t;

/**
 * Message structure used with the RPI_GPIO_ADD_EVENT message subtype.
 */
typedef struct
{
    struct _io_msg  hdr;
    unsigned        gpio;
    unsigned        detect;
    struct sigevent event;
    unsigned        match;
    unsigned        reserved;
} rpi_gpio_event_t;

typedef struct
{
    struct _io_msg  hdr;
    unsigned        gpio;
    unsigned        frequency;
    unsigned        range;
    unsigned        mode;
} rpi_gpio_pwm_t;

/**
 * Message structure for SPI messages.
 */
typedef struct
{
    struct _io_msg  hdr;
    unsigned        zero;
    union {
        uint32_t    cs;
        uint32_t    clkdiv;
    };
    uint8_t         data[];
} rpi_gpio_spi_t;

#endif

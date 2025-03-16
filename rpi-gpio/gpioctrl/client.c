/*
 * Copyright (C) 2024 Elad Lahav. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/rpi_gpio.h>
#include "gpioctrl.h"

static int              server_fd = -1;
static int              chid;
static struct sigevent  notify_event;

/**
 * A thread for receiving GPIO state change notifications.
 */
static void *
notify_thread(void * const arg)
{
    screen_event_t  screen_event;
    screen_create_event(&screen_event);
    screen_set_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE,
                                 (const int[]){ SCREEN_EVENT_USER });

    for (;;) {
        // Receive a pulse.
        struct _pulse pulse;
        int rc = MsgReceivePulse(chid, &pulse, sizeof(pulse), NULL);
        if (rc == -1) {
            perror("MsgReceivePulse");
            exit(EXIT_FAILURE);
        }

        if (pulse.code == _PULSE_CODE_MINAVAIL) {
            // GPIO value changed.
            // Notify the main loop.
            int const gpio = pulse.value.sival_int;
            int data[4] = { gpio > 0 ? gpio : -gpio,
                            gpio > 0 ? 1 : 0 };
            screen_set_event_property_iv(screen_event,
                                         SCREEN_PROPERTY_USER_DATA, data);
            send_event(screen_event);
        }
    }

    return NULL;
}

/**
 * Initialize a client connection to the GPIO server.
 * @return  true if successful, false otherwise
 */
bool
client_init(void)
{
    // Connect to the server.
    server_fd = open("/dev/gpio/msg", O_RDWR);
    if (server_fd == -1) {
        perror("open(\"/dev/gpio/msg\")");
        return false;
    }

    // Create a channel for receiving notifications.
    chid = ChannelCreate(_NTO_CHF_PRIVATE);
    if (chid == -1) {
        perror("ChannelCreate");
        return false;
    }

    int const coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        perror("ConnectAttach");
        return false;
    }

    // Register a pulse event for notifications.
    SIGEV_PULSE_INIT(&notify_event, coid, -1, _PULSE_CODE_MINAVAIL, 0);
    SIGEV_MAKE_UPDATEABLE(&notify_event);
    if (MsgRegisterEvent(&notify_event, server_fd) ==  -1) {
        perror("MsgRegisterEvent");
        return false;
    }

    // Create a separate thread for notifications.
    pthread_t tid;
    int rc = pthread_create(&tid, NULL, notify_thread, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return false;
    }

    return true;
}

/**
 * Determine if the client is connected to the server.
 * @return  true if connected, false otherwise
 */
bool
client_connected(void)
{
    return server_fd != -1;
}

/**
 * Determine the function (input, output, other) of a given GPIO.
 * @param   gpio    The GPIO number
 * @return  One of the RPI_GPIO_FUNC* constants, or -1 on error
 */
int
client_get_gpio_func(int const gpio)
{
    if (server_fd == -1) {
        errno = EBADF;
        return -1;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .hdr.subtype = RPI_GPIO_GET_SELECT,
        .gpio = gpio
    };

    if (MsgSend(server_fd, &msg, sizeof(msg), &msg, sizeof(msg)) == -1) {
        perror("MsgSend(RPI_GPIO_GET_SELECT)");
        return -1;
    }

    return msg.value;
}

/**
 * Select the function (input, output, other) of a given GPIO.
 * @param   gpio    The GPIO number
 * @param   func    One of the RPI_GPIO_FUNC* constants, or -1 on error
 * @return  0 if successful, -1 otherwise
 */
int
client_set_gpio_func(int const gpio, int const func)
{
    if (server_fd == -1) {
        errno = EBADF;
        return -1;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .hdr.subtype = RPI_GPIO_SET_SELECT,
        .gpio = gpio,
        .value = func
    };

    if (MsgSend(server_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        perror("MsgSend(RPI_GPIO_SET_SELECT)");
        return -1;
    }

    if (func == RPI_GPIO_FUNC_IN) {
        // Request a notification when an input GPIO changes its value.
        rpi_gpio_event_t    notify = {
            .hdr.type = _IO_MSG,
            .hdr.mgrid = RPI_GPIO_IOMGR,
            .hdr.subtype = RPI_GPIO_ADD_EVENT,
            .gpio = gpio,
            .detect = RPI_EVENT_EDGE_FALLING | RPI_EVENT_EDGE_RISING,
            .event = notify_event
        };

        if (MsgSend(server_fd, &notify, sizeof(notify), NULL, 0) == -1) {
            perror("MsgSend(RPI_GPIO_ADD_EVENT)");
            return -1;
        }
    }

    return 0;
}

/**
 * Determine the value of a given GPIO.
 * @param   gpio    The GPIO number
 * @return  0 if off, 1 if on, -1 on error
 */
int
client_get_gpio_value(int const gpio)
{
    if (server_fd == -1) {
        errno = EBADF;
        return -1;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .hdr.subtype = RPI_GPIO_READ,
        .gpio = gpio
    };

    if (MsgSend(server_fd, &msg, sizeof(msg), &msg, sizeof(msg)) == -1) {
        perror("MsgSend(RPI_GPIO_READ)");
        return -1;
    }

    return msg.value;
}

/**
 * Change the value of a given GPIO.
 * @param   gpio    The GPIO number
 * @return  0 if successful, -1 otherwise
 */
int
client_set_gpio_value(int const gpio, int const value)
{
    if (server_fd == -1) {
        errno = EBADF;
        return -1;
    }

    rpi_gpio_msg_t  msg = {
        .hdr.type = _IO_MSG,
        .hdr.mgrid = RPI_GPIO_IOMGR,
        .hdr.subtype = RPI_GPIO_WRITE,
        .gpio = gpio,
        .value = value
    };

    if (MsgSend(server_fd, &msg, sizeof(msg), NULL, 0) == -1) {
        perror("MsgSend(RPI_GPIO_WRITE)");
        return -1;
    }

    return 0;
}

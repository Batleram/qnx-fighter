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
#include "gpioctrl.h"

/** Screen application context. */
static screen_context_t context;

/**
 * Handle a SCREEN_EVENT_MANAGER event.
 * @param   event   The event handle
 */
static void
manager_event(screen_event_t event)
{
    int subtype;
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_SUBTYPE, &subtype)
        == -1) {
        perror("screen_get_event_property_iv(SCREEN_PROPERTY_SUBTYPE)");
        return;
    }

    if (subtype == SCREEN_EVENT_CLOSE) {
        exit(EXIT_SUCCESS);
    }
}

/**
 * Handle a SCREEN_EVENT_POINTER event.
 * @param   event   The event handle
 */
static void
pointer_event(screen_event_t event)
{
    static int  prev_buttons = 0;
    int         buttons;

    // Get current button state.
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &buttons)
        == -1) {
        perror("screen_get_event_property_iv(SCREEN_PROEPRTY_BUTTONS)");
        return;
    }

    if (((prev_buttons & SCREEN_LEFT_MOUSE_BUTTON) != 0)
        && ((buttons & SCREEN_LEFT_MOUSE_BUTTON) == 0)) {
        // Left button released.
        if (gpio_clicked(event)) {
            main_window_draw();
        }
    }

    prev_buttons = buttons;
}

/**
 * Handle a notification event sent from the client thread.
 */
static void
user_event(screen_event_t event)
{
    int data[4];
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_USER_DATA, data)
        == -1) {
        perror("screen_get_event_property_iv(SCREEN_PROEPRTY_USER_DATA)");
        return;
    }

    if (gpio_update_value(data[0], data[1])) {
        main_window_draw();
    }
}

/**
 * Handle a change to a window property.
 */
static void
property_event(screen_event_t event)
{
    int name;
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_NAME, &name)
        == -1) {
        perror("screen_get_event_property_iv(SCREEN_PROPERTY_NAME)");
        return;
    }

    if (name == SCREEN_PROPERTY_SIZE) {
        main_window_resize();
    }
}

/**
 * Send a user event to the main loop.
 * @param   event   The event to send
 */
void
send_event(screen_event_t event)
{
    if (screen_send_event(context, event, getpid()) == -1) {
        perror("screen_send_event");
    }
}

/**
 * Main function.
 * Initializes all components and then runs the event loop
 * @param   argc    Number of arguments
 * @param   argv    Argument vector
 * @return  EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int
main(int argc, char **argv)
{
    // Create the application's screen context.
    if (screen_create_context(&context, SCREEN_APPLICATION_CONTEXT) == -1) {
        perror("screen_create_context");
        return EXIT_FAILURE;
    }

    if (!client_init()) {
        return EXIT_FAILURE;
    }

    if (!gpio_init()) {
        return EXIT_FAILURE;
    }

    // Create the main window.
    if (!main_window_create(context)) {
        return EXIT_FAILURE;
    }

    // Event loop.
    screen_event_t  event;
    screen_create_event(&event);
    for (;;) {
        if (screen_get_event(context, event, -1UL) == -1) {
            perror("screen_get_event");
            return EXIT_FAILURE;
        }

        int type;
        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_TYPE, &type)
            == -1) {
            perror("screen_get_event_property_iv(SCREEN_PROPERTY_TYPE)");
            return EXIT_FAILURE;
        }

        switch (type) {
        case SCREEN_EVENT_MANAGER:
            manager_event(event);
            break;

        case SCREEN_EVENT_POINTER:
            pointer_event(event);
            break;

        case SCREEN_EVENT_USER:
            user_event(event);
            break;

        case SCREEN_EVENT_PROPERTY:
            property_event(event);
            break;
        }
    }

    return EXIT_SUCCESS;
}

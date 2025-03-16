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
#include <sys/rpi_gpio.h>
#include "gpioctrl.h"

#define NUM_PINS    40

#define ROW_HEIGHT      40
#define FIRST_ROW       20
#define FUNC_HEIGHT     30
#define FUNC_WIDTH      70
#define FUNC_LEFT       10
#define FUNC_RIGHT      460
#define FUNC_YOFF       ((ROW_HEIGHT - FUNC_HEIGHT) / 2)
#define BUTTON_HEIGHT   30
#define BUTTON_WIDTH    40
#define BUTTON_LEFT     80
#define BUTTON_RIGHT    (FUNC_RIGHT + BUTTON_LEFT)

/**
 * GPIO control structure.
 */
static struct
{
    /** GPIO number. */
    int id;
    /** Horizontal position. */
    int x;
    /** Vertical position. */
    int y;
    /** GPIO function (input, output, other). */
    int func;
    /** GPIO value */
    int value;
} gpios[NUM_PINS] = {
    { .id = -1 },
    { .id = -1 },
    { .id = 2 },
    { .id = -1 },
    { .id = 3 },
    { .id = -1 },
    { .id = 4 },
    { .id = 14 },
    { .id = -1 },
    { .id = 15 },
    { .id = 17 },
    { .id = 18 },
    { .id = 27 },
    { .id = -1 },
    { .id = 22 },
    { .id = 23 },
    { .id = -1 },
    { .id = 24 },
    { .id = 10 },
    { .id = -1 },
    { .id = 9 },
    { .id = 25 },
    { .id = 11 },
    { .id = 8 },
    { .id = -1 },
    { .id = 7 },
    { .id = 0 },
    { .id = 1 },
    { .id = 5 },
    { .id = -1 },
    { .id = 6 },
    { .id = 12 },
    { .id = 13 },
    { .id = -1 },
    { .id = 19 },
    { .id = 16 },
    { .id = 26 },
    { .id = 20 },
    { .id = -1 },
    { .id = 21 }
};

/** Convert GPIO numbers to pin numbers. */
static int  gpio_to_pin[] = {
    [0] = 27,
    [1] = 28,
    [2] = 3,
    [3] = 5,
    [4] = 7,
    [5] = 29,
    [6] = 31,
    [7] = 26,
    [8] = 24,
    [9] = 21,
    [10] = 19,
    [11] = 23,
    [12] = 32,
    [13] = 33,
    [14] = 8,
    [15] = 10,
    [16] = 36,
    [17] = 11,
    [18] = 12,
    [19] = 35,
    [20] = 38,
    [21] = 40,
    [22] = 15,
    [23] = 16,
    [24] = 18,
    [25] = 22,
    [26] = 37,
    [27] = 13
};

/** Images for the in/out button. */
static cairo_surface_t *func_image[3];

/** Images for the on/off output button. */
static cairo_surface_t *button_image[2];

/** Images for the on/off input indicator. */
static cairo_surface_t *led_image[2];

/**
 * Initialize GPIO controls.
 * @return  true if successful, false otherwise
 */
bool
gpio_init()
{
    // Load images.
    func_image[0] = cairo_image_surface_create_from_png("images/input.png");
    if (cairo_surface_status(func_image[0]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load input image\n");
        return false;
    }

    func_image[1] = cairo_image_surface_create_from_png("images/output.png");
    if (cairo_surface_status(func_image[1]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load output image\n");
        return false;
    }

    func_image[2] = cairo_image_surface_create_from_png("images/alt.png");
    if (cairo_surface_status(func_image[1]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load output image\n");
        return false;
    }

    button_image[0] = cairo_image_surface_create_from_png("images/button_off.png");
    if (cairo_surface_status(button_image[0]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load button off image\n");
        return false;
    }

    button_image[1] = cairo_image_surface_create_from_png("images/button_on.png");
    if (cairo_surface_status(button_image[1]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load button_on image\n");
        return false;
    }

    led_image[0] = cairo_image_surface_create_from_png("images/led_off.png");
    if (cairo_surface_status(led_image[0]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load led off image\n");
        return false;
    }

    led_image[1] = cairo_image_surface_create_from_png("images/led_on.png");
    if (cairo_surface_status(led_image[1]) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load led_on image\n");
        return false;
    }

    // Initialize GPIO structures.
    int y = FIRST_ROW;
    for (unsigned i = 0; i < NUM_PINS; i += 2) {
        gpios[i].y = y;
        gpios[i].x = FUNC_LEFT;
        gpios[i + 1].y = y;
        gpios[i + 1].x = FUNC_RIGHT;
        y += ROW_HEIGHT;
    }

    // Get the current function and value for each GPIO.
    for (unsigned i = 0; i < NUM_PINS; i++) {
        if (gpios[i].id == -1) {
            continue;
        }

        gpios[i].func = client_get_gpio_func(gpios[i].id);
        if (gpios[i].func == RPI_GPIO_FUNC_IN) {
            // Read input value.
            gpios[i].value = client_get_gpio_value(gpios[i].id);

            // Enable value change notifications.
            client_set_gpio_func(gpios[i].id, RPI_GPIO_FUNC_IN);
        } else if (gpios[i].func == RPI_GPIO_FUNC_OUT) {
            // Set output to low.
            gpios[i].value = 0;
            client_set_gpio_value(gpios[i].id, 0);
        } else {
            // ALT func, not supported.
        }
    }

    return true;
}

/**
 * Draw all GPIO controls.
 * @param   cairo   The Cairo drawing context
 */
void
gpio_draw(cairo_t * const cairo)
{
    for (unsigned i = 0; i < NUM_PINS; i++) {
        if (gpios[i].id == -1) {
            continue;
        }

        if (gpios[i].func == RPI_GPIO_FUNC_IN) {
            // Draw an input control.
            cairo_set_source_surface(cairo, func_image[0], gpios[i].x,
                                     gpios[i].y + FUNC_YOFF);
            cairo_paint(cairo);
            cairo_set_source_surface(cairo, led_image[gpios[i].value],
                                     gpios[i].x + 80,
                                     gpios[i].y + FUNC_YOFF);
            cairo_paint(cairo);
        } else if (gpios[i].func == RPI_GPIO_FUNC_OUT) {
            // Draw an output control.
            cairo_set_source_surface(cairo, func_image[1], gpios[i].x,
                                     gpios[i].y + FUNC_YOFF);
            cairo_paint(cairo);
            cairo_set_source_surface(cairo, button_image[gpios[i].value],
                                     gpios[i].x + 80,
                                     gpios[i].y + FUNC_YOFF);
            cairo_paint(cairo);
        } else {
            // ALT func, draw the function button in an undetermined state.
            cairo_set_source_surface(cairo, func_image[2], gpios[i].x,
                                     gpios[i].y + FUNC_YOFF);
            cairo_paint(cairo);
        }
        cairo_paint(cairo);
    }
}

/**
 * Handle a left mouse button release event.
 * @param   event   The mouse pointer event
 * @return  true if a GPIO control was changed, false otherwise
 */
bool
gpio_clicked(screen_event_t event)
{
    // Get the pointer position.
    int pos[2];
    if (screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos)
        == -1) {
        perror("screen_get_event_property_iv(SCREEN_PROEPRTY_POSITION)");
        return false;
    }

    // Determine which column was clicked, if any.
    int col;
    int side;
    if ((pos[0] >= FUNC_LEFT) && (pos[0] < FUNC_LEFT + FUNC_WIDTH)) {
        col = FUNC_LEFT;
        side = 0;
    } else if ((pos[0] >= FUNC_RIGHT) && (pos[0] < FUNC_RIGHT + FUNC_WIDTH)) {
        col = FUNC_RIGHT;
        side = 1;
    } else if ((pos[0] >= BUTTON_LEFT) && (pos[0] < BUTTON_LEFT + BUTTON_WIDTH)) {
        col = BUTTON_LEFT;
        side = 0;
    } else if ((pos[0] >= BUTTON_RIGHT) && (pos[0] < BUTTON_RIGHT + BUTTON_WIDTH)) {
        col = BUTTON_RIGHT;
        side = 1;
    } else {
        return false;
    }

    // Determine which row was clicked.
    pos[1] -= FIRST_ROW;
    int off = pos[1] % ROW_HEIGHT;
    if ((off < FUNC_YOFF) || (off >= (ROW_HEIGHT - FUNC_YOFF))) {
        return false;
    }

    int row = pos[1] / ROW_HEIGHT;
    int gpio = (row * 2) + side;
    if (gpios[gpio].id == -1) {
        return false;
    }

    if ((col == FUNC_LEFT) || (col == FUNC_RIGHT)) {
        // Toggle input/output.
        int func;
        if (gpios[gpio].func == RPI_GPIO_FUNC_OUT) {
            func = RPI_GPIO_FUNC_IN;
        } else {
            // Move from input or ALT to output.
            func = RPI_GPIO_FUNC_OUT;
        }

        if (client_set_gpio_func(gpios[gpio].id, func) == 0) {
            gpios[gpio].func = func;
        }
    } else if (gpios[gpio].func == RPI_GPIO_FUNC_OUT) {
        // Toggle output value.
        gpios[gpio].value = 1 - gpios[gpio].value;
        client_set_gpio_value(gpios[gpio].id, gpios[gpio].value);
    } else {
        // No action for toggling input.
        return false;
    }

    return true;
}

/**
 * Update the value of an input GPIO.
 * Called in response to a notification sent from the client thread.
 * @param   gpio    The GPIO number
 * @param   value   The new value
 * @return  true if the GPIO was updated, false otherwise
 */
bool
gpio_update_value(int const gpio, int const value)
{
    if (gpio >= (sizeof(gpio_to_pin) / sizeof(gpio_to_pin[0]))) {
        return false;
    }

    int const   pin = gpio_to_pin[gpio] - 1;
    if (gpios[pin].func != RPI_GPIO_FUNC_IN) {
        return false;
    }

    if (gpios[pin].value == value) {
        return false;
    }

    gpios[pin].value = value;
    return true;
}

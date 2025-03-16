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
#include "gpioctrl.h"

/** Window handle. */
static screen_window_t  window;

/** Window buffer handle. */
static screen_buffer_t  buffers[2];

/** Index of the buffer used for drawing. */
static unsigned         cur_buffer;

/** Cairo surface for the background image. */
static cairo_surface_t *background;

/** Current window width. */
static int              window_width;

/** Current window height. */
static int              window_height;

/** Drawing area width, as read from the background image. */
static int              image_width;

/** Drawing area height, as read from the background image. */
static int              image_height;

/**
 * Create the main window.
 * @param   context     The screen context for the application
 * @return  true if successful, false otherwise
 */
bool
main_window_create(screen_context_t context)
{
    // Load the background image.
    background = cairo_image_surface_create_from_png("images/background.png");
    if (cairo_surface_status(background) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to load background image\n");
        return false;
    }

    image_width = cairo_image_surface_get_width(background);
    image_height = cairo_image_surface_get_height(background);
    window_width = image_width;
    window_height = image_height;

    // Create the window.
    if (screen_create_window(&window, context) == -1) {
        perror("screen_create_context");
        return false;
    }

    // Set window properties.
    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_SIZE,
                                      (int[]){ window_width, window_height })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_SIZE)");
        return false;
    }

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_SOURCE_SIZE,
                                      (int[]){ window_width, window_height })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_SIZE)");
        return false;
    }

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_USAGE,
                                      (const int[]){ SCREEN_USAGE_WRITE })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        return false;
    }

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_FORMAT,
                                      (const int[]){ SCREEN_FORMAT_RGBA8888 })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        return false;
    }

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_COLOR,
                                      (const int[]){ 0xff104090 })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        return false;
    }

    // Create the window buffer.
    if (screen_create_window_buffers(window, 2) == -1) {
        perror("screen_create_window_buffers");
        return false;
    }

    if (screen_get_window_property_pv(window, SCREEN_PROPERTY_BUFFERS,
                                      (void **)buffers) == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_USAGE)");
        return false;
    }

    // Make the window visible.
    main_window_draw();

    // Connect to the window manager.
    if (screen_manage_window(window, "Frame=Y") == 0) {
        screen_event_t event;
        screen_create_event(&event);

        char title[] = "Title=gpioctrl";
        screen_set_event_property_iv(event, SCREEN_PROPERTY_TYPE,
                                     (const int[]){ SCREEN_EVENT_MANAGER });
        screen_set_event_property_cv(event, SCREEN_PROPERTY_USER_DATA,
                                     sizeof(title), title);
        screen_set_event_property_pv(event, SCREEN_PROPERTY_WINDOW,
                                     (void **)&window);
        screen_set_event_property_pv(event, SCREEN_PROPERTY_CONTEXT,
                                     (void **)&context);

        screen_inject_event(NULL, event);
    }

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_VISIBLE,
                                      (const int[]){ 1 })
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_VISIBLE)");
        return false;
    }

    return true;
}

/**
 * Draw the window.
 * @return  true if successful, false otherwise
 */
bool
main_window_draw(void)
{
    screen_buffer_t buffer = buffers[cur_buffer];

    // Get access to the window's buffer.
    uint8_t *ptr;
    if (screen_get_buffer_property_pv(buffer, SCREEN_PROPERTY_POINTER,
                                      (void **)&ptr) == -1) {
        perror("screen_get_buffer_property_pv(SCREEN_PROPERTY_POINTER)");
        return false;
    }

    int stride;
    if (screen_get_buffer_property_iv(buffer, SCREEN_PROPERTY_STRIDE, &stride)
        == -1) {
        perror("screen_get_buffer_property_iv(SCREEN_PROPERTY_STRIDE)");
        return false;
    }

    // Create Cairo drawing objects.
    cairo_surface_t *surface =
        cairo_image_surface_create_for_data(ptr, CAIRO_FORMAT_ARGB32,
                                            image_width, image_height, stride);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo surface\n");
        return false;
    }

    cairo_t *cairo = cairo_create(surface);
    if (cairo == NULL) {
        fprintf(stderr, "Failed to create Cairo context\n");
        return false;
    }

    // Draw background.
    cairo_set_source_surface(cairo, background, 0, 0);
    cairo_paint(cairo);

    // Draw all GPIO controls.
    gpio_draw(cairo);

    cairo_surface_destroy(surface);
    cairo_destroy(cairo);

    // Post the window after the update.
    screen_post_window(window, buffer, 0, NULL, 0);
    cur_buffer = 1 - cur_buffer;

    return true;
}

/**
 * Handle window resizing in response to a window manager event.
 */
bool
main_window_resize()
{
    // Get the new size.
    int size[2];
    if (screen_get_window_property_iv(window, SCREEN_PROPERTY_SIZE, size)
        == -1) {
        perror("screen_get_window_property_iv(SCREEN_PROPERTY_SIZE)");
        return false;
    }

    // Update source and buffer sizes.
    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_SOURCE_SIZE, size)
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_SIZE)");
        return false;
    }

    // Position content at the centre of the window.
    window_width = size[0];
    window_height = size[1];

    int pos[2] = { -((window_width - image_width) / 2),
                   -((window_height - image_height) / 2) };

    if (screen_set_window_property_iv(window, SCREEN_PROPERTY_SOURCE_POSITION, pos)
        == -1) {
        perror("screen_set_window_property_iv(SCREEN_PROPERTY_SOURCE_POSITION)");
        return false;
    }

    // Redraw the window.
    main_window_draw();

    return true;
}

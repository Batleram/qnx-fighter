/*
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/neutrino.h>

#include "public/rpi_ws281x.h"

// Driver mode definitions
// PWM/PCM/DMA driver mode removed (for now) from QNX version of this library
// enums and structure content adjusted accordingly
#define NONE 0
#define SPI 3

// Internal LED handling constants (some tweaks have been made to accomodate the QNX SPI methodology)

#define LED_STRIP_SPI_DEVICE_MODE 0b00010000010000100000

#define LED_ZERO 0b11000000
#define LED_ONE 0b11111100
#define PREAMBLE_BYTES 44

#define LED_RESET_NS 100000

// 4 colors (R, G, B + W), 8 bits per byte, 3 symbols per bit + 55uS low for reset signal
#define LED_COLORS 4

#define LED_COUNT(leds) (leds * LED_COLORS)
#define LED_BIT_COUNT(leds) (leds * LED_COLORS * 8)

// Minimum time to wait for reset to occur in microseconds.
#define LED_RESET_WAIT_TIME 300

// Pad out to the nearest uint32 + 32-bits for idle low/high times the number of channels
#define SPI_BYTE_COUNT(leds) (PREAMBLE_BYTES + ((LED_BIT_COUNT(leds) & ~0x7) + 4) + 4)

typedef struct ws2811_device
{
    int driver_mode;
    uint8_t *pxl_raw;
    int spi_bus_number[LED_STRIP_CHANNELS];
    int spi_device_number[LED_STRIP_CHANNELS];
    int max_count;
} ws2811_device_t;

// QNX-specific SPI constants
#define LED_STRIP_SPI_BUS_1 0
#define LED_STRIP_SPI_BUS_2 3
#define LED_STRIP_SPI_BUS_3 1
#define LED_STRIP_SPI_DEVICE 0

// Provides monotonic timestamp in microseconds.
static uint64_t get_microsecond_timestamp()
{
    struct timespec t;

    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
    {
        return 0;
    }

    return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

static void ws2811_cleanup(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    int chan;

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        if (ws2811->channel[chan].leds)
        {
            free(ws2811->channel[chan].leds);
        }
        ws2811->channel[chan].leds = NULL;

        if (ws2811->channel[chan].gamma)
        {
            free(ws2811->channel[chan].gamma);
        }
        ws2811->channel[chan].gamma = NULL;

        if (device && (device->spi_bus_number[chan] >= 0))
        {
            rpi_spi_cleanup_device(device->spi_bus_number[chan], device->spi_device_number[chan]);
        }
    }

    if (device)
    {
        free(device);
    }

    ws2811->device = NULL;
}

static ws2811_return_t spi_init(ws2811_t *ws2811)
{
    int chan;
    ws2811_device_t *device = ws2811->device;

    // initialize SPI devices indicated in the provided configuration
    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        if (device->spi_bus_number[chan] != -1)
        {
            if (rpi_spi_configure_device(device->spi_bus_number[chan], device->spi_device_number[chan], LED_STRIP_SPI_DEVICE_MODE, ws2811->freq))
            {
                return WS2811_ERROR_SPI_SETUP;
            }
        }
    }

    // Allocate SPI transmit buffer (same size as PCM)
    device->pxl_raw = malloc(SPI_BYTE_COUNT(device->max_count));
    if (device->pxl_raw == NULL)
    {
        ws2811_cleanup(ws2811);
        return WS2811_ERROR_OUT_OF_MEMORY;
    }

    uint32_t *pxl_raw = (uint32_t *)device->pxl_raw;
    int maxcount = device->max_count;
    int wordcount = LED_COUNT(maxcount) / sizeof(uint32_t);
    int i;

    for (i = 0; i < wordcount; i++)
    {
        pxl_raw[i] = 0x0;
    }

    return WS2811_SUCCESS;
}

static ws2811_return_t spi_transfer(ws2811_t *ws2811, int chan)
{
    // initialize SPI devices indicated in the provided configuration
    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        if (ws2811->device->spi_bus_number[chan] != -1)
        {
            if (rpi_spi_write_read_data(ws2811->device->spi_bus_number[chan], ws2811->device->spi_device_number[chan],
                                        ws2811->device->pxl_raw, SPI_BYTE_COUNT(ws2811->device->max_count)))
            {
                return WS2811_ERROR_SPI_TRANSFER;
            }
        }
    }

    return WS2811_SUCCESS;
}

ws2811_return_t ws2811_init(ws2811_t *ws2811)
{
    ws2811_device_t *device;
    int chan;

    ws2811->device = malloc(sizeof(*ws2811->device));
    if (!ws2811->device)
    {
        return WS2811_ERROR_OUT_OF_MEMORY;
    }

    memset(ws2811->device, 0, sizeof(*ws2811->device));
    device = ws2811->device;

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        device->spi_bus_number[chan] = -1;
        device->spi_device_number[chan] = -1;
    }

    // the SPI support currently implemented should work on both RPI 4 and RPI 5 and related devices
    // only support GPIO pins for SPI (for now)
    if (ws2811->channel[0].gpionum == LED_CHANNEL_0_DATA_PIN || ws2811->channel[0].gpionum == LED_CHANNEL_1_DATA_PIN)
    {
        ws2811->device->driver_mode = SPI;
        if (ws2811->channel[0].gpionum == LED_CHANNEL_0_DATA_PIN)
        {
            device->spi_bus_number[0] = LED_STRIP_SPI_BUS_1;
        }
        else if (ws2811->channel[0].gpionum == LED_CHANNEL_1_DATA_PIN)
        {
            device->spi_bus_number[0] = LED_STRIP_SPI_BUS_2;
        }
        else if (ws2811->channel[0].gpionum == LED_CHANNEL_2_DATA_PIN)
        {
            device->spi_bus_number[0] = LED_STRIP_SPI_BUS_3;
        }
        device->spi_device_number[0] = LED_STRIP_SPI_DEVICE;
    }
    else
    {
        return WS2811_ERROR_ILLEGAL_GPIO;
    }

    if (ws2811->channel[1].gpionum != -1)
    {
        if (ws2811->channel[1].gpionum == LED_CHANNEL_0_DATA_PIN || ws2811->channel[1].gpionum == LED_CHANNEL_1_DATA_PIN)
        {
            ws2811->device->driver_mode = SPI;
            if (ws2811->channel[1].gpionum == LED_CHANNEL_0_DATA_PIN)
            {
                device->spi_bus_number[1] = LED_STRIP_SPI_BUS_1;
            }
            else if (ws2811->channel[1].gpionum == LED_CHANNEL_1_DATA_PIN)
            {
                device->spi_bus_number[1] = LED_STRIP_SPI_BUS_2;
            }
            else if (ws2811->channel[1].gpionum == LED_CHANNEL_2_DATA_PIN)
            {
                device->spi_bus_number[1] = LED_STRIP_SPI_BUS_3;
            }
            device->spi_device_number[1] = LED_STRIP_SPI_DEVICE;
        }
        else
        {
            return WS2811_ERROR_ILLEGAL_GPIO;
        }
    }

    device->max_count = 0;
    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        if (ws2811->channel[chan].count > device->max_count)
        {
            device->max_count = ws2811->channel[chan].count;
        }
    }

    // Initialize all pointers to NULL.  Any non-NULL pointers will be freed on cleanup.
    device->pxl_raw = NULL;

    // Allocate the LED buffers
    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        if (channel->gpionum == LED_CHANNEL_0_DATA_PIN ||
            channel->gpionum == LED_CHANNEL_1_DATA_PIN ||
            channel->gpionum == LED_CHANNEL_2_DATA_PIN)
        {
            if (!channel->leds)
            {
                channel->leds = malloc(sizeof(ws2811_led_t) * channel->count);
            }
            if (!channel->leds)
            {
                ws2811_cleanup(ws2811);
                return WS2811_ERROR_OUT_OF_MEMORY;
            }

            memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

            if (!channel->strip_type)
            {
                channel->strip_type = WS2811_STRIP_RGB;
            }

            // Set default color correction, temperature and gamma lookup table
            if (!channel->color_correction)
            {
                channel->color_correction = (255 << LED_SHIFT_W) +
                                            (255 << LED_SHIFT_R) +
                                            (255 << LED_SHIFT_G) +
                                            (255 << LED_SHIFT_B);
            }

            if (!channel->color_temperature)
            {
                channel->color_temperature = ((int)255 << LED_SHIFT_W) +
                                             ((int)255 << LED_SHIFT_R) +
                                             ((int)255 << LED_SHIFT_G) +
                                             ((int)255 << LED_SHIFT_B);
            }

            if (!channel->gamma_factor)
            {
                channel->gamma_factor = 1.0;
            }

            if (!channel->gamma)
            {
                channel->gamma = malloc(sizeof(uint8_t) * 256 * LED_COLORS);
                for (int x = 0; x < 256; x++)
                {
                    for (int j = 0; j < LED_COLORS; j++)
                    {
                        channel->gamma[x * LED_COLORS + j] = x;
                    }
                }
            }

            channel->wshift = (channel->strip_type >> 24) & 0xff;
            channel->rshift = (channel->strip_type >> 16) & 0xff;
            channel->gshift = (channel->strip_type >> 8) & 0xff;
            channel->bshift = (channel->strip_type >> 0) & 0xff;
        }
    }

    switch (device->driver_mode)
    {
    case SPI:
        return spi_init(ws2811);
    default:
        break;
    }

    return WS2811_SUCCESS;
}

void ws2811_fini(ws2811_t *ws2811)
{
    ws2811_wait(ws2811);

    ws2811_cleanup(ws2811);
}

ws2811_return_t ws2811_render(ws2811_t *ws2811)
{
    static uint8_t convert_table[256][8] =
        {
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE, LED_ONE},

            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO, LED_ONE},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ZERO},
            {LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE, LED_ONE}

        };
    volatile uint8_t *pxl_raw = ws2811->device->pxl_raw;
    int driver_mode = ws2811->device->driver_mode;
    int i, l, chan;
    unsigned j;
    ws2811_return_t ret = WS2811_SUCCESS;
    uint32_t protocol_time = 0;
    static uint64_t previous_timestamp = 0;

    // Wait for any previous operation to complete.
    if ((ret = ws2811_wait(ws2811)) != WS2811_SUCCESS)
    {
        return ret;
    }

    if (ws2811->render_wait_time != 0)
    {
        const uint64_t current_timestamp = get_microsecond_timestamp();
        uint64_t time_diff = current_timestamp - previous_timestamp;

        if (ws2811->render_wait_time > time_diff)
        {
            usleep(ws2811->render_wait_time - time_diff);
        }
    }

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        const int scale = (channel->brightness & 0xff) + 1;
        uint8_t array_size = 3; // Assume 3 color LEDs, RGB

        // If our shift mask includes the highest nibble, then we have 4 LEDs, RBGW.
        if (channel->strip_type & SK6812_SHIFT_WMASK)
        {
            array_size = 4;
        }

        // 1.25µs per bit
        const uint32_t channel_protocol_time = channel->count * array_size * 8 * 1.25;

        // Only using the channel which takes the longest as both run in parallel
        if (channel_protocol_time > protocol_time)
        {
            protocol_time = channel_protocol_time;
        }

        for (i = 0; i < channel->count; i++) // Led
        {
            uint8_t color[LED_COLORS];
            for (j = 0; j < array_size; j++) // Color
            {
                switch (j)
                {
                case 0: // red
                    color[j] = channel->gamma[((((channel->leds[i] >> channel->rshift) & 0xff) * scale) >> 8) * LED_COLORS + j];
                    break;

                case 1: // green
                    color[j] = channel->gamma[((((channel->leds[i] >> channel->gshift) & 0xff) * scale) >> 8) * LED_COLORS + j];
                    break;

                case 2: // blue
                    color[j] = channel->gamma[((((channel->leds[i] >> channel->bshift) & 0xff) * scale) >> 8) * LED_COLORS + j];
                    break;

                case 3: // white
                    color[j] = channel->gamma[((((channel->leds[i] >> channel->wshift) & 0xff) * scale) >> 8) * LED_COLORS + j];
                    break;
                }
            }
            // printf("led color: %x\n", channel->leds[i]);
            // printf("color: %x %x %x %x\n", color[0], color[1], color[2], color[3]);

            for (j = 0; j < array_size; j++) // Color
            {
                for (l = 0; l < 8; ++l)
                {
                    uint8_t val = convert_table[color[j]][l];
                    if (channel->invert)
                        val = ~val;

                    pxl_raw[PREAMBLE_BYTES + i * array_size * 8 + j * 8 + l] = val;
                }
            }
        }

        if (driver_mode == SPI)
        {
            ret = spi_transfer(ws2811, chan);
            if (ret)
            {
                break;
            }
        }
    }

    // LED_RESET_WAIT_TIME is added to allow enough time for the reset to occur.
    previous_timestamp = get_microsecond_timestamp();
    ws2811->render_wait_time = protocol_time + LED_RESET_WAIT_TIME;

    return ret;
}

ws2811_return_t ws2811_wait(ws2811_t *ws2811)
{
    // Nothing to do for SPI but return error for other driver modes just in case
    if (ws2811->device->driver_mode != SPI)
    {
        return WS2811_ERROR_GENERIC;
    }

    return WS2811_SUCCESS;
}

const char *ws2811_get_return_t_str(const ws2811_return_t state)
{
    const int index = -state;
    static const char *const ret_state_str[] = {WS2811_RETURN_STATES(WS2811_RETURN_STATES_STRING)};

    if (index < (int)(sizeof(ret_state_str) / sizeof(ret_state_str[0])))
    {
        return ret_state_str[index];
    }

    return "";
}

void ws2811_init_gamma_lookup(ws2811_t *ws2811)
{
    int chan, counter;
    uint8_t color_factor;
    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        if (channel->gamma)
        {
            for (counter = 0; counter < 256; counter++)
            {
                for (int j = 0; j < LED_COLORS; j++)
                {
                    switch (j)
                    {
                    case 0: // red
                        color_factor = (uint8_t)(((double)((channel->color_correction >> LED_SHIFT_R) & 0xff) *
                                                  (double)((channel->color_temperature >> LED_SHIFT_R) & 0xff)) /
                                                 255.0);
                        break;

                    case 1: // green
                        color_factor = (uint8_t)(((double)((channel->color_correction >> LED_SHIFT_G) & 0xff) *
                                                  (double)((channel->color_temperature >> LED_SHIFT_G) & 0xff)) /
                                                 255.0);
                        break;

                    case 2: // blue
                        color_factor = (uint8_t)(((double)((channel->color_correction >> LED_SHIFT_B) & 0xff) *
                                                  (double)((channel->color_temperature >> LED_SHIFT_B) & 0xff)) /
                                                 255.0);
                        break;

                    case 3: // white
                        color_factor = (uint8_t)(((double)((channel->color_correction >> LED_SHIFT_W) & 0xff) *
                                                  (double)((channel->color_temperature >> LED_SHIFT_W) & 0xff)) /
                                                 255.0);
                        break;
                    }

                    channel->gamma[counter * LED_COLORS + j] =
                        (int)(pow((float)color_factor * (float)counter / (float)(255.00 * 255.0), channel->gamma_factor) * 255.00 + 0.5);
                }
            }
        }
    }
}

void ws2811_set_color_correction(ws2811_t *ws2811, ws2811_led_t color_correction)
{
    int chan;

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        channel->color_correction = color_correction;
    }

    ws2811_init_gamma_lookup(ws2811);
}

void ws2811_set_color_temperature(ws2811_t *ws2811, ws2811_led_t color_temperature)
{
    int chan;

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        channel->color_temperature = color_temperature;
    }

    ws2811_init_gamma_lookup(ws2811);
}

void ws2811_set_custom_gamma_factor(ws2811_t *ws2811, double gamma_factor)
{
    int chan;

    for (chan = 0; chan < LED_STRIP_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        channel->gamma_factor = gamma_factor;
    }

    ws2811_init_gamma_lookup(ws2811);
}

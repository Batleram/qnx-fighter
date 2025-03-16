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


#ifndef __WS2811_H__
#define __WS2811_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "rpi_spi.h"

/* 
  The WS2811_TARGET_FREQ constant below was modified for the adaptation of SPI functionality to QNX SDP 8.0.
  Frequency and other QNX-specific changes were derived from the following project:

  https://github.com/niklasr22/rpi5-ws2812
*/
#define WS2811_TARGET_FREQ                       6500000   // SPI frequency for LED strip data

// 4 color R, G, B and W ordering
#define SK6812_STRIP_RGBW                        0x18100800
#define SK6812_STRIP_RBGW                        0x18100008
#define SK6812_STRIP_GRBW                        0x18081000
#define SK6812_STRIP_GBRW                        0x18080010
#define SK6812_STRIP_BRGW                        0x18001008
#define SK6812_STRIP_BGRW                        0x18000810
#define SK6812_SHIFT_WMASK                       0xf0000000

// 3 color R, G and B ordering
#define WS2811_STRIP_RGB                         0x00100800
#define WS2811_STRIP_RBG                         0x00100008
#define WS2811_STRIP_GRB                         0x00081000
#define WS2811_STRIP_GBR                         0x00080010
#define WS2811_STRIP_BRG                         0x00001008
#define WS2811_STRIP_BGR                         0x00000810

// predefined fixed LED types
#define WS2812_STRIP                             WS2811_STRIP_GRB
#define SK6812_STRIP                             WS2811_STRIP_GRB
#define SK6812W_STRIP                            SK6812_STRIP_GRBW

// LED Channel info and pins
#define LED_STRIP_CHANNELS 3

#define LED_CHANNEL_0_DATA_PIN  SPI0_MOSI
#define LED_CHANNEL_1_DATA_PIN  SPI3_MOSI
#define LED_CHANNEL_2_DATA_PIN  SPI1_MOSI

#define LED_SHIFT_W 24
#define LED_SHIFT_R 16
#define LED_SHIFT_G 8
#define LED_SHIFT_B 0

struct ws2811_device;

typedef uint32_t ws2811_led_t;                   //< 0xWWRRGGBB
typedef struct ws2811_channel_t
{
    int gpionum;                                 //< GPIO Pin with PWM alternate function, 0 if unused
    int invert;                                  //< Invert output signal
    int count;                                   //< Number of LEDs, 0 if channel is unused
    int strip_type;                              //< Strip color layout -- one of WS2811_STRIP_xxx constants
    ws2811_led_t *leds;                          //< LED buffers, allocated by driver based on count
    uint8_t brightness;                          //< Brightness value between 0 and 255
    uint8_t wshift;                              //< White shift value
    uint8_t rshift;                              //< Red shift value
    uint8_t gshift;                              //< Green shift value
    uint8_t bshift;                              //< Blue shift value
    uint8_t *gamma;                              //< Gamma correction table
    /* added extra fields to support color correction and temperature settings
       for deriving extended gamma correction */
    double gamma_factor;
    ws2811_led_t color_correction;
    ws2811_led_t color_temperature;
} ws2811_channel_t;

// RPI hardware checking and PWM/PCM/DMA driver mode removed (for now) from QNX library
typedef struct ws2811_t
{
    uint64_t render_wait_time;                   //< time in µs before the next render can run
    struct ws2811_device *device;                //< Private data for driver use
    uint32_t freq;                               //< Required output frequency
    ws2811_channel_t channel[LED_STRIP_CHANNELS];
} ws2811_t;

#define WS2811_RETURN_STATES(X)                                                             \
            X(0, WS2811_SUCCESS, "Success"),                                                \
            X(-1, WS2811_ERROR_GENERIC, "Generic failure"),                                 \
            X(-2, WS2811_ERROR_OUT_OF_MEMORY, "Out of memory"),                             \
            X(-11, WS2811_ERROR_ILLEGAL_GPIO, "Selected GPIO not possible"),                \
            X(-12, WS2811_ERROR_PCM_SETUP, "Unable to initialize PCM"),                     \
            X(-13, WS2811_ERROR_SPI_SETUP, "Unable to initialize SPI"),                     \
            X(-14, WS2811_ERROR_SPI_TRANSFER, "SPI transfer error")                         \

#define WS2811_RETURN_STATES_ENUM(state, name, str) name = state
#define WS2811_RETURN_STATES_STRING(state, name, str) str

typedef enum {
    WS2811_RETURN_STATES(WS2811_RETURN_STATES_ENUM),

    WS2811_RETURN_STATE_COUNT
} ws2811_return_t;

/**
 * Allocate and initialize memory, buffers, pages, and hardware for driving LEDs.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 otherwise.
 */
ws2811_return_t ws2811_init(ws2811_t *ws2811);

/**
 * Shut down SPI logic and cleanup memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
void ws2811_fini(ws2811_t *ws2811);

/**
 * Render the pixel buffer from the user supplied LED arrays.
 * This will update all LEDs on both PWM channels.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
ws2811_return_t ws2811_render(ws2811_t *ws2811);

/**
 * Wait for any executing operation to complete before returning.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 on completion error
 */
ws2811_return_t ws2811_wait(ws2811_t *ws2811);

/**
 * Return string representation of API return codes.
 *
 * @param    state  return code / state
 *
 * @returns  0 on success, -1 on completion error
 */
const char * ws2811_get_return_t_str(const ws2811_return_t state);

/**
 * Set a gamma factor to correct for LED brightness levels.
 *
 * @param    ws2811  ws2811 instance pointer.
 * @param    gamma_factor  gamma correction factor 
 *
 * @returns  0 on success, -1 on completion error
 */
void ws2811_set_custom_gamma_factor(ws2811_t *ws2811, double gamma_factor);

/* New capabilities added to the library to aid with the port of another LED library integrating this library */
/**
 * Set a color correction factor to correct for LED brightness levels.
 *
 * @param    ws2811  ws2811 instance pointer.
 * @param    color_correction  color correction 
 *
 * @returns  0 on success, -1 on completion error
 */
void ws2811_set_color_correction(ws2811_t *ws2811, ws2811_led_t color_correction);

/**
 * Set a color temperature factor to correct for LED brightness levels.
 *
 * @param    ws2811  ws2811 instance pointer.
 * @param    color_temperature  color temperature 
 *
 * @returns  0 on success, -1 on completion error
 */
void ws2811_set_color_temperature(ws2811_t *ws2811, ws2811_led_t color_temperature);

#ifdef __cplusplus
}
#endif

#endif /* __WS2811_H__ */

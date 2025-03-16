# librpi_ws281x

This folder contains a API API to write colors to a collection of WS 281x LEDs.  It is suitable for simpler control of
one or more LED strips, or one or more LED matrices, but it does not contain specialized logic for managing matrices in a transparent fashion.

This library is a smaller port of the Linux library of the same name [https://github.com/jgarff/rpi_ws281x](https://github.com/jgarff/rpi_ws281x)
but the port focuses only on the SPI driver of the original library and the implementation of the SPI logic is based on [librpi_spi](../librpi_spi).
The interface has been mostly retained but some minor changes were made because the port only covers SPI functionality.

## APIs

### ws2811_init

Allocate and initialize memory, buffers, pages, and hardware for driving LEDs.

### ws2811_fini

Shut down SPI logic and cleanup memory.

### ws2811_render

Render the pixel buffer from the user supplied LED arrays.
This will update all LEDs on both PWM channels.

### ws2811_wait

Wait for any executing operation to complete before returning.

### ws2811_get_return_t_str

Return string representation of API return codes.

### ws2811_set_custom_gamma_factor

Set a gamma factor to correct for LED brightness levels.

### ws2811_set_color_correction

Set a color correction factor to correct for LED brightness levels.

(not in original library)

### ws2811_set_color_temperature

Set a color temperature factor to correct for LED brightness levels.

(not in original library)

---
See [rpi_ws281x.h](public/rpi_ws281x.h) for more details.

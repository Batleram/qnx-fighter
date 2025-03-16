/*
 * Copyright (c) 2024, BlackBerry Limited. All rights reserved.
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


 #ifndef RPI_SPI_API_H
 #define RPI_SPI_API_H
 
#include <hw/io-spi.h>

// SPI GPIO pins for RaspBerry PI 4 / 5
#define SPI0_CE0 8
#define SPI0_CE1 7
#define SPI0_MOSI 10
#define SPI0_MISO 9
#define SPI0_SCLK 7
#define SPI1_CE0 18
#define SPI1_CE1 17
#define SPI1_CE2 16
#define SPI1_MOSI 20
#define SPI1_MISO 19
#define SPI1_SCLK 21
#define SPI3_CE0 0
#define SPI3_CE1 24
#define SPI3_MOSI 2
#define SPI3_MISO 1
#define SPI3_SCLK 3

/* Return codes for client API */
#define SPI_SUCCESS 0
#define SPI_ERROR_NOT_CONNECTED -1
#define SPI_ERROR_BAD_ARGUMENT -2
#define SPI_ERROR_OPERATION_FAILED -3

/**
 * Query the SPI driver
 *
 * @param    bus_number      SPI bus number
 * @param    device_number   SPI device number
 * @param    driver_info     driver info (output)
 *
 * @returns  SPI_SUCCESS                on success,
 *           SPI_ERROR_NOT_CONNECTED    if the SPI device is not available to connect to
 *           SPI_ERROR_BAD_ARGUMENT     invalid driver info structure pointer provided
 *           SPI_ERROR_OPERATION_FAILED SPI operation failed
 */
int rpi_spi_get_driver_info(unsigned bus_number, unsigned device_number, spi_drvinfo_t *driver_info);

/**
 * Query the SPI device
 *
 * @param    bus_number      SPI bus number
 * @param    device_number   SPI device number
 * @param    driver_info     device info (output)
 *
 * @returns  SPI_SUCCESS                on success,
 *           SPI_ERROR_NOT_CONNECTED    if the SPI device is not available to connect to
 *           SPI_ERROR_BAD_ARGUMENT     invalid device info structure pointer provided
 *           SPI_ERROR_OPERATION_FAILED SPI operation failed
 */
int rpi_spi_get_device_info(unsigned bus_number, unsigned device_number, spi_devinfo_t *device_info);

/**
 * Configure the SPI device
 *
 * @param    bus_number          SPI bus number
 * @param    device_number       SPI device number
 * @param    mode                SPI device mode
 * @param    spi_device_speed_hz SPI device speed in Hz
 *
 * @returns  SPI_SUCCESS                on success,
 *           SPI_ERROR_NOT_CONNECTED    if the SPI device is not available to connect to
 *           SPI_ERROR_OPERATION_FAILED SPI operation failed
 */
int rpi_spi_configure_device(unsigned bus_number, unsigned device_number, unsigned mode, uint32_t spi_device_speed_hz);

/**
 * Write/read data to/from the SPI interface
 *
 * @param    bus_number          SPI bus number
 * @param    device_number       SPI device number
 * @param    data_buffer         pointer to buffer of data to write
 * @param    data_size           data buffer size
 *
 * @returns  SPI_SUCCESS                on success,
 *           SPI_ERROR_NOT_CONNECTED    if the SPI device is not available to connect to
 *           SPI_ERROR_OPERATION_FAILED SPI operation failed
 */
int rpi_spi_write_read_data(unsigned bus_number, unsigned device_number, uint8_t *data_buffer, uint32_t data_size);

/**
 * Cleanup from using the SPI device
 *
 * @param    bus_number          SPI bus number
 * @param    device_number       SPI device number
 *
 * @returns  SPI_SUCCESS                on success,
 *           SPI_ERROR_NOT_CONNECTED    if the SPI device is not available to connect to
 */
int rpi_spi_cleanup_device(unsigned bus_number, unsigned device_number);

#endif
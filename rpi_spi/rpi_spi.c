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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "public/rpi_spi.h"

#define SPI_DEVICE_FILENAME_FORMAT "/dev/io-spi/spi%d/dev%d"

#define MAX_SPI_BUSES 6
#define MAX_SPI_BUS_DEVICES 10 // should be good enough to start with

static int spi_device_fd[MAX_SPI_BUSES][MAX_SPI_BUS_DEVICES] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

// Mutex protecting the SPI device file descriptors
static pthread_mutex_t spi_fd_mutex;

/* Open the SPI device */
static int
open_spi_device_fd(unsigned bus_number, unsigned device_number)
{
    char spi_device_name[20] = {0};

    sprintf(spi_device_name, SPI_DEVICE_FILENAME_FORMAT, bus_number, device_number);

    pthread_mutex_lock(&spi_fd_mutex);

    if (spi_device_fd[bus_number][device_number] == -1)
    {
        spi_device_fd[bus_number][device_number] = open(spi_device_name, O_RDWR);
        if (spi_device_fd[bus_number][device_number] < 0)
        {
            perror("open");
            return SPI_ERROR_NOT_CONNECTED;
        }
    }

    pthread_mutex_unlock(&spi_fd_mutex);

    return SPI_SUCCESS;
}

/* Close the SPI device */
static int
close_spi_device_fd(unsigned bus_number, unsigned device_number)
{
    pthread_mutex_lock(&spi_fd_mutex);

    if (spi_device_fd[bus_number][device_number] != -1)
    {
        int err = close(spi_device_fd[bus_number][device_number]);
        if (err != EOK)
        {
            perror("close");
            return SPI_ERROR_NOT_CONNECTED;
        }
        spi_device_fd[bus_number][device_number] = -1;
    }

    pthread_mutex_unlock(&spi_fd_mutex);

    return SPI_SUCCESS;
}

int rpi_spi_get_driver_info(unsigned bus_number, unsigned device_number, spi_drvinfo_t *driver_info)
{
    int err;

    if (open_spi_device_fd(bus_number, device_number))
    {
        perror("open_spi_device_fd");
        return SPI_ERROR_NOT_CONNECTED;
    }

    if (driver_info == NULL)
    {
        perror("invalid driver_info");
        return SPI_ERROR_BAD_ARGUMENT;
    }

    // Send the SPI message
    err = devctl(spi_device_fd[bus_number][device_number], DCMD_SPI_GET_DRVINFO, driver_info, sizeof(spi_drvinfo_t), NULL);
    if (err != EOK)
    {
        perror("devctl");
        return SPI_ERROR_OPERATION_FAILED;
    }

    return SPI_SUCCESS;
}

int rpi_spi_get_device_info(unsigned bus_number, unsigned device_number, spi_devinfo_t *device_info)
{
    int err;

    if (open_spi_device_fd(bus_number, device_number))
    {
        perror("open_spi_device_fd");
        return SPI_ERROR_NOT_CONNECTED;
    }

    if (device_info == NULL)
    {
        perror("invalid device_info");
        return SPI_ERROR_BAD_ARGUMENT;
    }

    // Send the SPI message
    err = devctl(spi_device_fd[bus_number][device_number], DCMD_SPI_GET_DEVINFO, device_info, sizeof(spi_devinfo_t), NULL);
    if (err != EOK)
    {
        perror("devctl");
        return SPI_ERROR_OPERATION_FAILED;
    }

    return SPI_SUCCESS;
}

int rpi_spi_configure_device(unsigned bus_number, unsigned device_number, unsigned mode, uint32_t spi_device_speed_hz)
{
    int err;

    if (open_spi_device_fd(bus_number, device_number))
    {
        perror("open_spi_device_fd");
        return SPI_ERROR_NOT_CONNECTED;
    }

    // Configure the SPI device according to supplied parameters
    spi_cfg_t spi_device_cfg = {
        .mode = mode,
        .clock_rate = spi_device_speed_hz};

    // Send the SPI message
    err = devctl(spi_device_fd[bus_number][device_number], DCMD_SPI_SET_CONFIG, &spi_device_cfg, sizeof(spi_cfg_t), NULL);
    if (err != EOK)
    {
        perror("devctl");
        fprintf(stderr, "error: %d\n", err);
        return SPI_ERROR_OPERATION_FAILED;
    }

    return SPI_SUCCESS;
}

int rpi_spi_write_read_data(unsigned bus_number, unsigned device_number, uint8_t *data_buffer, uint32_t data_size)
{
    int err;

    if (open_spi_device_fd(bus_number, device_number))
    {
        perror("open_spi_device_fd");
        return SPI_ERROR_NOT_CONNECTED;
    }

    if (data_size < 1)
    {
        perror("invalid data size");
        return SPI_ERROR_BAD_ARGUMENT;
    }

    // Allocate memory for the message
    spi_xchng_t *spi_xchng_msg = NULL;
    spi_xchng_msg = malloc(sizeof(spi_xchng_t) + data_size); // allocate enough memory for the data
    if (!spi_xchng_msg)
    {
        perror("alloc failed");
        return -1;
    }

    // Add the data
    spi_xchng_msg->nbytes = data_size;
    for (int i = 0; i < data_size; i++)
    {
        spi_xchng_msg->data[i] = data_buffer[i];
    }

    // Send the SPI message
    err = devctl(spi_device_fd[bus_number][device_number], DCMD_SPI_DATA_XCHNG, spi_xchng_msg, sizeof(spi_xchng_t) + data_size, NULL);
    if (err != EOK)
    {
        free(spi_xchng_msg);
        fprintf(stderr, "error: %d\n", err);
        perror("devctl");
        return SPI_ERROR_OPERATION_FAILED;
    }

    // Free allocated message
    free(spi_xchng_msg);

    return SPI_SUCCESS;
}

int rpi_spi_cleanup_device(unsigned bus_number, unsigned device_number)
{
    if (close_spi_device_fd(bus_number, device_number))
    {
        perror("close_spi_device_fd");
        return SPI_ERROR_NOT_CONNECTED;
    }

    return SPI_SUCCESS;
}

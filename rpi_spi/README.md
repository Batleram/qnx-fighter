# rpi_spi

This folder contains a client API for interacting with
the SPI driver.  The API was designed to cover the
the different commands documented here: 

https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.spi_framework/topic/spi_ch_overview.html#SPI_POSIX

Note that currently any app using this API needs to be
executed as root to access the SPI device driver.

## rpi_spi_get_driver_info

Query the SPI driver

## rpi_spi_get_device_info

Query the SPI device

## rpi_spi_configure_device

Configure the SPI device

## rpi_spi_write_read_data

Write/read data to/from the SPI interface

## rpi_spi_cleanup_device

Cleanup from using the SPI device

---
See [rpi_spi.h](rpi_spi.h) for more details.


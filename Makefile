default:
	ntoaarch64-gcc -I./rpi-gpio/resmgr/public/ -I./rpi_spi/public/  -I./rpi_ws281x/public/ ./screen_writer.c ./rpi_ws281x/rpi_ws281x.c ./rpi_spi/rpi_spi.c -o writer -lscreen -lm -fno-builtin-libm
	/bin/bash -c 'sshpass -p "qnxuser" scp ./writer qnxuser@192.168.41.238:~'


default:
	ntoaarch64-gcc -I./rpi-gpio/resmgr/public/ ./screen_writer.c -o writer -lscreen
	/bin/bash -c 'sshpass -p "qnxuser" scp ./writer qnxuser@192.168.41.238:~'


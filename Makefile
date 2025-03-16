default:
	ntoaarch64-gcc -std=c11 ./screen_writer.c -o writer -lscreen
	/bin/bash -c 'sshpass -p "qnxuser" scp ./writer qnxuser@192.168.41.238:~'


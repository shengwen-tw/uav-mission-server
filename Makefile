all:
	gcc -Wall -o uart-server main.c serial.c 

clean:
	rm -f uart-server

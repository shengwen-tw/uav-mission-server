CFLAGS=

CFLAGS+=-Wall

CFLAGS+=-I./lib/mavlink
CFLAGS+=-I./lib/mavlink/common

SRC=main.c \
	serial.c 

all: $(SRC)
	gcc $(CFLAGS) -o uart-server $^

clean:
	rm -f uart-server

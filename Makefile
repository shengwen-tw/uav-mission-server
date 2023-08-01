CFLAGS=

CFLAGS+=-Wall -Wno-address-of-packed-member

CFLAGS+=-I./lib/mavlink
CFLAGS+=-I./lib/mavlink/common
CFLAGS+=-I./

SRC=main.c \
	serial.c \
	mavlink_parser.c \
	mavlink_publisher.c

all: $(SRC)
	gcc $(CFLAGS) -o uart-server $^

clean:
	rm -f uart-server

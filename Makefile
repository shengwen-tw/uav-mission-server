CFLAGS :=
CFLAGS += -O2 -Wall \
	-Wno-address-of-packed-member \
	-Wno-unused-result \
	-Wno-array-bounds \
	-Wno-stringop-overread
CFLAGS += -I lib/mavlink
CFLAGS += -I lib/mavlink/common
CFLAGS += -I.
CFLAGS += -D MASTER_PID_FILE="\"uart-server.pid\""

TARGET = uart-server

SRCS := \
	main.c \
	serial.c \
	system.c \
	mavlink_parser.c \
	mavlink_publisher.c

all: $(TARGET)

$(TARGET): $(SRCS)
	./scripts/gen-mavlink.sh
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET)

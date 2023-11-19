UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
  # Linux (gcc)
  CFLAGS :=
  CFLAGS += -Wno-address-of-packed-member \
            -Wno-unused-result \
            -Wno-array-bounds \
            -Wno-stringop-overread
else ifeq ($(UNAME), Darwin)
  # macOS (clang)
  CFLAGS :=
else ifeq ($(UNAME), FreeBSD)
  # FreeBSD (clang)
  CFLAGS :=
endif

CFLAGS += -O2 -Wall

CFLAGS += -I lib/mavlink
CFLAGS += -I lib/mavlink/common
CFLAGS += -I.
CFLAGS += -D MASTER_PID_FILE="\"/tmp/uart-server.pid\""

TARGET = uart-server

SRCS := \
	main.c \
	serial.c \
	system.c \
	fcu.c \
	mavlink_parser.c \
	mavlink_publisher.c \
        siyi_camera.c

all: $(TARGET)

$(TARGET): $(SRCS)
	./scripts/gen-mavlink.sh
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) $(TARGET)

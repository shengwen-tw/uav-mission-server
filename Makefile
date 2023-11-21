UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
  # Linux (gcc)
  CFLAGS :=
  CFLAGS += -Wno-address-of-packed-member \
            -Wno-stringop-overflow \
            -Wno-unused-result \
            -Wno-array-bounds
  LDFLAGS :=
else ifeq ($(UNAME), Darwin)
  # macOS (clang)
  CFLAGS :=
  LDFLAGS :=
else ifeq ($(UNAME), FreeBSD)
  # FreeBSD (clang)
  CFLAGS :=
  LDFLAGS :=
endif

CFLAGS += -O2 -Wall

CFLAGS += -I lib/mavlink
CFLAGS += -I lib/mavlink/common
CFLAGS += -I.
CFLAGS += -D MASTER_PID_FILE="\"/tmp/uart-server.pid\""

# GStreamer
CFLAGS += $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0)

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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET)

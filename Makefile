UNAME := $(shell uname)

ASAN := #-fsanitize=address -static-libasan

ifeq ($(UNAME), Linux)
  # Linux (gcc)
  CFLAGS :=
  CFLAGS += -Wno-stringop-overread \
            -Wno-address-of-packed-member \
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

CFLAGS += -O2 -Wall $(ASAN)

CFLAGS += -I lib/mavlink
CFLAGS += -I lib/mavlink/common
CFLAGS += -I.
CFLAGS += -D MASTER_PID_FILE="\"/tmp/mission-server.pid\""

# GStreamer
CFLAGS += $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0)

# libyaml
LDFLAGS += -lyaml

TARGET = mission-server

SRCS := \
	src/main.c \
        src/uart_server.c \
	src/serial.c \
	src/system.c \
	src/fcu.c \
	src/mavlink_parser.c \
	src/mavlink_publisher.c \
        src/siyi_camera.c \
        src/rtsp_stream.c \
        src/config.c

all: $(TARGET)

$(TARGET): $(SRCS)
	./scripts/gen-mavlink.sh
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

FORMAT_EXCLUDE := #-path ./dir1 -o -path ./dir2 
FORMAT_FILES = ".*\.\(c\|h\)"

FORMAT_EXCLUDE = -path ./lib
FORMAT_FILES = ".*\.\(c\|h\)"

test:
	./mission-server -d siyi -c configs/siyi_a8_mini.yaml -s /dev/ttyUSB0 -b 57600

format:
	@echo "Execute clang-format"
	@find . -type d \( $(FORMAT_EXCLUDE) \) -prune -o \
                -regex $(FORMAT_FILES) -print \
                -exec clang-format -style=file -i {} \;

clean:
	$(RM) $(TARGET)

.PHONY: all test format clean

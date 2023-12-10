UNAME := $(shell uname)

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

CFLAGS += -O2 -Wall

CFLAGS += -I lib/mavlink
CFLAGS += -I lib/mavlink/common
CFLAGS += -I.
CFLAGS += -D MASTER_PID_FILE="\"/tmp/uart-server.pid\""

# GStreamer
CFLAGS += $(shell pkg-config --cflags gstreamer-1.0)
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0)

# libyaml
LDFLAGS += -lyaml

TARGET = uart-server

SRCS := \
	main.c \
	serial.c \
	system.c \
	fcu.c \
	mavlink_parser.c \
	mavlink_publisher.c \
        siyi_camera.c \
        rtsp_stream.c \
        config.c

all: $(TARGET)

$(TARGET): $(SRCS)
	./scripts/gen-mavlink.sh
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

FORMAT_EXCLUDE := #-path ./dir1 -o -path ./dir2 
FORMAT_FILES = ".*\.\(c\|h\)"

FORMAT_EXCLUDE = -path ./lib
FORMAT_FILES = ".*\.\(c\|h\)"

format:
	@echo "Execute clang-format"
	@find . -type d \( $(FORMAT_EXCLUDE) \) -prune -o \
                -regex $(FORMAT_FILES) -print \
                -exec clang-format -style=file -i {} \;

clean:
	$(RM) $(TARGET)

.PHONY: all format clean

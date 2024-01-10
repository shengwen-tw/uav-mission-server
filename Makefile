include mk/common.mk
OUT ?= build

ASAN := #-fsanitize=address -static-libasan

CFLAGS :=
LDFLAGS := -lpthread

ifeq ($(UNAME_S), Linux)
  # Linux (gcc)
  CFLAGS += -Wno-stringop-overread \
            -Wno-address-of-packed-member \
            -Wno-stringop-overflow \
            -Wno-unused-result \
            -Wno-array-bounds
else ifeq ($(UNAME), Darwin)
  # macOS (clang)
else ifeq ($(UNAME), FreeBSD)
  # FreeBSD (clang)
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

BIN := $(OUT)/mission-server

OBJS := \
	uart_server.o \
	serial.o \
	system.o \
	mavlink_receiver.o \
	mavlink_publisher.o \
	device.o \
	siyi_camera.o \
	rtsp_stream.o \
	config.o \
        crc16.o \
	main.o

OBJS := $(addprefix $(OUT)/, $(OBJS))
deps := $(OBJS:%.o=%.o.d)

all: $(BIN)

lib/mavlink/common/mavlink.h:
	scripts/gen-mavlink.sh

$(OUT)/%.o: src/%.c lib/mavlink/common/mavlink.h
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF $@.d $<

$(BIN): $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(LDFLAGS)

test: $(BIN)
	$(BIN) -s /dev/ttyUSB0 -b 57600

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
	$(RM) $(OBJS) $(BIN) $(deps)

distclean: clean
	-rm -rf lib/mavlink

.PHONY: all test format clean

-include $(deps)

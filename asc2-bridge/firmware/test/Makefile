CC=gcc
CCFLAGS=-std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function
BUILD_DIR=build

# The chip's info.yaml, from a clone of its repository beside this one (`../../../chip.mk`)
ROOT=../../..
include $(ROOT)/chip.mk

# The memory and the devices of the bridge against the emulator's. This
# needs no Pico and no SDK.
.PHONY: all
all: pins $(BUILD_DIR)/devices_test
	$(BUILD_DIR)/devices_test

# The pins of the demo board of Tiny Tapeout, against the chip's `info.yaml`
# and the netlists of the board
.PHONY: pins
pins:
	$(CHIP_CHECK)
	python3 -I pins_test.py $(CHIP_INFO)

$(BUILD_DIR)/devices_test: devices_test.c ../devices.h ../../../asc2-emulated/asc.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CCFLAGS) -o $@ devices_test.c

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

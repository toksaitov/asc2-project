CC=gcc
CCFLAGS=-g -std=c11
VERILATOR=verilator
ASA=../../asc2-assembler/asa
# The chip: a clone of its repository beside this one (`../../chip.mk`)
ROOT=../..
include $(ROOT)/chip.mk
REFTRACE_DIR=../../asc2-tools/reftrace
REFTRACE=$(REFTRACE_DIR)/asc_reftrace
BUILD_DIR=build
TRACE=$(BUILD_DIR)/obj/asc_trace

# The assembly tests: `EMU_TESTS` and `EMU_DOUBLE_FAULT_TESTS`
include ../asm/tests.mk
TESTS=$(EMU_TESTS) $(EMU_DOUBLE_FAULT_TESTS)

# Number of sectors of zeros that follow the bytes of a `.disk` file on the disk of its test
DISK_SECTORS=4

# Tests that also run with an RP2040 that takes 500 clocks to answer each
# change of REQ
SLOW_TESTS=cpu_data_move mmu_lookup cpu_irq_kbd

# Tests that also run with no answer to the first attempt at every fifth
# transaction. The chip waits, tries again, and gets the answer that the
# RP2040 has kept; each access happens once.
FAULT_TESTS=cpu_data_move mmu_lookup
FAULT_PERIOD=5

# The C programs and the kernel, which `../Makefile` builds. They run for
# millions of instructions. The kernel runs the sessions of `../kernel/`
# that start from the disk of the kernel's tests.
TESTS_DIR=..
RACH2_DIR=../../asc2-rach2
KERNEL=$(RACH2_DIR)/build/rach2.bin
KERNEL_DISK=$(RACH2_DIR)/build/tests/disk.img
KERNEL_SESSIONS=shell tests
PROGRAM_MAX_RETIRED=100000000

.PHONY: all
all: suite slow faults debug programs own-time

$(ASA): ../../asc2-assembler/asa.c ../../asc2-emulated/asc.h
	$(MAKE) -C ../../asc2-assembler all

$(REFTRACE): $(REFTRACE_DIR)/asc_reftrace.c $(REFTRACE_DIR)/trace_common.h ../../asc2-emulated/asc.h
	$(CC) $(CCFLAGS) -I$(abspath $(REFTRACE_DIR)) -o $@ $(REFTRACE_DIR)/asc_reftrace.c

$(BUILD_DIR)/%.bin: ../asm/%.s $(ASA)
	@mkdir -p $(BUILD_DIR)
	$(ASA) $< $@

$(BUILD_DIR)/%.img: ../asm/%.disk
	@mkdir -p $(BUILD_DIR)
	cp $< $@
	dd if=/dev/zero bs=4096 count=$(DISK_SECTORS) >> $@ 2> /dev/null

# The chip is built with the test input that steps the timer (see `asc_soc.sv`)
$(TRACE): asc_trace.cpp asc_trace_top.sv $(CHIP_SRCS) $(CHIP_INFO) $(REFTRACE_DIR)/trace_common.h ../../asc2-emulated/asc.h
	$(CHIP_CHECK)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) --cc --exe --build -j 4 -Wall +define+ASC_TEST_TICK --top-module asc_trace_top --prefix Vchip --Mdir $(BUILD_DIR)/obj \
	  -CFLAGS "-std=c++14 -O2 -I$(abspath $(REFTRACE_DIR))" asc_trace_top.sv $(CHIP_SRCS) asc_trace.cpp -o asc_trace \
	  > $(BUILD_DIR)/build.log 2>&1 || { tail -20 $(BUILD_DIR)/build.log; exit 1; }

.PHONY: build
build: $(TRACE) $(REFTRACE) \
       $(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(TESTS))) \
       $(patsubst ../asm/%.disk,$(BUILD_DIR)/%.img,$(wildcard ../asm/*.disk))

# Runs the tests of `TESTS` on the emulator and on the chip, the chip with
# the options of `CHIP_FLAGS`. Both must stop the same way and print the same
# trace, the same requests on the bus, the same console output, and the same
# last words.
define RUN_TESTS
	@mkdir -p $(BUILD_DIR)/emu $(BUILD_DIR)/$(1)
	@for test in $(2); do \
		args=""; \
		if [ -f ../asm/$$test.events ]; then args="$$args -e ../asm/$$test.events"; fi; \
		if [ -f ../asm/$$test.in ]; then args="$$args -i ../asm/$$test.in"; fi; \
		if [ -f ../asm/$$test.disk ]; then args="$$args -d $(BUILD_DIR)/$$test.img"; fi; \
		$(REFTRACE) -r $$args -o $(BUILD_DIR)/emu/$$test.out -b $(BUILD_DIR)/emu/$$test.bus $(BUILD_DIR)/$$test.bin \
		  > $(BUILD_DIR)/emu/$$test.trace 2> $(BUILD_DIR)/emu/$$test.log; \
		expected=$$?; \
		$(TRACE) -r $(3) $$args -o $(BUILD_DIR)/$(1)/$$test.out -b $(BUILD_DIR)/$(1)/$$test.bus $(BUILD_DIR)/$$test.bin \
		  > $(BUILD_DIR)/$(1)/$$test.trace 2> $(BUILD_DIR)/$(1)/$$test.log; \
		status=$$?; \
		if [ $$status -ne $$expected ]; then \
			tail -5 $(BUILD_DIR)/$(1)/$$test.log; \
			echo "FAILED: $$test (status $$status, the emulator ends with $$expected)"; \
			exit 1; \
		fi; \
		for kind in trace bus out; do \
			if ! cmp -s $(BUILD_DIR)/emu/$$test.$$kind $(BUILD_DIR)/$(1)/$$test.$$kind; then \
				echo "FAILED: $$test ($(BUILD_DIR)/$(1)/$$test.$$kind differs from $(BUILD_DIR)/emu/$$test.$$kind)"; \
				exit 1; \
			fi; \
		done; \
		if ! grep -v -e '^The RP2040 has sent' -e '^The debug port was' $(BUILD_DIR)/$(1)/$$test.log | cmp -s $(BUILD_DIR)/emu/$$test.log -; then \
			echo "FAILED: $$test ($(BUILD_DIR)/$(1)/$$test.log differs from $(BUILD_DIR)/emu/$$test.log)"; \
			exit 1; \
		fi; \
		$(4) \
		echo "passed: $$test$(5)"; \
	done
endef

.PHONY: suite
suite: build
	$(call RUN_TESTS,chip,$(TESTS),,,)

.PHONY: slow
slow: build
	$(call RUN_TESTS,slow,$(SLOW_TESTS),-w 500,, (a slow RP2040))

# A run that has repeated no transaction has tested nothing
.PHONY: faults
faults: build
	$(call RUN_TESTS,faults,$(FAULT_TESTS),-f $(FAULT_PERIOD),\
	  if grep -q '^The RP2040 has sent 0 answers' $(BUILD_DIR)/faults/$$test.log; then \
	    echo "FAILED: $$test (no transaction was repeated)"; exit 1; fi;, (answers held back))

# The debug port, read at its pins after every seventh instruction of every
# program of the suite and compared with the registers inside the chip, and
# after every 499th instruction of the kernel's session with its tests. A
# run that has read nothing has tested nothing. `cpu_dbg_lock` locks the
# port: from then on it must show nothing but that it is locked, and a run
# of that program that has read no locked port has tested nothing either.
#
# `cpu_icache` is left out: a read of the debug port empties the instruction
# cache, and that program checks which words the cache keeps.
DEBUG_PERIOD=7
DEBUG_KERNEL_PERIOD=499
DEBUG_TESTS=$(filter-out cpu_icache,$(TESTS))

.PHONY: debug
debug: build
	$(call RUN_TESTS,debug,$(DEBUG_TESTS),-g $(DEBUG_PERIOD),\
	  if grep -q '^The debug port was read and compared 0 times' $(BUILD_DIR)/debug/$$test.log; then \
	    echo "FAILED: $$test (the debug port was not read)"; exit 1; fi; \
	  if [ $$test = cpu_dbg_lock ] && ! grep -q '^The debug port was locked at' $(BUILD_DIR)/debug/$$test.log; then \
	    echo "FAILED: $$test (the locked debug port was not read)"; exit 1; fi;, (the debug port compared))
	@$(MAKE) -C $(TESTS_DIR) kernel-suite > $(BUILD_DIR)/programs.log 2>&1 || { tail -20 $(BUILD_DIR)/programs.log; exit 1; }
	@$(TRACE) -q -r -n $(PROGRAM_MAX_RETIRED) -g $(DEBUG_KERNEL_PERIOD) -d $(KERNEL_DISK) -i $(TESTS_DIR)/kernel/tests.in \
	  -o $(BUILD_DIR)/debug/kernel_tests.out $(KERNEL) 2> $(BUILD_DIR)/debug/kernel_tests.log \
	  || { tail -5 $(BUILD_DIR)/debug/kernel_tests.log; echo "FAILED: kernel_tests (the debug port compared)"; exit 1; }
	@cmp -s $(TESTS_DIR)/kernel/tests.expect $(BUILD_DIR)/debug/kernel_tests.out \
	  || { echo "FAILED: kernel_tests (the console output differs while the debug port is read)"; exit 1; }
	@echo "passed: kernel_tests (the debug port compared: `sed -n 's/^The debug port was read and compared \([0-9]*\) times/\1/p' $(BUILD_DIR)/debug/kernel_tests.log` times)"

# Runs the program `$$image` with the options `$$args` on the emulator and
# on the chip, step by step, under the name `$$name`. The lines of CPU state
# of a long program are not kept: the two runs must print lines with the same
# checksum. The requests on the bus are compared and then removed, because
# they take up to a gigabyte. A run with a disk must leave the same disk on
# both sides.
define RUN_IN_STEP
	for side in emu chip; do \
		if [ $$side = emu ]; then tool=$(REFTRACE); else tool=$(TRACE); fi; \
		rm -f $(BUILD_DIR)/$$side/$$name.img; \
		{ $$tool -r -n $(PROGRAM_MAX_RETIRED) $$args -s $(BUILD_DIR)/$$side/$$name.img \
		    -o $(BUILD_DIR)/$$side/$$name.out -b $(BUILD_DIR)/$$side/$$name.bus $$image \
		    2> $(BUILD_DIR)/$$side/$$name.log; echo $$? > $(BUILD_DIR)/$$side/$$name.status; } \
		  | cksum > $(BUILD_DIR)/$$side/$$name.sum; \
	done; \
	kinds="status sum bus out log"; \
	if [ -f $(BUILD_DIR)/emu/$$name.img ]; then kinds="$$kinds img"; fi; \
	for kind in $$kinds; do \
		if ! cmp -s $(BUILD_DIR)/emu/$$name.$$kind $(BUILD_DIR)/chip/$$name.$$kind; then \
			echo "FAILED: $$name ($(BUILD_DIR)/chip/$$name.$$kind differs from $(BUILD_DIR)/emu/$$name.$$kind)"; \
			exit 1; \
		fi; \
	done; \
	if [ `cat $(BUILD_DIR)/chip/$$name.status` -ne 0 ]; then \
		cat $(BUILD_DIR)/chip/$$name.log; \
		echo "FAILED: $$name (status `cat $(BUILD_DIR)/chip/$$name.status`)"; \
		exit 1; \
	fi; \
	rm -f $(BUILD_DIR)/emu/$$name.bus $(BUILD_DIR)/chip/$$name.bus; \
	echo "passed: $$name"
endef

.PHONY: programs
programs: build
	@$(MAKE) -C $(TESTS_DIR) long-suite c-suite kernel-suite > $(BUILD_DIR)/programs.log 2>&1 || { tail -20 $(BUILD_DIR)/programs.log; exit 1; }
	@mkdir -p $(BUILD_DIR)/emu $(BUILD_DIR)/chip
	@for image in $(TESTS_DIR)/build/long/*.bin $(TESTS_DIR)/build/c/*.bin; do \
		name=`basename $$image .bin`; \
		args=""; \
		$(RUN_IN_STEP); \
	done
	@for session in $(KERNEL_SESSIONS); do \
		name=kernel_$$session; \
		image=$(KERNEL); \
		args="-d $(KERNEL_DISK) -i $(TESTS_DIR)/kernel/$$session.in"; \
		$(RUN_IN_STEP); \
	done

# The chip on its own time, as on a board: no STOP between the
# instructions, and a timer that steps once in 256 clocks. Interrupts come at
# other moments than in the emulator, so a program must end the same way and
# print the same on the console, and nothing else is compared.
define RUN_ON_OWN_TIME
	$(REFTRACE) -q -r -n $(PROGRAM_MAX_RETIRED) $$args -o $(BUILD_DIR)/emu/$$name.out $$image 2> $(BUILD_DIR)/emu/$$name.log; \
	expected=$$?; \
	$(TRACE) -t -r -n $(PROGRAM_MAX_RETIRED) $$args -o $(BUILD_DIR)/own-time/$$name.out $$image 2> $(BUILD_DIR)/own-time/$$name.log; \
	status=$$?; \
	if [ $$status -ne $$expected ]; then \
		tail -5 $(BUILD_DIR)/own-time/$$name.log; \
		echo "FAILED: $$name (status $$status, the emulator ends with $$expected)"; \
		exit 1; \
	fi; \
	for kind in out log; do \
		if ! cmp -s $(BUILD_DIR)/emu/$$name.$$kind $(BUILD_DIR)/own-time/$$name.$$kind; then \
			echo "FAILED: $$name ($(BUILD_DIR)/own-time/$$name.$$kind differs from $(BUILD_DIR)/emu/$$name.$$kind)"; \
			exit 1; \
		fi; \
	done; \
	echo "passed: $$name (on the chip's own time)"
endef

.PHONY: own-time
own-time: build
	@$(MAKE) -C $(TESTS_DIR) long-suite c-suite kernel-suite > $(BUILD_DIR)/programs.log 2>&1 || { tail -20 $(BUILD_DIR)/programs.log; exit 1; }
	@mkdir -p $(BUILD_DIR)/emu $(BUILD_DIR)/own-time
	@for image in $(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(TESTS))) $(TESTS_DIR)/build/long/*.bin $(TESTS_DIR)/build/c/*.bin; do \
		name=`basename $$image .bin`; \
		args=""; \
		if [ -f ../asm/$$name.events ]; then args="$$args -e ../asm/$$name.events"; fi; \
		if [ -f ../asm/$$name.in ]; then args="$$args -i ../asm/$$name.in"; fi; \
		if [ -f ../asm/$$name.disk ]; then args="$$args -d $(BUILD_DIR)/$$name.img"; fi; \
		$(RUN_ON_OWN_TIME); \
	done
	@for session in $(KERNEL_SESSIONS); do \
		name=kernel_$$session; \
		image=$(KERNEL); \
		args="-d $(KERNEL_DISK) -i $(TESTS_DIR)/kernel/$$session.in"; \
		$(RUN_ON_OWN_TIME); \
	done

# Prints the clocks that the jobs of `../gain/gain.py` take on the chip, for
# an RP2040 that answers at once and for one that answers as the bridge of
# the bench does (`../gain/gain_time.py`). It is not a test, but every value
# that a job stores is compared with a model in Python.
.PHONY: gain
gain: $(TRACE) $(REFTRACE) $(ASA)
	python3 -I ../gain/gain_time.py clocks $(ASA) $(REFTRACE) $(TRACE) $(BUILD_DIR)/gain

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

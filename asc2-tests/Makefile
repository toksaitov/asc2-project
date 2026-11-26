CC=gcc
CCFLAGS=-g -std=c11
ASA=../asc2-assembler/asa
# `-r` fills RAM with other bytes than zeros before a program runs: the RAM of
# the real machine keeps its bytes over a reset
REFTRACE=../asc2-tools/reftrace/asc_reftrace
REFTRACE_FLAGS=-q -r
REFTRACE_INC=$(abspath ../asc2-tools/reftrace)
EMU_BUILD_DIR=build/emu

TCC_DIR=../asc2-tcc
TCC=$(TCC_DIR)/build/tcc
LIBTCC1=$(TCC_DIR)/build/libtcc1.a
ELF2FW=../asc2-tools/elf2fw/asc_elf2fw
C_BUILD_DIR=build/c

RACH2_DIR=../asc2-rach2
KERNEL=$(RACH2_DIR)/build/rach2.bin
KRFFS=../asc2-tools/krffs/asc_krffs
KERNEL_BUILD_DIR=build/kernel

# The disk of the kernel's tests: the shell, the utilities, and the programs that check the kernel
KERNEL_DISK=$(RACH2_DIR)/build/tests/disk.img

# The disk of the machine itself: the shell, the utilities, the programs of the showcase, and the welcome
SHOWCASE_DISK=$(RACH2_DIR)/build/disk.img

# The script that runs a program on the board through the Pico
BRIDGE=python3 -I ../asc2-bridge/host/bridge.py
BOARD_BUILD_DIR=build/board

# The longest time of a session of the kernel on the board, in seconds. The
# board runs about 60,000 instructions a second.
BOARD_KERNEL_SECONDS=1500

# `kernel/screen` on the board: the seconds after the start at which the
# bridge presses the button. The first press starts a flight of `burkut`,
# whose first picture waits for it by then; the second, four seconds long,
# ends the game.
BOARD_SCREEN_PRESSES=45 60:4000

# The C program that the board also runs one instruction at a time, and the
# number of its first instructions that are compared with the emulator's
BOARD_STEPS_TEST=types
BOARD_STEPS=40

# The bytes that the Pico sends as a keyboard: a press and a release of the
# key A. `board/keys.events` has the same bytes for the emulator.
BOARD_KEYS=1c f0 1c
BOARD_PRESSES=0.5 1.2

# The assembly tests: `EMU_TESTS` and `EMU_DOUBLE_FAULT_TESTS`
include asm/tests.mk

# Number of sectors of zeros that follow the bytes of a `.disk` file on the disk of its test
DISK_SECTORS=4

# C programs. Each one prints its results, and the output must equal the
# output of the same C file built for the computer that runs the tests.
C_TESTS=calls types varargs llong ops structs mem fixed

# The flags of the build for this computer. The optimizer is off: a mistake
# in a test that C leaves undefined must not make the two outputs differ.
HOST_CCFLAGS=-std=c11 -O0 -fwrapv -fno-strict-aliasing -Wall -Wextra

# The addresses of a firmware image: the text in the boot ROM, the data in RAM
C_LDFLAGS=-nostdlib -Wl,-Ttext=0x80800000 -Wl,-Tdata=0x80001000

# A C program runs many more instructions than an assembly test
C_MAX_RETIRED=50000000

# The longest session of the kernel runs about 25 million instructions
KERNEL_MAX_RETIRED=100000000

# The long assembly programs of `long/`: each one prints lines on the
# console, which must be those of its `.expect` file
LONG_TESTS=dot4_lanes dot4_random dot4_irq array_random
LONG_BUILD_DIR=build/long
LONG_MAX_RETIRED=20000000

.PHONY: all
all: emu-suite long-suite c-suite kernel-suite

$(ASA): ../asc2-assembler/asa.c ../asc2-emulated/asc.h
	$(MAKE) -C ../asc2-assembler all

$(REFTRACE): ../asc2-tools/reftrace/asc_reftrace.c ../asc2-tools/reftrace/trace_common.h ../asc2-emulated/asc.h
	$(CC) $(CCFLAGS) -I$(REFTRACE_INC) -o $@ ../asc2-tools/reftrace/asc_reftrace.c

$(EMU_BUILD_DIR)/%.bin: asm/%.s $(ASA)
	mkdir -p $(EMU_BUILD_DIR)
	$(ASA) $< $@

$(EMU_BUILD_DIR)/%.img: asm/%.disk
	mkdir -p $(EMU_BUILD_DIR)
	cp $< $@
	dd if=/dev/zero bs=4096 count=$(DISK_SECTORS) >> $@ 2> /dev/null

# A test may come with files of the same name: `.events` with the bytes of
# the keyboard, `.in` with the input of the console, `.expect` with the output
# that the console must show, `.disk` with the first bytes of the disk, `.bus`
# with the requests that the CPU must send on the bus, `.frame` with what the
# screen must show at the end, and `.stop` with the line that the runner must
# print when the CPU stops. An `.events` file may also press the button.
.PHONY: emu-suite
emu-suite: $(REFTRACE) \
           $(addprefix $(EMU_BUILD_DIR)/,$(addsuffix .bin,$(EMU_TESTS) $(EMU_DOUBLE_FAULT_TESTS))) \
           $(patsubst asm/%.disk,$(EMU_BUILD_DIR)/%.img,$(wildcard asm/*.disk))
	@for test in $(EMU_TESTS) $(EMU_DOUBLE_FAULT_TESTS); do \
		args=""; \
		if [ -f asm/$$test.events ]; then args="$$args -e asm/$$test.events"; fi; \
		if [ -f asm/$$test.in ]; then args="$$args -i asm/$$test.in"; fi; \
		if [ -f asm/$$test.expect ]; then args="$$args -o $(EMU_BUILD_DIR)/$$test.out"; fi; \
		if [ -f asm/$$test.disk ]; then args="$$args -d $(EMU_BUILD_DIR)/$$test.img"; fi; \
		if [ -f asm/$$test.bus ]; then args="$$args -b $(EMU_BUILD_DIR)/$$test.bus"; fi; \
		if [ -f asm/$$test.frame ]; then args="$$args -f $(EMU_BUILD_DIR)/$$test.frame"; fi; \
		expected=0; \
		case " $(EMU_DOUBLE_FAULT_TESTS) " in *" $$test "*) expected=3;; esac; \
		$(REFTRACE) $(REFTRACE_FLAGS) $$args $(EMU_BUILD_DIR)/$$test.bin 2> $(EMU_BUILD_DIR)/$$test.log; \
		status=$$?; \
		if [ $$status -ne $$expected ]; then \
			cat $(EMU_BUILD_DIR)/$$test.log; \
			echo "FAILED: $$test (status $$status, expected $$expected)"; \
			exit 1; \
		fi; \
		if [ -f asm/$$test.expect ] && ! cmp -s asm/$$test.expect $(EMU_BUILD_DIR)/$$test.out; then \
			echo "FAILED: $$test (the console output differs from asm/$$test.expect)"; \
			exit 1; \
		fi; \
		if [ -f asm/$$test.bus ] && ! cmp -s asm/$$test.bus $(EMU_BUILD_DIR)/$$test.bus; then \
			echo "FAILED: $$test (the bus requests differ from asm/$$test.bus)"; \
			exit 1; \
		fi; \
		if [ -f asm/$$test.frame ] && ! cmp -s asm/$$test.frame $(EMU_BUILD_DIR)/$$test.frame; then \
			echo "FAILED: $$test (the screen differs from asm/$$test.frame)"; \
			exit 1; \
		fi; \
		if [ -f asm/$$test.stop ] && ! cmp -s asm/$$test.stop $(EMU_BUILD_DIR)/$$test.log; then \
			echo "FAILED: $$test (the stop differs from asm/$$test.stop)"; \
			exit 1; \
		fi; \
		echo "passed: $$test"; \
	done

# The long programs. `dot4_expect.py` computes the sums of two of them once
# more, without `dot4` and without `mul`, and `array_expect.py` the total of
# `array_random` by the rules of the instructions of the array.
.PHONY: long-suite
long-suite: $(REFTRACE) $(ASA)
	@mkdir -p $(LONG_BUILD_DIR)
	@for name in random irq; do \
		if ! python3 -I long/dot4_expect.py $$name | cmp -s - long/dot4_$$name.expect; then \
			echo "FAILED: long/dot4_$$name (long/dot4_$$name.expect differs from the output of long/dot4_expect.py)"; \
			exit 1; \
		fi; \
	done
	@if ! python3 -I long/array_expect.py | cmp -s - long/array_random.expect; then \
		echo "FAILED: long/array_random (long/array_random.expect differs from the output of long/array_expect.py)"; \
		exit 1; \
	fi
	@for test in $(LONG_TESTS); do \
		$(ASA) long/$$test.s $(LONG_BUILD_DIR)/$$test.bin || exit 1; \
		$(REFTRACE) $(REFTRACE_FLAGS) -n $(LONG_MAX_RETIRED) -o $(LONG_BUILD_DIR)/$$test.out $(LONG_BUILD_DIR)/$$test.bin \
		  2> $(LONG_BUILD_DIR)/$$test.log; \
		status=$$?; \
		if [ $$status -ne 0 ]; then \
			cat $(LONG_BUILD_DIR)/$$test.log; \
			echo "FAILED: long/$$test (status $$status)"; \
			exit 1; \
		fi; \
		if ! cmp -s long/$$test.expect $(LONG_BUILD_DIR)/$$test.out; then \
			echo "FAILED: long/$$test (the console output differs from long/$$test.expect)"; \
			exit 1; \
		fi; \
		echo "passed: long/$$test"; \
	done

.PHONY: tcc
tcc:
	$(MAKE) -C $(TCC_DIR) all

.PHONY: elf2fw
elf2fw:
	$(MAKE) -C ../asc2-tools/elf2fw all

# A C test is `c/name.c`. If a part of it is written in assembly, the part is
# `c/name_asm.s`, and `c/name_host.c` has the same functions in C for the
# build on this computer.
.PHONY: c-suite
c-suite: tcc elf2fw $(ASA) $(REFTRACE)
	@mkdir -p $(C_BUILD_DIR)
	@$(ASA) -c c/start.s $(C_BUILD_DIR)/start.o
	@$(ASA) -c c/console.s $(C_BUILD_DIR)/console.o
	@$(TCC) -c -o $(C_BUILD_DIR)/out.o c/out.c
	@for test in $(C_TESTS); do \
		objs="$(C_BUILD_DIR)/$$test.o"; \
		host_srcs="c/$$test.c"; \
		$(TCC) -c -o $(C_BUILD_DIR)/$$test.o c/$$test.c || exit 1; \
		if [ -f c/$${test}_asm.s ]; then \
			$(ASA) -c c/$${test}_asm.s $(C_BUILD_DIR)/$${test}_asm.o || exit 1; \
			objs="$$objs $(C_BUILD_DIR)/$${test}_asm.o"; \
			host_srcs="$$host_srcs c/$${test}_host.c"; \
		fi; \
		$(TCC) $(C_LDFLAGS) -o $(C_BUILD_DIR)/$$test.elf \
			$(C_BUILD_DIR)/start.o $$objs $(C_BUILD_DIR)/out.o $(C_BUILD_DIR)/console.o $(LIBTCC1) || exit 1; \
		$(ELF2FW) $(C_BUILD_DIR)/$$test.elf $(C_BUILD_DIR)/$$test.bin || exit 1; \
		$(CC) $(HOST_CCFLAGS) -o $(C_BUILD_DIR)/$$test.host $$host_srcs c/out.c c/host.c || exit 1; \
		$(C_BUILD_DIR)/$$test.host > $(C_BUILD_DIR)/$$test.expect || exit 1; \
		$(REFTRACE) $(REFTRACE_FLAGS) -n $(C_MAX_RETIRED) -o $(C_BUILD_DIR)/$$test.out $(C_BUILD_DIR)/$$test.bin 2> $(C_BUILD_DIR)/$$test.log; \
		status=$$?; \
		if [ $$status -ne 0 ]; then \
			cat $(C_BUILD_DIR)/$$test.log; \
			echo "FAILED: c/$$test (status $$status)"; \
			exit 1; \
		fi; \
		if ! cmp -s $(C_BUILD_DIR)/$$test.expect $(C_BUILD_DIR)/$$test.out; then \
			echo "FAILED: c/$$test (the output differs from the output of the build on this computer)"; \
			echo "Compare $(C_BUILD_DIR)/$$test.expect and $(C_BUILD_DIR)/$$test.out"; \
			exit 1; \
		fi; \
		echo "passed: c/$$test"; \
	done

# Boots the kernel with the disk $(2) and types the lines of
# `kernel/$(1).in`. The run must end with the status $(3), and it leaves
# what the console has shown in `$(1).out` and its disk in `$(1).img`. The
# file system of that disk must be sound.
define RUN_SESSION
	$(REFTRACE) $(REFTRACE_FLAGS) -n $(KERNEL_MAX_RETIRED) -d $(2) -s $(KERNEL_BUILD_DIR)/$(1).img \
	  -i kernel/$(1).in -o $(KERNEL_BUILD_DIR)/$(1).out $(KERNEL) 2> $(KERNEL_BUILD_DIR)/$(1).log; \
	status=$$?; \
	if [ $$status -ne $(3) ]; then \
		cat $(KERNEL_BUILD_DIR)/$(1).log; \
		tail -5 $(KERNEL_BUILD_DIR)/$(1).out; \
		echo "FAILED: kernel/$(1) (status $$status, expected $(3))"; \
		exit 1; \
	fi; \
	if ! $(KRFFS) check $(KERNEL_BUILD_DIR)/$(1).img > $(KERNEL_BUILD_DIR)/$(1).check 2>&1; then \
		tail -5 $(KERNEL_BUILD_DIR)/$(1).check; \
		echo "FAILED: kernel/$(1) (the file system of $(KERNEL_BUILD_DIR)/$(1).img is broken)"; \
		exit 1; \
	fi
endef

# Compares the file $(2) with the file $(3); $(4) says what they are.
define COMPARE
	if ! cmp -s $(2) $(3); then \
		echo "FAILED: kernel/$(1) ($(4))"; \
		echo "Compare $(2) and $(3)"; \
		exit 1; \
	fi
endef

# The kernel boots three times, each time with typed lines, and the console
# must show the lines of an `.expect` file.
# * `shell`: the shell and the utilities. The copy of `ish` that the session
#   makes on the disk must have the bytes of `ish`.
# * `again`: a second boot from the disk that `shell` has left. `ls` must
#   print what `asc_krffs` lists on that disk; the lines of a listing start
#   with a space, and the other lines are in `kernel/again.expect`. The shell
#   ends with the exit code of its last command, which is 3, so the run fails.
# * `tests`: the programs that check the kernel. They must leave the disk
#   with the files that it had.
# * `edit`: the editor `ed` makes a file, changes it, writes it, and reads it
#   again; `cat` prints what is on the disk.
.PHONY: kernel-suite
kernel-suite: $(REFTRACE)
	@$(MAKE) -C $(RACH2_DIR) all
	@mkdir -p $(KERNEL_BUILD_DIR)
	@$(call RUN_SESSION,shell,$(KERNEL_DISK),0); \
	$(call COMPARE,shell,kernel/shell.expect,$(KERNEL_BUILD_DIR)/shell.out,the console output differs); \
	$(KRFFS) get $(KERNEL_BUILD_DIR)/shell.img copy $(KERNEL_BUILD_DIR)/shell.copy || exit 1; \
	$(call COMPARE,shell,$(RACH2_DIR)/build/bin/ish,$(KERNEL_BUILD_DIR)/shell.copy,the copy of a file differs from the file); \
	echo "passed: kernel/shell"
	@$(call RUN_SESSION,again,$(KERNEL_BUILD_DIR)/shell.img,1); \
	$(KRFFS) list $(KERNEL_BUILD_DIR)/shell.img > $(KERNEL_BUILD_DIR)/again.list || exit 1; \
	grep '^ ' $(KERNEL_BUILD_DIR)/again.out > $(KERNEL_BUILD_DIR)/again.ls; \
	grep -v '^ ' $(KERNEL_BUILD_DIR)/again.out > $(KERNEL_BUILD_DIR)/again.rest; \
	$(call COMPARE,again,$(KERNEL_BUILD_DIR)/again.list,$(KERNEL_BUILD_DIR)/again.ls,ls does not print the files of the disk); \
	$(call COMPARE,again,kernel/again.expect,$(KERNEL_BUILD_DIR)/again.rest,the console output differs); \
	echo "passed: kernel/again"
	@$(call RUN_SESSION,tests,$(KERNEL_DISK),0); \
	$(call COMPARE,tests,kernel/tests.expect,$(KERNEL_BUILD_DIR)/tests.out,the console output differs); \
	$(KRFFS) list $(KERNEL_DISK) > $(KERNEL_BUILD_DIR)/tests.before || exit 1; \
	$(KRFFS) list $(KERNEL_BUILD_DIR)/tests.img > $(KERNEL_BUILD_DIR)/tests.after || exit 1; \
	$(call COMPARE,tests,$(KERNEL_BUILD_DIR)/tests.before,$(KERNEL_BUILD_DIR)/tests.after,the tests have left other files on the disk); \
	echo "passed: kernel/tests"
	@$(call RUN_SESSION,edit,$(KERNEL_DISK),0); \
	$(call COMPARE,edit,kernel/edit.expect,$(KERNEL_BUILD_DIR)/edit.out,the console output differs); \
	echo "passed: kernel/edit"
	@$(REFTRACE) $(REFTRACE_FLAGS) -n $(KERNEL_MAX_RETIRED) -d $(KERNEL_DISK) -e kernel/screen.events -i kernel/screen.in \
	  -o $(KERNEL_BUILD_DIR)/screen.out -f $(KERNEL_BUILD_DIR)/screen.frame -m $(KERNEL_BUILD_DIR)/screen.film $(KERNEL) \
	  2> $(KERNEL_BUILD_DIR)/screen.log || { cat $(KERNEL_BUILD_DIR)/screen.log; echo "FAILED: kernel/screen"; exit 1; }; \
	$(call COMPARE,screen,kernel/screen.expect,$(KERNEL_BUILD_DIR)/screen.out,the console output differs); \
	$(call COMPARE,screen,kernel/screen.frame,$(KERNEL_BUILD_DIR)/screen.frame,the screen does not show the console at the end); \
	python3 -I kernel/has_frame.py $(KERNEL_BUILD_DIR)/screen.film ../asc2-demos/scenes.frame || \
	  { echo "FAILED: kernel/screen (the scenes have drawn other pixels than on the bare machine)"; exit 1; }; \
	echo "passed: kernel/screen (the scenes and the game run from the shell, and the console has the screen again)"
	@$(REFTRACE) $(REFTRACE_FLAGS) -n $(KERNEL_MAX_RETIRED) -d $(SHOWCASE_DISK) -e kernel/showcase.events -i kernel/showcase.in \
	  -o $(KERNEL_BUILD_DIR)/showcase.out -f $(KERNEL_BUILD_DIR)/showcase.frame $(KERNEL) \
	  2> $(KERNEL_BUILD_DIR)/showcase.log || { cat $(KERNEL_BUILD_DIR)/showcase.log; echo "FAILED: kernel/showcase"; exit 1; }; \
	$(call COMPARE,showcase,kernel/showcase.expect,$(KERNEL_BUILD_DIR)/showcase.out,the console output differs); \
	$(call COMPARE,showcase,kernel/showcase.frame,$(KERNEL_BUILD_DIR)/showcase.frame,the screen does not show the console at the end); \
	echo "passed: kernel/showcase (the programs of the showcase run from the shell and end with a hold of the button)"
	@$(REFTRACE) $(REFTRACE_FLAGS) -n $(KERNEL_MAX_RETIRED) -d $(SHOWCASE_DISK) -i kernel/tour.in \
	  -o $(KERNEL_BUILD_DIR)/tour.out -f $(KERNEL_BUILD_DIR)/tour.frame $(KERNEL) \
	  2> $(KERNEL_BUILD_DIR)/tour.log || { cat $(KERNEL_BUILD_DIR)/tour.log; echo "FAILED: kernel/tour"; exit 1; }; \
	$(call COMPARE,tour,kernel/tour.expect,$(KERNEL_BUILD_DIR)/tour.out,the console output differs); \
	$(call COMPARE,tour,kernel/tour.frame,$(KERNEL_BUILD_DIR)/tour.frame,the screen does not show the console at the end); \
	echo "passed: kernel/tour (the tour starts every program of the showcase, and each one shows itself and ends with nobody at the button)"

# `kernel/screen` runs the two programs of the disk that take the screen:
# `scenes`, whose last picture must be the one of the bare machine
# (`../asc2-demos/scenes.frame`), and the game `burkut` with the presses of
# `kernel/screen.events`. At the end the screen must show the console again.
#
# `kernel/showcase` runs the other programs that take the screen, from the
# disk of the machine itself (the disk of the tests has no room for them), one
# after another, with the presses of `kernel/showcase.events`: each one is taken
# through its pictures and ended with a hold of the button. The lines that
# they print must be the same, the speeds among them: the clock of the
# emulator counts instructions. The session runs on the emulator only: the
# speeds of the board are other numbers. What a program prints depends on
# when a press reaches it, and so on how long the programs before it took
# to load: after a change of the disk or of the kernel the lines differ.
# Look at the difference then, and if it is what the programs should say,
# copy `build/kernel/showcase.out` and `showcase.frame` over the two files.
# The benchmark `mark` ends the session; it needs no press, and its points
# are the ones of the emulator, far from the 1000 of the board.
#
# `kernel/tour` runs the program `tour` with no press at all: it starts the
# programs of the showcase one after another, and each one goes through its
# pictures and ends by itself. Its lines change for the same reasons, and
# the files to copy then are `build/kernel/tour.out` and `tour.frame`.

# The scheduler of the kernel, measured (`sys/proc/slice.c` there). The
# program `load` runs under three kernels: one that gives every process a
# long turn, one that gives a short turn, and one whose network chooses. Each
# kernel prints its counts at the end. The emulator is built with a disk
# that takes 2500 instructions for a command, about a third of a period of
# the timer, as the flash of the Pico does on the board; with the usual 16
# instructions no process would ever wait for the disk. `FIRST_WEIGHTS=0`
# lets the network start from zeros: `make measure FIRST_WEIGHTS=0` is the
# run that the weights of the kernel come from.
MEASURE_BUILD_DIR=build/measure
MEASURE_DISK_STEPS=2500
FIRST_WEIGHTS=

.PHONY: measure
measure:
	@mkdir -p $(MEASURE_BUILD_DIR)
	@$(CC) $(CCFLAGS) -O2 -DRP_DISK_STEPS=$(MEASURE_DISK_STEPS) -I$(REFTRACE_INC) -o $(MEASURE_BUILD_DIR)/asc_reftrace ../asc2-tools/reftrace/asc_reftrace.c
	@for policy in 0 1 2; do \
		dir=$(MEASURE_BUILD_DIR)/policy$$policy; \
		rm -rf $$dir; \
		$(MAKE) -C $(RACH2_DIR) all BUILD_DIR=$(abspath $(MEASURE_BUILD_DIR))/policy$$policy \
		  KERNEL_DEFS="-DSLICE_STATS -DSLICE_POLICY=$$policy $(if $(FIRST_WEIGHTS),-DFIRST_WEIGHTS=$(FIRST_WEIGHTS))" > $$dir.build 2>&1 || { tail -5 $$dir.build; exit 1; }; \
		$(MEASURE_BUILD_DIR)/asc_reftrace $(REFTRACE_FLAGS) -n $(KERNEL_MAX_RETIRED) -d $$dir/tests/disk.img -s $$dir.img \
		  -i kernel/load.in -o $$dir.out $$dir/rach2.bin 2> $$dir.log || { cat $$dir.log; tail -5 $$dir.out; exit 1; }; \
		case $$policy in 0) echo "Every turn is long:";; 1) echo "Every turn is short:";; 2) echo "The network chooses:";; esac; \
		grep "^Turns\|^Right\|^Long turns\|^Waits\|^Weights" $$dir.out; \
	done

# Reads the disk of the Pico into `$(1).img` and checks its file system. The
# disk must have the files of the disk $(2), which the same session has left
# on the emulator.
define CHECK_BOARD_DISK
	$(BRIDGE) disk get $(BOARD_BUILD_DIR)/$(1).img || exit 1; \
	if ! $(KRFFS) check $(BOARD_BUILD_DIR)/$(1).img > $(BOARD_BUILD_DIR)/$(1).check 2>&1; then \
		tail -5 $(BOARD_BUILD_DIR)/$(1).check; \
		echo "FAILED: kernel/$(1) on the board (the file system of $(BOARD_BUILD_DIR)/$(1).img is broken)"; \
		exit 1; \
	fi; \
	$(KRFFS) list $(BOARD_BUILD_DIR)/$(1).img > $(BOARD_BUILD_DIR)/$(1).list || exit 1; \
	$(KRFFS) list $(2) > $(BOARD_BUILD_DIR)/$(1).emu.list || exit 1; \
	$(call COMPARE,$(1) on the board,$(BOARD_BUILD_DIR)/$(1).emu.list,$(BOARD_BUILD_DIR)/$(1).list,the disk has other files than on the emulator)
endef

# The C programs and the kernel on the board: the chip in the DE10-Lite and
# the Pico with the firmware of `../asc2-bridge/`. Each program must print
# on the board what it prints on the emulator. Two programs come first: the
# smoke program, and `board/disk.s`, which writes a sector of the Pico's
# flash and reads it back. The kernel runs its three sessions with the disk
# in the flash of the Pico, and the disk is read back and checked. One C
# program also runs its first instructions one at a time, each compared with
# the emulator's, and is stopped and let go in the middle of its run. This
# target is not a part of `make`: it needs the two boards on the table, and
# `picotool` for the disk.
.PHONY: board-suite
board-suite: long-suite c-suite kernel-suite board-keys board-lock board-regs board-button board-screen
	@mkdir -p $(BOARD_BUILD_DIR)
	@$(ASA) ../asc2-firmware/hello.s $(BOARD_BUILD_DIR)/hello.bin
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/hello.bin board/hello.expect
	@$(ASA) board/disk.s $(BOARD_BUILD_DIR)/disk.bin
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/disk.bin board/disk.expect
	@$(ASA) board/dot4.s $(BOARD_BUILD_DIR)/dot4.bin
	@if ! $(REFTRACE) $(REFTRACE_FLAGS) -o $(BOARD_BUILD_DIR)/dot4.out $(BOARD_BUILD_DIR)/dot4.bin \
	      2> $(BOARD_BUILD_DIR)/dot4.log || ! cmp -s board/dot4.expect $(BOARD_BUILD_DIR)/dot4.out; then \
		cat $(BOARD_BUILD_DIR)/dot4.log; \
		echo "FAILED: board/dot4 on the emulator"; \
		exit 1; \
	fi
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/dot4.bin board/dot4.expect
	@$(ASA) board/high.s $(BOARD_BUILD_DIR)/high.bin
	@if ! $(REFTRACE) $(REFTRACE_FLAGS) -o $(BOARD_BUILD_DIR)/high.out $(BOARD_BUILD_DIR)/high.bin \
	      2> $(BOARD_BUILD_DIR)/high.log || ! cmp -s board/high.expect $(BOARD_BUILD_DIR)/high.out; then \
		cat $(BOARD_BUILD_DIR)/high.log; \
		echo "FAILED: board/high on the emulator"; \
		exit 1; \
	fi
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/high.bin board/high.expect
	@for test in $(LONG_TESTS); do \
		$(BRIDGE) check $(LONG_BUILD_DIR)/$$test.bin long/$$test.expect || exit 1; \
	done
	@for test in $(C_TESTS); do \
		$(BRIDGE) check $(C_BUILD_DIR)/$$test.bin $(C_BUILD_DIR)/$$test.expect || exit 1; \
	done
	@$(REFTRACE) -n $(BOARD_STEPS) $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).bin > $(BOARD_BUILD_DIR)/steps.trace 2> /dev/null; \
	$(BRIDGE) check $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).bin $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).expect \
	  --steps $(BOARD_BUILD_DIR)/steps.trace
	@$(BRIDGE) disk put $(KERNEL_DISK)
	@$(BRIDGE) check $(KERNEL) kernel/shell.expect --input kernel/shell.in --for $(BOARD_KERNEL_SECONDS)
	@$(BRIDGE) check $(KERNEL) $(KERNEL_BUILD_DIR)/again.out --input kernel/again.in --for $(BOARD_KERNEL_SECONDS)
	@$(call CHECK_BOARD_DISK,again,$(KERNEL_BUILD_DIR)/again.img); \
	$(KRFFS) get $(BOARD_BUILD_DIR)/again.img copy $(BOARD_BUILD_DIR)/again.copy || exit 1; \
	$(call COMPARE,again on the board,$(RACH2_DIR)/build/bin/ish,$(BOARD_BUILD_DIR)/again.copy,the copy of a file differs from the file); \
	echo "passed: the disk after kernel/again on the board"
	@$(BRIDGE) disk put $(KERNEL_DISK)
	@$(BRIDGE) check $(KERNEL) kernel/tests.expect --input kernel/tests.in --for $(BOARD_KERNEL_SECONDS)
	@$(call CHECK_BOARD_DISK,tests,$(KERNEL_BUILD_DIR)/tests.img); \
	echo "passed: the disk after kernel/tests on the board"
	@$(BRIDGE) check $(KERNEL) kernel/screen.expect --input kernel/screen.in --for $(BOARD_KERNEL_SECONDS) \
	  --press "$(BOARD_SCREEN_PRESSES)" --frame kernel/screen.frame

# The keyboard on the board: the Pico sends three bytes as a keyboard does,
# and `board/keys.s` prints what the keyboard controller of the chip got.
# The emulator runs the program first, with the same bytes. `board-suite`
# runs it first of all. It needs the two wires of the keyboard: from GP16
# and GP17 of the Pico to the two keyboard pins of the board.
.PHONY: board-keys
board-keys: $(REFTRACE) $(ASA)
	@mkdir -p $(BOARD_BUILD_DIR)
	@$(ASA) board/keys.s $(BOARD_BUILD_DIR)/keys.bin
	@if ! $(REFTRACE) $(REFTRACE_FLAGS) -e board/keys.events -o $(BOARD_BUILD_DIR)/keys.out $(BOARD_BUILD_DIR)/keys.bin \
	      2> $(BOARD_BUILD_DIR)/keys.log || ! cmp -s board/keys.expect $(BOARD_BUILD_DIR)/keys.out; then \
		cat $(BOARD_BUILD_DIR)/keys.log; \
		echo "FAILED: board/keys on the emulator"; \
		exit 1; \
	fi
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/keys.bin board/keys.expect --keys "$(BOARD_KEYS)" --for 10

# The push button on the board: the program sleeps, and each of two presses
# wakes it with an interrupt. The bridge presses the button itself, with its
# command `press`, so nobody has to stand at the table. `board-suite` runs
# it too.
.PHONY: board-button
board-button: $(ASA) $(REFTRACE)
	@mkdir -p $(BOARD_BUILD_DIR)
	@$(ASA) board/button.s $(BOARD_BUILD_DIR)/button.bin
	@if ! $(REFTRACE) $(REFTRACE_FLAGS) -e board/button.events -o $(BOARD_BUILD_DIR)/button.out $(BOARD_BUILD_DIR)/button.bin \
	      2> $(BOARD_BUILD_DIR)/button.log || ! cmp -s board/button.expect $(BOARD_BUILD_DIR)/button.out; then \
		cat $(BOARD_BUILD_DIR)/button.log; \
		echo "FAILED: board/button on the emulator"; \
		exit 1; \
	fi
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/button.bin board/button.expect --press "$(BOARD_PRESSES)" --for 10

# The screen on the board: the scenes of `../asc2-demos/` run on the chip,
# and the last frame that the bridge has sent to the display must be the
# frame that the emulator shows, pixel for pixel. Whether the glass shows
# it is for the eye. `board-suite` runs it too.
.PHONY: board-screen
board-screen:
	@$(MAKE) -s -C ../asc2-demos check
	@$(BRIDGE) check ../asc2-demos/build/scenes.bin ../asc2-demos/scenes.expect --frame ../asc2-demos/scenes.frame --for 60

# The debug port on the board: the start of a C program runs one
# instruction at a time, and around every step the debug port of the chip
# must show the address and the instruction that the emulator has there.
# `board-suite` runs it too. It needs the three wires of the debug port:
# from GP18, GP19, and GP20 of the Pico to pins 1, 3, and 5 of the board's
# header.
.PHONY: board-regs
board-regs: c-suite
	@mkdir -p $(BOARD_BUILD_DIR)
	@$(REFTRACE) -n $(BOARD_STEPS) $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).bin > $(BOARD_BUILD_DIR)/steps.trace 2> /dev/null; \
	$(BRIDGE) check $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).bin $(C_BUILD_DIR)/$(BOARD_STEPS_TEST).expect \
	  --steps $(BOARD_BUILD_DIR)/steps.trace --regs

# The lock of the debug port on the board: `board/lock.s` locks the port.
# The bridge reads the port before the first instruction, where it must be
# open, and after the last line of the program, where it must show the lock
# and no register. `board-suite` runs it before `board-regs`, which reads
# the registers of the next program through the port: the reset has opened
# it. It needs the three wires of the debug port.
.PHONY: board-lock
board-lock: $(ASA) $(REFTRACE)
	@mkdir -p $(BOARD_BUILD_DIR)
	@$(ASA) board/lock.s $(BOARD_BUILD_DIR)/lock.bin
	@if ! $(REFTRACE) $(REFTRACE_FLAGS) -o $(BOARD_BUILD_DIR)/lock.out $(BOARD_BUILD_DIR)/lock.bin \
	      2> $(BOARD_BUILD_DIR)/lock.log || ! cmp -s board/lock.expect $(BOARD_BUILD_DIR)/lock.out; then \
		cat $(BOARD_BUILD_DIR)/lock.log; \
		echo "FAILED: board/lock on the emulator"; \
		exit 1; \
	fi
	@$(BRIDGE) check $(BOARD_BUILD_DIR)/lock.bin board/lock.expect --lock --for 10

# Prints what the instruction cache and the array of the dot products save:
# the requests on the bus of six small jobs, written for the chip before the
# two and for the chip with them (`gain/gain.py`). It is not a test, but every
# value that a job stores is compared with a model in Python.
.PHONY: gain
gain: $(ASA) $(REFTRACE)
	python3 -I gain/gain.py $(ASA) $(REFTRACE) build/gain

# Prints the time of the same jobs on the board, in microseconds by the clock
# of the bridge (`gain/gain_time.py`). `make gain` in `chip` prints their
# clocks on the simulated chip.
.PHONY: board-gain
board-gain: $(ASA) $(REFTRACE)
	python3 -I gain/gain_time.py board $(ASA) $(REFTRACE) ../asc2-bridge/host/bridge.py build/gain-board

.PHONY: clean
clean:
	rm -rf build

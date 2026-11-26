CC=gcc
ASA=../asc2-assembler/asa
TCC_DIR=../asc2-tcc
TCC=$(TCC_DIR)/build/tcc
LIBTCC1=$(TCC_DIR)/build/libtcc1.a
ELF2FW=../asc2-tools/elf2fw/asc_elf2fw
KRFFS=../asc2-tools/krffs/asc_krffs
BUILD_DIR=build

# Definitions for the C code of the kernel, for example `-DSLICE_STATS -DSLICE_POLICY=0` (`sys/proc/slice.h`)
KERNEL_DEFS=

# The addresses of a firmware image: the code in the boot ROM, the variables in RAM
KERNEL_LDFLAGS=-nostdlib -Wl,-Ttext=0x80800000 -Wl,-Tdata=0x80001000

# The start-up code goes first: the CPU starts at the first instruction of the image
KERNEL_OBJS=loader.o kernel.o \
            sys/cpu.o sys/isr_handler.o sys/isr.o sys/console.o sys/kio.o sys/panic.o \
            sys/pic.o sys/pit.o sys/rp.o sys/disk.o sys/syscall.o sys/screen.o \
            sys/lib/kstring.o \
            sys/mm/pages.o sys/mm/paging.o \
            sys/fs/krffs.o sys/fs/file.o \
            sys/proc/switch.o sys/proc/process.o sys/proc/sched.o sys/proc/slice.o sys/proc/exec.o sys/proc/array.o

KERNEL_HEADERS=$(wildcard sys/*.h sys/*/*.h)

# The programs of the disk: the shell, the utilities, and the programs that check the kernel
ISH_OBJS=ish/ish.o ish/ish_cstring_utilities.o ish/ish_shell_utilities.o
UTILS=echo cat ls cp mv rm touch ed

# The programs that take the screen and the button: the sources are those of the bare machine, in `../asc2-demos`
DEMOS_DIR=../asc2-demos
SCREEN_PROGRAMS=burkut scenes race pilot guess fence scope sched shapes flight film mark tour
TESTS=work crash forks execs args files dots array load screen

# The programs of the screen that the disk of the tests has: the tests of the files need the rest of its room
TEST_SCREEN_PROGRAMS=burkut scenes

# The code that every program is linked with: the start-up code and the library
USER_OBJS=$(addprefix $(BUILD_DIR)/,user/crt0.o user/text.o user/syscalls.o)
USER_HEADERS=user/user.h $(wildcard ish/*.h tests/*.h)

PROGRAMS=$(addprefix $(BUILD_DIR)/bin/,ish $(UTILS) $(SCREEN_PROGRAMS))
TEST_PROGRAMS=$(addprefix $(BUILD_DIR)/tests/bin/,$(TESTS))

# The size of the disk: the 64 sectors that the bridge keeps in the flash of the Pico
DISK_KB=256

COMPILE_PROGRAM=$(TCC) -I user -c -o $@ $<
LINK_PROGRAM=$(TCC) -nostdlib -o $@ $^ $(LIBTCC1)

.PHONY: all
all:
	$(MAKE) -C ../asc2-assembler all
	$(MAKE) -C $(TCC_DIR) all
	$(MAKE) -C ../asc2-tools/elf2fw all
	$(MAKE) -C ../asc2-tools/krffs all
	$(MAKE) $(BUILD_DIR)/machine.checked $(BUILD_DIR)/rach2.bin $(BUILD_DIR)/disk.img $(BUILD_DIR)/tests/disk.img

# `sys/machine.h` repeats numbers of `asc.h`. The two files are compiled
# together, and the compiler refuses a name that has two values.
$(BUILD_DIR)/machine.checked: sys/machine.h ../asc2-emulated/asc.h
	@mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -fsyntax-only -Werror -x c -include ../asc2-emulated/asc.h sys/machine.h
	@touch $@

# The programs are compiled with the header of their library. These rules
# stand before the rule of the kernel, which fits their files too.
$(BUILD_DIR)/user/%.o: user/%.c $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILE_PROGRAM)

$(BUILD_DIR)/ish/%.o: ish/%.c $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILE_PROGRAM)

$(BUILD_DIR)/utils/%.o: utils/%.c $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILE_PROGRAM)

$(BUILD_DIR)/tests/%.o: tests/%.c $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(COMPILE_PROGRAM)

$(BUILD_DIR)/screen/machine.o: $(DEMOS_DIR)/lib/machine.c $(DEMOS_DIR)/lib/machine.h $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(TCC) -DRACH2 -I user -c -o $@ $<

$(BUILD_DIR)/screen/show.o: $(DEMOS_DIR)/lib/show.c $(DEMOS_DIR)/lib/show.h $(DEMOS_DIR)/lib/machine.h
	@mkdir -p $(dir $@)
	$(TCC) -DRACH2 -c -o $@ $<

# `pilot.c` is the game with a neuron that plays it
$(BUILD_DIR)/screen/pilot.o: $(DEMOS_DIR)/burkut.c

# `shapes.c`, `flight.c`, and `film.c` take their models and pictures from files that the programs of `../asc2-kb/models` write
$(BUILD_DIR)/screen/shapes.o: $(DEMOS_DIR)/shapes_models.h
$(BUILD_DIR)/screen/flight.o: $(DEMOS_DIR)/flight_land.h
$(BUILD_DIR)/screen/film.o: $(DEMOS_DIR)/shapes_models.h $(DEMOS_DIR)/film_pictures.h

# `sched.c`, `mark.c`, and `tour.c` use the system calls of the kernel itself: they make children, ask about the scheduler, read and write files
$(addprefix $(BUILD_DIR)/screen/,sched.o mark.o tour.o): $(BUILD_DIR)/screen/%.o: $(DEMOS_DIR)/%.c $(DEMOS_DIR)/lib/machine.h $(DEMOS_DIR)/lib/show.h $(USER_HEADERS)
	@mkdir -p $(dir $@)
	$(TCC) -DRACH2 $(MARK_DEFS) -I user -c -o $@ $<

$(BUILD_DIR)/screen/%.o: $(DEMOS_DIR)/%.c $(DEMOS_DIR)/lib/machine.h $(DEMOS_DIR)/lib/show.h $(DEMOS_DIR)/pictures.h
	@mkdir -p $(dir $@)
	$(TCC) -DRACH2 -c -o $@ $<

$(BUILD_DIR)/screen/%.o: $(DEMOS_DIR)/%.s
	@mkdir -p $(dir $@)
	$(ASA) -c $< $@

$(BUILD_DIR)/screen/%.o: $(DEMOS_DIR)/lib/%.s
	@mkdir -p $(dir $@)
	$(ASA) -c $< $@

$(BUILD_DIR)/%.o: %.c $(KERNEL_HEADERS)
	@mkdir -p $(dir $@)
	$(TCC) -I . $(KERNEL_DEFS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(ASA) -c $< $@

# A program is an ELF file with the name that it has on the disk. The linker
# chooses its addresses by itself.
$(BUILD_DIR)/bin/ish: $(addprefix $(BUILD_DIR)/,$(ISH_OBJS)) $(USER_OBJS)
	@mkdir -p $(dir $@)
	$(LINK_PROGRAM)

$(addprefix $(BUILD_DIR)/bin/,$(SCREEN_PROGRAMS)): $(BUILD_DIR)/bin/%: $(BUILD_DIR)/screen/%.o $(BUILD_DIR)/screen/machine.o $(USER_OBJS)
	@mkdir -p $(dir $@)
	$(LINK_PROGRAM)

$(BUILD_DIR)/bin/%: $(BUILD_DIR)/utils/%.o $(USER_OBJS)
	@mkdir -p $(dir $@)
	$(LINK_PROGRAM)

$(BUILD_DIR)/tests/bin/%: $(BUILD_DIR)/tests/%.o $(BUILD_DIR)/tests/check.o $(USER_OBJS)
	@mkdir -p $(dir $@)
	$(LINK_PROGRAM)

# The parts of the programs of the screen that are written in assembly
$(BUILD_DIR)/bin/race: $(BUILD_DIR)/screen/race_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/pilot: $(BUILD_DIR)/screen/pilot_asm.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/guess: $(BUILD_DIR)/screen/guess_asm.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/fence: $(BUILD_DIR)/screen/fence_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/scope: $(BUILD_DIR)/screen/scope_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/sched: $(BUILD_DIR)/screen/sched_asm.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/shapes: $(BUILD_DIR)/screen/shapes_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/flight: $(BUILD_DIR)/screen/flight_asm.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/film: $(BUILD_DIR)/screen/film_asm.o $(BUILD_DIR)/screen/shapes_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/mark: $(BUILD_DIR)/screen/mark_asm.o $(BUILD_DIR)/screen/race_asm.o $(BUILD_DIR)/screen/array.o $(BUILD_DIR)/screen/show.o
$(BUILD_DIR)/bin/tour: $(BUILD_DIR)/screen/show.o

# The parts of the test programs that are written in assembly
$(BUILD_DIR)/tests/bin/work: $(BUILD_DIR)/tests/work_asm.o
$(BUILD_DIR)/tests/bin/crash: $(BUILD_DIR)/tests/crash_asm.o
$(BUILD_DIR)/tests/bin/dots: $(BUILD_DIR)/tests/dots_asm.o
$(BUILD_DIR)/tests/bin/array: $(BUILD_DIR)/tests/array_asm.o

$(BUILD_DIR)/rach2.elf: $(addprefix $(BUILD_DIR)/,$(KERNEL_OBJS))
	$(TCC) $(KERNEL_LDFLAGS) -o $@ $^ $(LIBTCC1)

$(BUILD_DIR)/rach2.bin: $(BUILD_DIR)/rach2.elf
	$(ELF2FW) $< $@

# The disk of the machine: a file system with the shell, the utilities, the
# programs of the screen, and `motd`, the welcome that the shell prints
$(BUILD_DIR)/disk.img: $(PROGRAMS) motd
	$(KRFFS) make $@ $(DISK_KB) $^

# The disk of the tests: the shell, the utilities, two programs of the screen, and the programs that check the kernel
$(BUILD_DIR)/tests/disk.img: $(addprefix $(BUILD_DIR)/bin/,ish $(UTILS) $(TEST_SCREEN_PROGRAMS)) $(TEST_PROGRAMS)
	$(KRFFS) make $@ $(DISK_KB) $^

# Puts the system on the bench: the disk and the kernel go into the flash of
# the Pico, which from then on starts the kernel by itself at every power-up.
# Any serial terminal is then the console of the machine.
BRIDGE=python3 -I ../asc2-bridge/host/bridge.py

.PHONY: install
install: all
	$(BRIDGE) disk put $(BUILD_DIR)/disk.img
	$(BRIDGE) save $(BUILD_DIR)/rach2.bin

# Keeps the object files, which `make` would remove as intermediate files
.SECONDARY:

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

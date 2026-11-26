# Builds the demo programs of this directory for the bare machine, with the
# C compiler and the assembler of the project.
ASA=../asc2-assembler/asa
TCC_DIR=../asc2-tcc
TCC=$(TCC_DIR)/build/tcc
LIBTCC1=$(TCC_DIR)/build/libtcc1.a
ELF2FW=../asc2-tools/elf2fw/asc_elf2fw
REFTRACE=../asc2-tools/reftrace/asc_reftrace
BRIDGE=python3 -I ../asc2-bridge/host/bridge.py
BUILD_DIR=build
LDFLAGS=-nostdlib -Wl,-Ttext=0x80800000 -Wl,-Tdata=0x80001000

DEMOS=speed scenes burkut cube learn
LIB=$(BUILD_DIR)/machine.o $(BUILD_DIR)/console.o

.PHONY: all
all: $(addprefix $(BUILD_DIR)/,$(addsuffix .bin,$(DEMOS)))

$(BUILD_DIR)/%.o: lib/%.s
	@mkdir -p $(BUILD_DIR)
	$(ASA) -c $< $@

$(BUILD_DIR)/machine.o: lib/machine.c lib/machine.h
	@mkdir -p $(BUILD_DIR)
	$(TCC) -c -o $@ $<

pictures.h: pictures.py
	python3 -I pictures.py > $@

$(BUILD_DIR)/%.o: %.c lib/machine.h pictures.h
	@mkdir -p $(BUILD_DIR)
	$(TCC) -c -o $@ $<

$(BUILD_DIR)/%_asm.o: %_asm.s
	@mkdir -p $(BUILD_DIR)
	$(ASA) -c $< $@

# The parts of the programs that are written in assembly
$(BUILD_DIR)/cube.bin: $(BUILD_DIR)/cube_asm.o $(BUILD_DIR)/array.o
$(BUILD_DIR)/learn.bin: $(BUILD_DIR)/array.o

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/start.o $(BUILD_DIR)/%.o $(LIB)
	$(TCC) $(LDFLAGS) -o $(BUILD_DIR)/$*.elf $^ $(LIBTCC1)
	$(ELF2FW) $(BUILD_DIR)/$*.elf $@

# The scenes on the emulator: what the console prints and what the screen
# shows at the end must be `scenes.expect` and `scenes.frame`. Then the game:
# with the presses of `burkut.events` the burkut passes 12 pillars and falls.
# The game never ends, so the run stops at a number of instructions, and the
# trace tool says so with its status 4.
BURKUT_STEPS=4000000

.PHONY: check
check: $(BUILD_DIR)/scenes.bin $(BUILD_DIR)/burkut.bin
	@$(REFTRACE) -q -r -n 50000000 -o $(BUILD_DIR)/scenes.out -f $(BUILD_DIR)/scenes.frame $(BUILD_DIR)/scenes.bin \
	  2> $(BUILD_DIR)/scenes.log || { cat $(BUILD_DIR)/scenes.log; echo "FAILED: scenes"; exit 1; }
	@cmp -s scenes.expect $(BUILD_DIR)/scenes.out || { echo "FAILED: scenes (the console output differs from scenes.expect)"; exit 1; }
	@cmp -s scenes.frame $(BUILD_DIR)/scenes.frame || { echo "FAILED: scenes (the screen differs from scenes.frame)"; exit 1; }
	@echo "passed: scenes"
	@$(REFTRACE) -q -r -n $(BURKUT_STEPS) -e burkut.events -o $(BUILD_DIR)/burkut.out -m $(BUILD_DIR)/burkut.film $(BUILD_DIR)/burkut.bin \
	  2> $(BUILD_DIR)/burkut.log; test $$? -eq 4 || { cat $(BUILD_DIR)/burkut.log; echo "FAILED: burkut"; exit 1; }
	@cmp -s burkut.expect $(BUILD_DIR)/burkut.out || { echo "FAILED: burkut (the console output differs from burkut.expect)"; exit 1; }
	@echo "passed: burkut (a flight of 12 points with the presses of burkut.events)"

# Flies the game anew on the emulator and writes the presses and the console
# output that `make check` compares with. The game must be flown anew after
# every change of `burkut.c`: the presses are counted in instructions.
.PHONY: pilot
pilot: $(BUILD_DIR)/burkut.bin
	python3 -I pilot.py $(REFTRACE) $(BUILD_DIR)/burkut.bin 12 burkut.events
	-$(REFTRACE) -q -r -n $(BURKUT_STEPS) -e burkut.events -o burkut.expect $(BUILD_DIR)/burkut.bin 2> /dev/null

# Loads a program into the bridge on the bench and stays as its terminal
# (Ctrl-] leaves): `make run-burkut`.
.PHONY: run-%
run-%: $(BUILD_DIR)/%.bin
	$(BRIDGE) run $<

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

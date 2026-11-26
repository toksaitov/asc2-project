VERILATOR=verilator
IVERILOG=iverilog
VVP=vvp
SV2V=sv2v
TOP_MODULE=tt_um_toksaitov_almurut2
BUILD_DIR=build

# The chip: a clone of its repository beside this one (`../../chip.mk`)
ROOT=../..
include $(ROOT)/chip.mk
VERILOG_SRCS=$(CHIP_SRCS)

# The Tiny Tapeout tools on this computer: a clone of `tt-support-tools` and
# a Python environment with its requirements. `asc2-kb/tt-flow.md` tells how
# both were set up.
TT_SETUP=$(HOME)/ttsetup
TT_TOOLS=$(TT_SETUP)/smoke/tt
TT_VENV=$(TT_SETUP)/venv

# The repository of the chip is the Tiny Tapeout project: `info.yaml`
# there, and the sources and `config.json` in its `src/`
INFO_YAML=$(CHIP_INFO)

# A copy of the project, made for a layout run: the files of the chip's
# repository that git keeps or would keep
PROJECT_DIR=$(BUILD_DIR)/project

# The size of the chip in tiles. `info.yaml` holds the size that is
# submitted; another size on the command line tries a layout run with it.
TILES=$(shell sed -n 's/^ *tiles: *"\(.*\)"/\1/p' $(INFO_YAML))

# The settings of the layout run. Another file on the command line tries a
# layout run with other settings.
CONFIG=$(CHIP_DIR)/src/config.json

# The tools refuse a project without a remote. They read its address and do not contact it.
PROJECT_REMOTE=https://github.com/toksaitov/ttsky26e-almurut2

# The gates that the layout run made of the chip, and the Verilog models of
# the cells of the SKY130 library, which the run keeps in `~/.ciel/`
NETLIST=$(PROJECT_DIR)/runs/wokwi/final/nl/$(TOP_MODULE).nl.v
CELLS_DIR=$(firstword $(wildcard $(HOME)/.ciel/ciel/sky130/versions/*/sky130A/libs.ref/sky130_fd_sc_hd/verilog))
GATES_SEEDS=1 2 3 4 5 6 7 8

# The test of the start on the demo board of Tiny Tapeout runs for both
# revisions of the board (`+board=2` is v3.2, `+board=3` is v3.3), for the
# three levels that a free pin of the bus can read, and for these seeds
POWERUP_BOARDS=2 3
POWERUP_PULLS=0 1 2
POWERUP_SEEDS=1 2 3 4

# Runs the test of the start that `$(1)` names, on `$(2)`
define RUN_POWERUP
	@for board in $(POWERUP_BOARDS); do for pull in $(POWERUP_PULLS); do for seed in $(POWERUP_SEEDS); do \
		$(1) +board=$$board +pull=$$pull +verilator+rand+reset+2 +verilator+seed+$$seed > $(BUILD_DIR)/powerup.run.log 2>&1 \
		  || { grep FAILED $(BUILD_DIR)/powerup.run.log | head -5; echo "FAILED: tt_um_powerup_tb$(2) (board v3.$$board, pull $$pull, seed $$seed)"; exit 1; }; \
	done; done; echo "passed: tt_um_powerup_tb$(2) (board v3.$$board, 3 levels of the free pins, `echo $(POWERUP_SEEDS) | wc -w | tr -d ' '` seeds)"; done
endef

.PHONY: all
all: lint test powerup

.PHONY: lint
lint:
	$(CHIP_CHECK)
	$(VERILATOR) --lint-only -Wall --top-module $(TOP_MODULE) $(VERILOG_SRCS)

.PHONY: test
test: $(BUILD_DIR)/tb/Vtt_um_tb
	$(BUILD_DIR)/tb/Vtt_um_tb

$(BUILD_DIR)/tb/Vtt_um_tb: tb/tt_um_tb.sv $(VERILOG_SRCS) $(INFO_YAML)
	$(CHIP_CHECK)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wall -j 4 --top-module tt_um_tb -Mdir $(BUILD_DIR)/tb \
	  tb/tt_um_tb.sv $(VERILOG_SRCS) > $(BUILD_DIR)/tb.log 2>&1 || { tail -20 $(BUILD_DIR)/tb.log; exit 1; }

# The start of the chip on the demo board: a design that is selected late,
# flops that start with values by chance, the reset pin of both revisions
# of the board, and the reset button
.PHONY: powerup
powerup: $(BUILD_DIR)/powerup/Vtt_um_powerup_tb
	$(call RUN_POWERUP,$(BUILD_DIR)/powerup/Vtt_um_powerup_tb,)

$(BUILD_DIR)/powerup/Vtt_um_powerup_tb: tb/tt_um_powerup_tb.sv $(VERILOG_SRCS) $(INFO_YAML)
	$(CHIP_CHECK)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wall -j 4 --top-module tt_um_powerup_tb -Mdir $(BUILD_DIR)/powerup \
	  tb/tt_um_powerup_tb.sv $(VERILOG_SRCS) > $(BUILD_DIR)/powerup.log 2>&1 || { tail -20 $(BUILD_DIR)/powerup.log; exit 1; }

# Copies the project anew, as GitHub would check it out: the files of the
# chip's repository, a clone of the tools in `tt/`, and a git repository of
# its own, as the tools demand. Each file that `info.yaml` names must be in
# `src/`.
.PHONY: project
project:
	$(CHIP_CHECK)
	@for f in $(VERILOG_SRCS); do test -f $$f || { echo "$$f is missing"; exit 1; }; done
	rm -rf $(PROJECT_DIR)
	mkdir -p $(PROJECT_DIR)
	cd $(CHIP_DIR) && git ls-files -z --cached --others --exclude-standard | \
	  rsync -a --from0 --files-from=- --ignore-missing-args ./ $(abspath $(PROJECT_DIR))/
	sed 's/^\( *tiles: *\)".*"/\1"$(TILES)"/' $(INFO_YAML) > $(PROJECT_DIR)/info.yaml
	cp $(CONFIG) $(PROJECT_DIR)/src/config.json
	git clone -q $(TT_TOOLS) $(PROJECT_DIR)/tt
	cd $(PROJECT_DIR) && git init -q && git remote add origin $(PROJECT_REMOTE) && \
	  git add -A && git commit -q -m "Put the project together"

# The official layout run. It needs Docker, takes minutes, and ends with the
# use of the area and the results of the timing checks.
.PHONY: harden
harden: project
	cd $(PROJECT_DIR) && . $(TT_VENV)/bin/activate && export PDK=sky130A LIBRELANE_TAG=3.0.14 && \
	  ./tt/tt_tool.py --create-user-config && \
	  ./tt/tt_tool.py --harden > harden.log 2>&1; \
	  status=$$?; tail -5 harden.log; test $$status -eq 0
	cd $(PROJECT_DIR) && . $(TT_VENV)/bin/activate && export PDK=sky130A LIBRELANE_TAG=3.0.14 && \
	  ./tt/tt_tool.py --print-stats && ./tt/tt_tool.py --print-warnings

# The official precheck, the rules of the shuttle on the layout of the last
# `make harden`, in a container. It needs Docker and about 12 GB of its disk;
# the first run fetches the PDK in the version of the official action and
# builds the image, which takes a while. The result is a table of checks.
PRECHECK_PDK_VERSION=0536d02d875c8f67dd7cca3902ac457e62f20005
PRECHECK_PDK_ROOT=$(BUILD_DIR)/pdk
PRECHECK_IMAGE=tt-precheck:local
PRECHECK_DIR=$(BUILD_DIR)/precheck

.PHONY: precheck
precheck:
	@test -d $(PROJECT_DIR)/runs/wokwi/final || { echo "there is no layout; run make harden first"; exit 1; }
	cd $(PROJECT_DIR) && . $(TT_VENV)/bin/activate && export PDK=sky130A LIBRELANE_TAG=3.0.14 && ./tt/tt_tool.py --create-tt-submission
	. $(TT_VENV)/bin/activate && ciel enable --pdk-root $(abspath $(PRECHECK_PDK_ROOT)) --pdk-family sky130 $(PRECHECK_PDK_VERSION) > /dev/null
	rm -rf $(PRECHECK_DIR) && mkdir -p $(PRECHECK_DIR)/context $(PRECHECK_DIR)/reports $(PRECHECK_DIR)/tmp
	cp precheck/Dockerfile $(PRECHECK_DIR)/context/ && cp -R $(PROJECT_DIR)/tt/precheck $(PRECHECK_DIR)/context/precheck
	docker build -q -t $(PRECHECK_IMAGE) $(PRECHECK_DIR)/context
	docker run --rm \
	  -v $(abspath $(PROJECT_DIR)):/proj:ro \
	  -v $(abspath $(PROJECT_DIR))/tt/tech:/tt/tech:ro \
	  -v $(abspath $(PRECHECK_PDK_ROOT))/ciel/sky130/versions/$(PRECHECK_PDK_VERSION):/pdk:ro \
	  -v $(abspath $(PRECHECK_DIR))/reports:/tt/precheck/reports \
	  -v $(abspath $(PRECHECK_DIR))/tmp:/tmp \
	  -e PDK_ROOT=/pdk -w /tt/precheck $(PRECHECK_IMAGE) \
	  nix-shell --run "python precheck.py --gds /proj/tt_submission/$(TOP_MODULE).gds --tech sky130A" > $(PRECHECK_DIR)/precheck.log 2>&1; \
	  status=$$?; cat $(PRECHECK_DIR)/reports/results.md; echo; test $$status -eq 0

# The test on the Verilog that `sv2v` makes of the sources. A tool that reads
# Verilog alone gets the chip in this form. Icarus Verilog is such a tool:
# it runs the test of the reset, with flops that start as unknown.
.PHONY: sv2v
sv2v:
	$(CHIP_CHECK)
	@mkdir -p $(BUILD_DIR)/sv2v
	$(SV2V) $(VERILOG_SRCS) > $(BUILD_DIR)/sv2v/$(TOP_MODULE).v
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wno-fatal -j 4 --top-module tt_um_tb -Mdir $(BUILD_DIR)/sv2v/tb \
	  tb/tt_um_tb.sv $(BUILD_DIR)/sv2v/$(TOP_MODULE).v > $(BUILD_DIR)/sv2v/tb.log 2>&1 || { tail -20 $(BUILD_DIR)/sv2v/tb.log; exit 1; }
	$(BUILD_DIR)/sv2v/tb/Vtt_um_tb
	$(IVERILOG) -g2012 -o $(BUILD_DIR)/sv2v/reset.vvp tb/tt_um_reset_tb.sv $(BUILD_DIR)/sv2v/$(TOP_MODULE).v
	$(VVP) -N $(BUILD_DIR)/sv2v/reset.vvp

# The test on the gates of the last layout run, without delays. It runs once
# for every seed: a seed gives every flop another value to start with, as
# the power does on the real chip. The test of the start on the demo board
# runs on the gates in the same way. Then Icarus Verilog runs the test of
# the reset on the gates, with flops that start as unknown.
.PHONY: gates
gates:
	@test -f $(NETLIST) || { echo "$(NETLIST) is missing; run make harden first"; exit 1; }
	@mkdir -p $(BUILD_DIR)/gates
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wno-fatal -Wno-lint -Wno-style -j 4 --top-module tt_um_tb -Mdir $(BUILD_DIR)/gates/tb \
	  +define+FUNCTIONAL +define+UNIT_DELAY=\#1 tb/tt_um_tb.sv $(NETLIST) $(CELLS_DIR)/primitives.v $(CELLS_DIR)/sky130_fd_sc_hd.v \
	  > $(BUILD_DIR)/gates/tb.log 2>&1 || { tail -20 $(BUILD_DIR)/gates/tb.log; exit 1; }
	@for seed in $(GATES_SEEDS); do \
		$(BUILD_DIR)/gates/tb/Vtt_um_tb +verilator+rand+reset+2 +verilator+seed+$$seed > $(BUILD_DIR)/gates/run.log 2>&1 \
		  || { grep FAILED $(BUILD_DIR)/gates/run.log | head -5; echo "FAILED: tt_um_tb on the gates (seed $$seed)"; exit 1; }; \
		echo "passed: tt_um_tb on the gates (seed $$seed)"; \
	done
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wno-fatal -Wno-lint -Wno-style -j 4 --top-module tt_um_powerup_tb -Mdir $(BUILD_DIR)/gates/powerup \
	  +define+FUNCTIONAL +define+UNIT_DELAY=\#1 tb/tt_um_powerup_tb.sv $(NETLIST) $(CELLS_DIR)/primitives.v $(CELLS_DIR)/sky130_fd_sc_hd.v \
	  > $(BUILD_DIR)/gates/powerup.log 2>&1 || { tail -20 $(BUILD_DIR)/gates/powerup.log; exit 1; }
	$(call RUN_POWERUP,$(BUILD_DIR)/gates/powerup/Vtt_um_powerup_tb, on the gates)
	$(IVERILOG) -g2012 -DFUNCTIONAL -DUNIT_DELAY=\#1 -o $(BUILD_DIR)/gates/reset.vvp \
	  tb/tt_um_reset_tb.sv $(NETLIST) $(CELLS_DIR)/primitives.v $(CELLS_DIR)/sky130_fd_sc_hd.v
	$(VVP) -N $(BUILD_DIR)/gates/reset.vvp

# Whole programs on the gates of the last layout run. The test program of
# `../../asc2-tests/chip/` plays the RP2040 for the gates as it does for the
# hardware description (`tb/asc_trace_gates_top.sv` stands between them).
# The gates do not show the registers of the CPU, so the chip runs on its own
# time, and what leaves its pins is compared with the emulator.
#
# First the programs of `../../asc2-tests/asm/` that end by themselves and
# need no input and no timer: every request that the gates send on the bus
# must be the request of the emulator, in the same order. A program that
# takes another way, or reads or writes another address or value, sends other
# requests. Each program runs from two sets of values that the flops start
# with. Then the kernel boots from its disk, the program `array` of its tests
# runs from the shell, and the console must show what it shows on the
# emulator. A clock of the gates takes about 50 times as long as a clock of
# the hardware description, so the kernel runs once, for about 20 minutes.
GATES_ASA=../../asc2-assembler/asa
GATES_REFTRACE=../../asc2-tools/reftrace/asc_reftrace
GATES_TRACE=$(BUILD_DIR)/gates/trace/asc_trace
GATES_ASM_DIR=../../asc2-tests/asm
GATES_RACH2_DIR=../../asc2-rach2
# These need typed keys, a disk, presses, or lost answers from the RP2040,
# count on the steps of the timer, wait for the display, which takes its
# time by the instructions of the emulator, or end with a double fault
GATES_NOT_RUN=cpu_double_fault cpu_irq_kbd cpu_irq_pit rp_button rp_console rp_disk rp_lost_answer rp_lost_vector rp_screen
GATES_PROGRAMS=$(filter-out $(GATES_NOT_RUN),$(basename $(notdir $(wildcard $(GATES_ASM_DIR)/*.s))))
GATES_PROGRAM_SEEDS=1 2
# The clocks of the gates for a program: this many for each request of the emulator, and some more
GATES_CLOCKS_A_REQUEST=100
GATES_KERNEL_CLOCKS=90000000

.PHONY: gates-programs
gates-programs:
	@test -f $(NETLIST) || { echo "$(NETLIST) is missing; run make harden first"; exit 1; }
	@mkdir -p $(BUILD_DIR)/gates/programs
	@$(MAKE) -s -C $(GATES_RACH2_DIR) all > /dev/null
	$(VERILATOR) --cc --exe --build --timing -j 4 -Wno-fatal -Wno-lint -Wno-style --top-module asc_trace_top --prefix Vchip \
	  -Mdir $(BUILD_DIR)/gates/trace +define+FUNCTIONAL +define+UNIT_DELAY=\#1 \
	  -CFLAGS "-std=c++20 -O2 -I$(abspath ../../asc2-tools/reftrace)" tb/asc_trace_gates_top.sv $(NETLIST) \
	  $(CELLS_DIR)/primitives.v $(CELLS_DIR)/sky130_fd_sc_hd.v $(abspath ../../asc2-tests/chip/asc_trace.cpp) -o asc_trace \
	  > $(BUILD_DIR)/gates/trace.log 2>&1 || { tail -20 $(BUILD_DIR)/gates/trace.log; exit 1; }
	@for name in $(GATES_PROGRAMS); do \
		base=$(BUILD_DIR)/gates/programs/$$name; \
		$(GATES_ASA) $(GATES_ASM_DIR)/$$name.s $$base.bin || exit 1; \
		$(GATES_REFTRACE) -q -r -n 5000000 -b $$base.emu.bus $$base.bin > /dev/null 2>&1 \
		  || { echo "FAILED: $$name on the emulator"; exit 1; }; \
		clocks=$$((`wc -l < $$base.emu.bus` * $(GATES_CLOCKS_A_REQUEST) + 20000)); \
		for seed in $(GATES_PROGRAM_SEEDS); do \
			ASC_SEED=$$seed $(GATES_TRACE) -t -q -r -n 100000000 -c $$clocks -b $$base.gates.bus $$base.bin > /dev/null 2>&1; \
			cmp -s $$base.emu.bus $$base.gates.bus \
			  || { echo "FAILED: $$name on the gates (seed $$seed): $$base.gates.bus differs from $$base.emu.bus"; exit 1; }; \
		done; \
		echo "passed: $$name on the gates (`wc -l < $$base.emu.bus | tr -d ' '` requests equal the emulator's, `echo $(GATES_PROGRAM_SEEDS) | wc -w | tr -d ' '` seeds)"; \
	done
	@printf 'array\nexit\n' > $(BUILD_DIR)/gates/programs/kernel.in
	@$(GATES_REFTRACE) -q -r -n 400000000 -d $(GATES_RACH2_DIR)/build/tests/disk.img -s $(BUILD_DIR)/gates/programs/kernel.img \
	  -i $(BUILD_DIR)/gates/programs/kernel.in -o $(BUILD_DIR)/gates/programs/kernel.emu.out $(GATES_RACH2_DIR)/build/rach2.bin \
	  > /dev/null 2>&1 || { echo "FAILED: the kernel with the program array on the emulator"; exit 1; }
	@$(GATES_TRACE) -t -q -r -n 400000000 -c $(GATES_KERNEL_CLOCKS) -d $(GATES_RACH2_DIR)/build/tests/disk.img \
	  -i $(BUILD_DIR)/gates/programs/kernel.in -o $(BUILD_DIR)/gates/programs/kernel.gates.out $(GATES_RACH2_DIR)/build/rach2.bin \
	  > /dev/null 2>&1; \
	cmp -s $(BUILD_DIR)/gates/programs/kernel.emu.out $(BUILD_DIR)/gates/programs/kernel.gates.out \
	  || { echo "FAILED: the kernel on the gates ($(BUILD_DIR)/gates/programs/kernel.gates.out differs from kernel.emu.out)"; exit 1; }
	@echo "passed: the kernel on the gates runs the program array from its shell as on the emulator"

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

VERILATOR=verilator
YOSYS=yosys
# The bus unit of the chip, from a clone of its repository beside this one (`../../chip.mk`)
ROOT=../..
include $(ROOT)/chip.mk
BUS_DIR=$(CHIP_DIR)/src
BUS_SRCS=$(CHIP_BUS_SRCS)
TB=asc_bus_tb.sv
BUILD_DIR=build

# The test bench uses delays, so Verilator builds it as a program of its own
VSIM=$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wno-fatal -j 4 --top-module asc_bus_tb

# The chip waits 2**16 clocks for an answer. The short tests use 2**10, so
# that a fault is over quickly; the long ones use the width of the chip.
LONG_FLAGS=-GTO_BITS=16 -GN_CLEAN=5000 -GN_FAULT=700

.PHONY: all
all: lint sim tear mutants gates

.PHONY: long
long: sim-long gates-long

.PHONY: lint
lint:
	$(CHIP_CHECK)
	$(VERILATOR) --lint-only -Wall --top-module asc_bus $(BUS_DIR)/asc_bus.sv
	$(VERILATOR) --lint-only -Wall --top-module asc_reset_filter $(BUS_DIR)/asc_reset_filter.sv
	$(VERILATOR) --lint-only -Wall --top-module asc_irq_filter $(BUS_DIR)/asc_irq_filter.sv

.PHONY: sim
sim:
	@mkdir -p $(BUILD_DIR)
	$(VSIM) -Mdir $(BUILD_DIR)/sim $(TB) $(BUS_SRCS) > $(BUILD_DIR)/sim.log 2>&1 || { tail -20 $(BUILD_DIR)/sim.log; exit 1; }
	$(BUILD_DIR)/sim/Vasc_bus_tb

.PHONY: sim-long
sim-long:
	@mkdir -p $(BUILD_DIR)
	$(VSIM) $(LONG_FLAGS) -Mdir $(BUILD_DIR)/sim-long $(TB) $(BUS_SRCS) > $(BUILD_DIR)/sim-long.log 2>&1 || { tail -20 $(BUILD_DIR)/sim-long.log; exit 1; }
	$(BUILD_DIR)/sim-long/Vasc_bus_tb

# The torn clock edge of the bench, on demand: a test input throws the
# state of the bus unit back to idle in the middle of a transaction
.PHONY: tear
tear:
	@mkdir -p $(BUILD_DIR)
	$(VSIM) +define+ASC_BUS_TEST_TEAR -Mdir $(BUILD_DIR)/tear $(TB) $(BUS_SRCS) > $(BUILD_DIR)/tear.log 2>&1 || { tail -20 $(BUILD_DIR)/tear.log; exit 1; }
	$(BUILD_DIR)/tear/Vasc_bus_tb

# Five broken copies of the bus files. The test must fail on each one,
# which shows that it can see the fault that each protection is there for:
#   1. No filter on ACK: noise on the wire must cause link faults.
#   2. FRAME stays high during the quiet time: the RP2040 that watches
#      FRAME must fail.
#   3. No filter on IRQ: noise pulses must reach the CPU.
#   4. No check for a state thrown back to idle: the bus unit must hang,
#      which the time limit of the test catches.
#   5. No filter on the reset: a noise pulse must reset the chip.
.PHONY: mutants
mutants:
	@mkdir -p $(BUILD_DIR)
	sed -e 's/ack_event  = ack_stable \&\& (ack_s0_q != ack_seen_q);/ack_event  = (ack_s0_q != ack_seen_q);/' $(BUS_DIR)/asc_bus.sv > $(BUILD_DIR)/mutant1_asc_bus.sv
	grep -q 'ack_event  = (ack_s0_q != ack_seen_q);' $(BUILD_DIR)/mutant1_asc_bus.sv
	$(VSIM) -Mdir $(BUILD_DIR)/mutant1 $(TB) $(BUS_DIR)/asc_reset_filter.sv $(BUS_DIR)/asc_irq_filter.sv $(BUILD_DIR)/mutant1_asc_bus.sv > $(BUILD_DIR)/mutant1.log 2>&1 || { tail -20 $(BUILD_DIR)/mutant1.log; exit 1; }
	! $(BUILD_DIR)/mutant1/Vasc_bus_tb > $(BUILD_DIR)/mutant1.run 2>&1
	grep FAIL $(BUILD_DIR)/mutant1.run | head -3
	sed -e 's/frame_q     <= (state_d != S_IDLE) \&\& (state_d != S_QUIET);/frame_q     <= (state_d != S_IDLE);/' $(BUS_DIR)/asc_bus.sv > $(BUILD_DIR)/mutant2_asc_bus.sv
	grep -q 'frame_q     <= (state_d != S_IDLE);' $(BUILD_DIR)/mutant2_asc_bus.sv
	$(VSIM) -Mdir $(BUILD_DIR)/mutant2 $(TB) $(BUS_DIR)/asc_reset_filter.sv $(BUS_DIR)/asc_irq_filter.sv $(BUILD_DIR)/mutant2_asc_bus.sv > $(BUILD_DIR)/mutant2.log 2>&1 || { tail -20 $(BUILD_DIR)/mutant2.log; exit 1; }
	! timeout 600 $(BUILD_DIR)/mutant2/Vasc_bus_tb > $(BUILD_DIR)/mutant2.run 2>&1
	grep FAIL $(BUILD_DIR)/mutant2.run | head -3 || tail -3 $(BUILD_DIR)/mutant2.run
	sed -e 's/if (sync_q == hist_q \&\& hist_q == hist2_q) begin/if (1'"'"'b1) begin/' $(BUS_DIR)/asc_irq_filter.sv > $(BUILD_DIR)/mutant3_asc_irq_filter.sv
	grep -q "if (1'b1) begin" $(BUILD_DIR)/mutant3_asc_irq_filter.sv
	$(VSIM) -Mdir $(BUILD_DIR)/mutant3 $(TB) $(BUS_DIR)/asc_reset_filter.sv $(BUILD_DIR)/mutant3_asc_irq_filter.sv $(BUS_DIR)/asc_bus.sv > $(BUILD_DIR)/mutant3.log 2>&1 || { tail -20 $(BUILD_DIR)/mutant3.log; exit 1; }
	! $(BUILD_DIR)/mutant3/Vasc_bus_tb > $(BUILD_DIR)/mutant3.run 2>&1
	grep FAIL $(BUILD_DIR)/mutant3.run | head -3
	sed -e 's/                if (open_q) begin/                if (1'"'"'b0) begin/' $(BUS_DIR)/asc_bus.sv > $(BUILD_DIR)/mutant4_asc_bus.sv
	grep -q "                if (1'b0) begin" $(BUILD_DIR)/mutant4_asc_bus.sv
	$(VSIM) -Mdir $(BUILD_DIR)/mutant4 $(TB) $(BUS_DIR)/asc_reset_filter.sv $(BUS_DIR)/asc_irq_filter.sv $(BUILD_DIR)/mutant4_asc_bus.sv > $(BUILD_DIR)/mutant4.log 2>&1 || { tail -20 $(BUILD_DIR)/mutant4.log; exit 1; }
	! $(BUILD_DIR)/mutant4/Vasc_bus_tb > $(BUILD_DIR)/mutant4.run 2>&1
	grep FAIL $(BUILD_DIR)/mutant4.run | head -3
	sed -e 's/if (&{past_q, sync_q}) begin/if (sync_q) begin/' $(BUS_DIR)/asc_reset_filter.sv > $(BUILD_DIR)/mutant5_asc_reset_filter.sv
	grep -q "if (sync_q) begin" $(BUILD_DIR)/mutant5_asc_reset_filter.sv
	$(VSIM) -Mdir $(BUILD_DIR)/mutant5 $(TB) $(BUILD_DIR)/mutant5_asc_reset_filter.sv $(BUS_DIR)/asc_irq_filter.sv $(BUS_DIR)/asc_bus.sv > $(BUILD_DIR)/mutant5.log 2>&1 || { tail -20 $(BUILD_DIR)/mutant5.log; exit 1; }
	! $(BUILD_DIR)/mutant5/Vasc_bus_tb > $(BUILD_DIR)/mutant5.run 2>&1
	grep FAIL $(BUILD_DIR)/mutant5.run | head -3
	@echo "mutants: all five noticed"

# The same test on the gates that Yosys makes from the three files (generic
# gates without delays). Yosys is the synthesis tool of the Tiny Tapeout flow.
define GATES_SCRIPT
read_verilog -sv $(BUS_DIR)/asc_reset_filter.sv
synth -top asc_reset_filter -flatten
write_verilog -noattr $(1)/asc_reset_filter.v
design -reset
read_verilog -sv $(BUS_DIR)/asc_irq_filter.sv
synth -top asc_irq_filter -flatten
write_verilog -noattr $(1)/asc_irq_filter.v
design -reset
read_verilog -sv $(BUS_DIR)/asc_bus.sv
chparam -set TO_BITS $(2) asc_bus
synth -top asc_bus -flatten
stat
write_verilog -noattr $(1)/asc_bus.v
endef
export GATES_SCRIPT_SHORT = $(call GATES_SCRIPT,$(BUILD_DIR)/gates,10)
export GATES_SCRIPT_LONG = $(call GATES_SCRIPT,$(BUILD_DIR)/gates-long,16)

.PHONY: gates
gates:
	@mkdir -p $(BUILD_DIR)/gates
	printf '%s\n' "$$GATES_SCRIPT_SHORT" > $(BUILD_DIR)/gates/synth.ys
	$(YOSYS) -q -l $(BUILD_DIR)/gates/synth.log $(BUILD_DIR)/gates/synth.ys > /dev/null || { tail -20 $(BUILD_DIR)/gates/synth.log; exit 1; }
	$(VSIM) +define+GATE_LEVEL -Mdir $(BUILD_DIR)/gates/sim $(TB) $(BUILD_DIR)/gates/asc_reset_filter.v $(BUILD_DIR)/gates/asc_irq_filter.v $(BUILD_DIR)/gates/asc_bus.v \
	  > $(BUILD_DIR)/gates/build.log 2>&1 || { tail -20 $(BUILD_DIR)/gates/build.log; exit 1; }
	$(BUILD_DIR)/gates/sim/Vasc_bus_tb

.PHONY: gates-long
gates-long:
	@mkdir -p $(BUILD_DIR)/gates-long
	printf '%s\n' "$$GATES_SCRIPT_LONG" > $(BUILD_DIR)/gates-long/synth.ys
	$(YOSYS) -q -l $(BUILD_DIR)/gates-long/synth.log $(BUILD_DIR)/gates-long/synth.ys > /dev/null || { tail -20 $(BUILD_DIR)/gates-long/synth.log; exit 1; }
	$(VSIM) +define+GATE_LEVEL $(LONG_FLAGS) -Mdir $(BUILD_DIR)/gates-long/sim $(TB) \
	  $(BUILD_DIR)/gates-long/asc_reset_filter.v $(BUILD_DIR)/gates-long/asc_irq_filter.v $(BUILD_DIR)/gates-long/asc_bus.v \
	  > $(BUILD_DIR)/gates-long/build.log 2>&1 || { tail -20 $(BUILD_DIR)/gates-long/build.log; exit 1; }
	$(BUILD_DIR)/gates-long/sim/Vasc_bus_tb

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

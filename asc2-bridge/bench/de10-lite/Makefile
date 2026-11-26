PROJECT=bench_mem

# Loads the design into the FPGA's SRAM over JTAG. Nothing here writes the
# board's configuration flash: there is no .pof target, by design.

# The bus unit of the chip, from a clone of its repository beside this one
# (`../../../chip.mk`). `bench_mem.qsf` names the same three files.
ROOT=../../..
include $(ROOT)/chip.mk
BUS=$(CHIP_DIR)/src

.PHONY: all
all: build

.PHONY: build
build:
	quartus_sh --flow compile $(PROJECT)

.PHONY: upload
upload:
	quartus_pgm -c 'USB-Blaster [USB-0]' -m jtag -o "p;output_files/$(PROJECT).sof"

.PHONY: lint-local
lint-local:
	verilator --lint-only -Wall +define+ASC_BUS_TEST_TEAR --top-module $(PROJECT) lint/altsource_probe.sv lint/altpll.sv \
		$(BUS)/asc_reset_filter.sv $(BUS)/asc_irq_filter.sv $(BUS)/asc_bus.sv dly_line.sv $(PROJECT).sv

.PHONY: sim
sim:
	verilator --binary --timing -Wno-fatal -j 4 +define+ASC_BUS_TEST_TEAR --top-module tb_bench_mem -Mdir obj_tb \
		tb/sim_altsource_probe.sv tb/sim_altpll.sv $(BUS)/asc_reset_filter.sv $(BUS)/asc_irq_filter.sv $(BUS)/asc_bus.sv \
		dly_line.sv $(PROJECT).sv tb/tb_bench_mem.sv > obj_tb.log 2>&1 || { tail -30 obj_tb.log; exit 1; }
	./obj_tb/Vtb_bench_mem

.PHONY: clean
clean:
	rm -rf db incremental_db output_files obj_tb obj_tb.log

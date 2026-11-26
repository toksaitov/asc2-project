PROJECT=asc2

QUARTUS_MAP=quartus_map
QUARTUS_FIT=quartus_fit
QUARTUS_ASM=quartus_asm
QUARTUS_STA=quartus_sta
QUARTUS_PGM=quartus_pgm

VERILATOR=verilator
BUILD_DIR=build

# The chip: a clone of its repository beside this one (`../../chip.mk`)
ROOT=../..
include $(ROOT)/chip.mk

# The two modules of the board, then the files of the chip
DE10_FILELIST=files.f
DE10_QSF_FILELIST=files.qsf
BOARD_SRCS=$(shell sed -e '/^[[:space:]]*$$/d' -e '/^[[:space:]]*\#/d' $(DE10_FILELIST))
VERILOG_SRCS=$(BOARD_SRCS) $(CHIP_SRCS)

.PHONY: all
all: build

# The design for the FPGA. This needs Quartus and takes minutes.
.PHONY: build
build: $(BUILD_DIR)/$(PROJECT).sof

# Quartus makes a file for the flash of the board next to the one for JTAG.
# Nothing here loads it, and it is deleted so that nobody does by mistake.
$(BUILD_DIR)/$(PROJECT).sof: .stamp-fit
	$(QUARTUS_ASM) $(PROJECT)
	rm -f $(BUILD_DIR)/$(PROJECT).pof
	@touch $@

.stamp-fit: .stamp-map
	$(QUARTUS_FIT) $(PROJECT)
	$(QUARTUS_STA) $(PROJECT)
	@touch $@

.stamp-map: $(VERILOG_SRCS) $(DE10_FILELIST) $(DE10_QSF_FILELIST) $(PROJECT).qsf $(PROJECT).qpf $(PROJECT).sdc
	$(CHIP_CHECK)
	$(QUARTUS_MAP) $(PROJECT)
	@touch $@

# The list of the source files in the form that Quartus reads: those of
# `files.f`, then those of the chip
$(DE10_QSF_FILELIST): $(DE10_FILELIST) $(CHIP_INFO)
	$(CHIP_CHECK)
	@{ \
		printf "# Generated from \`files.f\` and the chip's info.yaml. Do not edit by hand.\n"; \
		for f in $(VERILOG_SRCS); do echo "set_global_assignment -name SYSTEMVERILOG_FILE $$f"; done; \
	} > $@

# Loads the design into the FPGA over JTAG. The design is gone when the
# board loses power. Nothing here writes the flash of the board.
.PHONY: upload
upload: $(BUILD_DIR)/$(PROJECT).sof
	$(QUARTUS_PGM) -c 'USB-Blaster [USB-0]' -m jtag -o "p;$(BUILD_DIR)/$(PROJECT).sof"

# Lint on a computer without Quartus
.PHONY: lint-local
lint-local:
	$(CHIP_CHECK)
	$(VERILATOR) --lint-only --timing --timescale 1ns/1ps -Wall --top-module $(PROJECT) tb/altpll.sv $(VERILOG_SRCS)

# The board's top module against a model of the Pico
.PHONY: test
test: $(BUILD_DIR)/tb/Vasc2_tb
	$(BUILD_DIR)/tb/Vasc2_tb

$(BUILD_DIR)/tb/Vasc2_tb: tb/asc2_tb.sv tb/delay_watch.sv tb/altpll.sv $(VERILOG_SRCS) $(DE10_FILELIST)
	$(CHIP_CHECK)
	@mkdir -p $(BUILD_DIR)
	$(VERILATOR) --binary --timing --timescale 1ns/1ps -Wall -j 4 --top-module asc2_tb -Mdir $(BUILD_DIR)/tb \
	  tb/asc2_tb.sv tb/delay_watch.sv tb/altpll.sv $(VERILOG_SRCS) > $(BUILD_DIR)/tb.log 2>&1 || { tail -20 $(BUILD_DIR)/tb.log; exit 1; }

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) db incremental_db
	rm -f *.rpt *.summary *.done *.smsg *.qws *.jdi *.pin *.sld
	rm -f .stamp-* $(DE10_QSF_FILELIST)

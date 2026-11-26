# Where the chip of Almurut II is. Its hardware description lives in its
# own repository, https://github.com/toksaitov/ttsky26e-almurut2, and the
# builds here read it from a clone of that repository beside this one:
#
#   asc2-project/          this repository
#   ttsky26e-almurut2/     the chip: info.yaml, src/, docs/, test/
#
# A clone in another place is named on the command line with an absolute
# path, as `make CHIP_DIR=/path/to/ttsky26e-almurut2`. Each Makefile that
# builds the chip sets ROOT, the path from its own directory to the root of
# this repository, and then includes this file.

CHIP_DIR ?= $(ROOT)/../ttsky26e-almurut2

# The source files of the chip, in the order of its info.yaml, the top
# module first
CHIP_INFO = $(CHIP_DIR)/info.yaml
CHIP_SRCS = $(addprefix $(CHIP_DIR)/src/,$(shell sed -n 's/^ *- "\(.*\.sv\)"$$/\1/p' $(CHIP_INFO) 2>/dev/null))

# The bus unit of the chip with the filters of its inputs, for the tests of
# the bus alone
CHIP_BUS_SRCS = $(addprefix $(CHIP_DIR)/src/,asc_reset_filter.sv asc_irq_filter.sv asc_bus.sv)

# The first line of every recipe that needs the chip
CHIP_CHECK = @test -f $(CHIP_INFO) || { echo "The chip is not in $(CHIP_DIR): clone https://github.com/toksaitov/ttsky26e-almurut2 beside this repository, or name its place with CHIP_DIR=<absolute path>"; exit 1; }

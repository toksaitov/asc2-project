Almurut II Bus Memory Test
==========================

This directory contains the memory test of the bus on the bench: a Raspberry Pi Pico serves memory over the 8-bit bus of [../../asc2-kb/bus-protocol-v1.md](../../asc2-kb/bus-protocol-v1.md), a DE10-Lite plays the chip and checks every answer, and a script on the Mac drives the two boards. The DE10-Lite runs the chip's bus unit and input filters ([asc_bus.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_bus.sv), [asc_reset_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_reset_filter.sv), and [asc_irq_filter.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_irq_filter.sv) in the [src/](https://github.com/toksaitov/ttsky26e-almurut2/tree/main/src) of the chip's repository, [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2)) behind pin delays that stand in for the Tiny Tapeout pads, with a test sequencer in the place of the CPU. The test ran for four hours on the bench without an error.

## Layout

* [firmware/](firmware/): the firmware of the Pico. [mem.c](firmware/mem.c) serves RAM, a 16 KB ROM from a RAM copy, device registers, and a disk in flash sectors at a system clock of 150 MHz. Core 1 answers the bus from code in RAM and does nothing else; core 0 takes line commands over USB, raises interrupts at random times, and does the slow flash work. [rpbus.pio](firmware/rpbus.pio) is the byte handshake. The header comment of [mem.c](firmware/mem.c) lists the pins and the memory map.
* [de10-lite/](de10-lite/): the Quartus project of the DE10-Lite. [bench_mem.sv](de10-lite/bench_mem.sv) is the top module: the bus unit, the reset and interrupt filters, the pin delays of [dly_line.sv](de10-lite/dly_line.sv), and the sequencer. It is controlled and read through three In-System Sources and Probes instances, `T4A`, `T4B`, and `T4C`; its header comment lists their bits. [lint/](de10-lite/lint) holds stubs of the Altera blocks for Verilator's lint, and [tb/](de10-lite/tb) the test bench with simulation models of the same blocks.
* [host/](host/): [memtest.py](host/memtest.py), the test, and [bench_link.py](host/bench_link.py), the links to the two boards: the Pico over its USB serial port, the DE10-Lite through a `quartus_stp` session on the ssh host `quartus`. Each run writes its results to `host/results/` as JSON.

## Building

The firmware needs the Pico SDK and the Arm GCC toolchain:

```bash
cd firmware
cmake -S . -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk
make -C build
```

This makes `build/bench_mem.uf2`, which goes onto the Pico in its BOOTSEL mode. The build stops on any warning.

The DE10-Lite design is built on the host `quartus` from copies of this repository and of the chip's, side by side as [../../chip.mk](../../chip.mk) expects them, since the project names the bus unit by a relative path ([../../asc2-targets/de10-lite/Readme.md](../../asc2-targets/de10-lite/Readme.md) shows how to copy both):

```bash
cd de10-lite
make build
make upload
```

`make upload` loads `output_files/bench_mem.sof` into the FPGA over JTAG. The design is loaded as a `.sof` file only; nothing here writes the configuration flash of the board, and there is no `.pof` target.

## Simulation

```bash
cd de10-lite
make lint-local
make sim
```

`make lint-local` runs Verilator's lint with `-Wall` on the design. `make sim` runs the whole design against a model of the Pico with the same memory map: RAM, the ROM window, device registers, a disk, a console stream that the design must echo, interrupts at random times, and faults put into the bus on purpose. The model checks that the two sides never drive the bus together. The run takes about 20 seconds and ends with `PASS`. It needs Verilator 5.

## Running the Test

With the Pico running the firmware and the DE10-Lite running the design, the script runs one or more phases:

```bash
python3 -I host/memtest.py prep manual matrix speed long25
python3 -I host/memtest.py soak --hours 4
python3 -I host/memtest.py faults torture tear
```

The phases are listed in the header comment of [memtest.py](host/memtest.py), and `--help` lists the options. `prep` always runs first and fills the ROM. At the end, and on any error, the chip side is held in reset and every pin of the Pico goes back to an input.

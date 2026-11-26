Almurut II Targets
==================

This directory groups the places where the chip of Almurut II runs. The chip itself, its hardware description, has its own repository, [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2), and the targets read it from a clone beside this repository, as [../chip.mk](../chip.mk) sets out.

* [tiny-tapeout/](tiny-tapeout): the checks of the chip for a Tiny Tapeout shuttle: lint, a test of the pins, the start on the demo board, the layout run, and the tests on its gates.
* [de10-lite/](de10-lite): the chip in the MAX 10 FPGA of a Terasic DE10-Lite board, beside a Raspberry Pi Pico that is its bridge. It runs the whole computer on the bench.

These directories hold the top modules of the boards, the tests, the Makefiles, and the project files of each target. See [../Readme.md](../Readme.md) for the repository-wide overview.

Almurut II Pictures
===================

This directory contains the pictures of the platform's [Readme](../Readme.md) and the scripts that make them. Every picture can be made again from the sources: the diagrams from a script, and the animated pictures of the showcase from a run of `tour` on the emulator.

## Layout

* [tools/diagrams.py](tools/diagrams.py): draws the diagrams as SVG files, each for GitHub's light theme and for its dark theme (`system`, `accelerator`, `instruction`, `gain`, `cells`, `platform`, `bus`, and `emulator`)
* [tools/film.py](tools/film.py): turns the frames of a run on the emulator into an animated GIF or a PNG
* [emulator.txt](emulator.txt): the left part of the emulator's screen after `ls` at the shell, as text; [emulator-light.svg](emulator-light.svg) and [emulator-dark.svg](emulator-dark.svg) are drawn from its top 13 rows
* [chip.png](chip.png): the layout of the chip, from the picture that the build of [ttsky26e-almurut2](https://github.com/toksaitov/ttsky26e-almurut2) on GitHub makes of the layout
* [chip-glide.gif](chip-glide.gif): wires of the chip in 3D, recorded by hand in the [Tiny Tapeout Explorer](https://gds-explorer.tinytapeout.com/viewer?file=https://toksaitov.github.io/ttsky26e-almurut2/tinytapeout.oas&pdk=sky130A), which opens the layout that the build of the chip publishes
* The other `*.gif`: the screen of the machine while the kernel runs `tour` with nobody at the button, a picture for each program of the tour, and [title.gif](title.gif), the last scene of `film`
* [Makefile](Makefile): makes the diagrams and the animated pictures

## Making the Pictures

```bash
make
```

This needs Python 3 with Pillow, and the kernel with its disk and the tools of the platform (`make` in [../asc2-tests/](../asc2-tests/) builds them). `make diagrams` draws the diagrams alone. `make films` runs `tour` on the emulator without a screen ([../asc2-tools/reftrace/](../asc2-tools/reftrace/)), which writes every frame that reaches the screen of the machine to `build/tour.film` with the time of the bridge's clock, and cuts the animated pictures out of it. The seconds of each picture are in the [Makefile](Makefile).

A picture plays at the speed of the bridge's clock in that run: for each of its 20 steps in a second it shows the frame that the screen held at that moment. The numbers of the chart `gain` are the times that the board measured ([../asc2-kb/array-design.md](../asc2-kb/array-design.md)), and those of `cells` are the cells that the build of the chip counted, with its kinds of gates added up.

Every diagram is 680 units wide with no text smaller than 20 units, so it still reads on a phone. The script fixes the width of every text from the widths of Arial's letters, so a text takes the same room with any font, and it stops if a text crosses the edge of a box, a line, or the drawing.

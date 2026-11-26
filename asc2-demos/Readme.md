asc2-demos
==========

Programs that show the screen and the push button of the machine, written in C for the compiler of the project. Each is built in two ways from the same source: as a program of the kernel, which [../asc2-rach2/](../asc2-rach2/) puts on its disk as `burkut` and `scenes` and the shell starts, and as a program of the bare machine, as the firmware of [../asc2-firmware/](../asc2-firmware/) is. Under the kernel a program asks for the screen first (`scr_open` of [lib/machine.h](lib/machine.h)), and the console has the display again when it ends. The screen and the button are devices of the bridge, as every device of the machine is (see [../asc2-kb/screen-and-button.md](../asc2-kb/screen-and-button.md)). The overview of the whole platform is in [../Readme.md](../Readme.md).

## Layout

* [lib/](lib/): the start code, the console, and [machine.h](lib/machine.h) with the registers of the devices and a few functions for text, rectangles, and pictures
* [pictures.py](pictures.py): draws the pictures of the programs and writes them as [pictures.h](pictures.h)
* [scenes.c](scenes.c): three scenes, of text, of a picture, and of a short film; [scenes.expect](scenes.expect) and [scenes.frame](scenes.frame) are what the console prints and what the screen shows at the end
* [speed.c](speed.c): measures how fast the machine draws and prints the numbers
* [burkut.c](burkut.c): a game for the one button
* [pilot.py](pilot.py): flies the game on the emulator and writes the presses as [burkut.events](burkut.events); [burkut.expect](burkut.expect) is what the console prints with them
* [lib/show.c](lib/show.c): what the programs of the showcase share: the title card, the bars, the large digits, the one rule for the button (a press goes on, a hold of two seconds ends), and the way a program shows itself when `tour` starts it
* [race.c](race.c), [pilot.c](pilot.c) (the game with a neuron at the button), [guess.c](guess.c), [fence.c](fence.c), [scope.c](scope.c), [sched.c](sched.c), [shapes.c](shapes.c), [flight.c](flight.c), [film.c](film.c), [mark.c](mark.c), [tour.c](tour.c): the showcase, programs of the kernel only (see The Showcase)
* `*_asm.s`: the short loops of those programs, in assembly: a loop of at most 25 instructions runs out of the cache of the chip
* [shapes_models.h](shapes_models.h), [flight_land.h](flight_land.h), [film_pictures.h](film_pictures.h): the models, the land, and the pictures, written by the programs of [../asc2-kb/models/](../asc2-kb/models/)

## Building and Checking

```bash
make
make check
```

`make` needs the assembler, the C compiler, and the tools of the project (`make` in [../asc2-tests/](../asc2-tests/) builds them). `make check` runs the scenes on the emulator and compares the console and the screen with the two files. Then it runs the game with the presses of [burkut.events](burkut.events): the burkut must pass 12 pillars and fall. `make pilot` flies the game anew after a change of [burkut.c](burkut.c). The trace tool writes every frame of a run to a file with `-m`, one line for a frame. `make board-screen` in [../asc2-tests/](../asc2-tests/) does the same on the board and compares the frame that the Pico has sent to the display.

## Running on the Board

```bash
make run-burkut
```

This loads the program into the bridge and stays as its terminal; Ctrl-] leaves. `run-scenes` and `run-speed` run the other two.

## The Showcase

The programs that show what the machine can do; [../asc2-rach2/](../asc2-rach2/) puts them on the disk of the machine, and the shell starts each by its name. `tour` starts them all, one after another, each with a card that says what is to be seen. The comment at the top of each program tells what it shows and how it is made. `make check` in [../asc2-tests/](../asc2-tests/) runs them on the emulator (the sessions `showcase` and `tour`).

## Burkut

A burkut, a golden eagle, flies between pillars of rock. A press of the button is a beat of the wings; without one the burkut falls. Every pillar passed is a point, and a touch of a pillar or of the ground ends the flight. The next press starts a new one. The game draws 30 frames in a second and prints on the console the score of each flight and how long a frame took. Without a button at hand, the monitor of the bridge plays a press: `press 60` in another terminal with `bridge.py cmd`.

## What the Board Measured

`speed` on the bench (the chip in the FPGA, the RP2040 at 150 MHz), at the chip's clock of 12.5 MHz and of 25 MHz:

| In a second                                   | 12.5 MHz | 25 MHz |
|-----------------------------------------------|---------:|-------:|
| Turns of an empty loop                        |    9,550 | 12,368 |
| Words written to the frame memory             |    5,102 |  6,566 |
| Characters drawn                              |    3,769 |  4,822 |
| Rectangles of 16 by 16 pixels                 |    1,169 |  1,505 |
| Pictures of 12 by 8 pixels                    |    1,369 |  1,757 |
| Pictures of 128 by 24 pixels                  |    1,338 |  1,682 |
| Frames shown with one word changed            |      219 |    222 |
| Frames of a film of pictures                  |      150 |    162 |
| Frames with all 168 characters drawn anew     |       20 |     25 |
| Frames with all 256 words written anew        |       18 |     23 |

The machine runs about 29,000 instructions in a second at 12.5 MHz, because every instruction is a talk with the RP2040 over the bus. So the screen is fast when the RP2040 does the drawing (a character, a rectangle, or a whole picture costs the program one write) and slow when the program sets the pixels itself. The display takes a frame in about 4.5 ms, which is the limit of 220 frames in a second.

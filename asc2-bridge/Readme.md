Almurut II Bridge
=================

This directory contains the bridge: the firmware of the microcontroller that stands next to the chip of Almurut II. The bridge is the chip's link to memory, storage, and every peripheral: it gives the chip the clock and the reset and answers every access that the CPU makes on the bus, with the RAM, the boot ROM, a console, a disk, a clock, a screen, and a button. On the bench it is the RP2040 of a Raspberry Pi Pico on jumper wires to a DE10-Lite with the chip in its FPGA ([../asc2-targets/de10-lite/](../asc2-targets/de10-lite/)). On the demo board of Tiny Tapeout (v3.2 or v3.3) it is the RP2350B of the board, and the traces of the board are the wires. The overview of the whole platform is in [../Readme.md](../Readme.md). [../asc2-kb/bus-protocol-v1.md](../asc2-kb/bus-protocol-v1.md) describes the bus, [../asc2-emulated/asc.h](../asc2-emulated/asc.h) defines the memory and the devices, and [../asc2-kb/bridge-design.md](../asc2-kb/bridge-design.md) records why the bridge is built as it is.

## Layout

* [firmware/bridge.c](firmware/bridge.c): the firmware. Core 1 serves the bus and does nothing else; core 0 has the USB serial port with the console and the monitor, the flash work of the disk, the keyboard that the bridge plays, and the watchdog. The header comment lists the pins and the commands of the monitor.
* [firmware/pins.h](firmware/pins.h): the pins of the two boards, the bench and the demo board.
* [firmware/boards/](firmware/boards/): the board file of the demo board for the Pico SDK.
* [firmware/devices.h](firmware/devices.h): the memory and the devices in plain C: what the bridge answers to a request. The firmware includes it, and so does its test on the Mac.
* [firmware/rpbus.pio](firmware/rpbus.pio): the handshake of one byte, for the PIO of the RP2040 or the RP2350.
* [firmware/test/](firmware/test/): the test of [devices.h](firmware/devices.h) against the emulator, and the test of the pins of the demo board.
* [host/bridge.py](host/bridge.py): a script for the Mac that loads a program through the monitor and is its terminal.
* [bench/](bench/): the memory test of the bus, with a firmware and an FPGA design of its own. It checks the wires and the protocol without a CPU. See [bench/Readme.md](bench/Readme.md).

## Building and Loading the Firmware

The build needs the Pico SDK and the Arm GCC toolchain:

```bash
cd firmware
cmake -S . -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk
make -C build
```

This makes `build/bridge.uf2`. The build stops on any warning. With `picotool` the file goes onto a Pico that is plugged in, without a press of its button:

```bash
picotool load -f -x build/bridge.uf2
```

Without `picotool`, hold the BOOTSEL button of the Pico while plugging it in and copy the file to the drive that appears. On the demo board the button is called BOOT.

### The Demo Board of Tiny Tapeout

```bash
cd firmware
cmake -S . -B build-tt -DPICO_SDK_PATH=$HOME/pico/pico-sdk -DPICO_BOARD=tt_demoboard_v3 -DTT_DESIGN=N
make -C build-tt
```

`N` is the address of the chip in the multiplexer of its shuttle, which the shuttle's data sheet gives after the closing. This firmware builds without warnings, and `make -C firmware/test` checks each of its pins against the netlists of both versions of the board; its first run is on the first demo board with the chip. What it does differently from the bench:

* It chooses the design at power-up: the pulses of the address, then the enable, with the reset held.
* It releases the reset with the clock stopped, waits 200 us, and then starts the clock. After that its pin of the reset is an input, so the reset button of the board works, and a press of it starts the bridge's side again too.
* The eight pins of the bus have pull-ups, and the line that tells that the bridge drives the bus has a pin that the board joins to nothing.
* The display and the button are wired to the analog header of the board, which has no pins soldered in: the button to GPIO41, SDA to GPIO42, SCL to GPIO43, RST to GPIO44, and D/C to GPIO45.

All switches of the board are off and its Pmod sockets are empty ([../asc2-kb/bus-protocol-v1.md](../asc2-kb/bus-protocol-v1.md), the rules for the devkit). A switch that is on ties an input of the chip to 3.3 V, and a Pmod board can drive against the bus.

## Running a Program

A program for the boot ROM is a firmware image, as the assembler and [../asc2-tools/elf2fw/](../asc2-tools/elf2fw/) make them. With the firmware on the Pico and the design in the FPGA:

```bash
python3 -I host/bridge.py run ../asc2-firmware/hello
```

The script hands the image to the monitor, starts the chip, and becomes the terminal of the program. `ctrl+]` leaves it. Other forms:

```bash
python3 -I host/bridge.py run IMAGE --for 2
python3 -I host/bridge.py check IMAGE EXPECT
python3 -I host/bridge.py cmd stat
python3 -I host/bridge.py term
python3 -I host/bridge.py disk put DISK_IMAGE
python3 -I host/bridge.py disk get DISK_IMAGE
```

* `run IMAGE --for 2` shows what the program prints in two seconds and ends. `--type TEXT` types a text to the program, and `--clock N` sets the clock of the chip first.
* `check IMAGE EXPECT` runs the program and compares what it prints with the file `EXPECT`. It prints one line, `passed` or `FAILED`, with the number of bus transactions and of those that broke off. `--input FILE` types the bytes of a file to the program, and `--for S` sets the longest time of the run in seconds. `--steps TRACE` first runs the instructions of a trace of the emulator one at a time and compares each with the trace, and `--keys "1c f0 1c"` sends bytes as a keyboard does; [../asc2-tests/Makefile](../asc2-tests/Makefile) uses both.
* `cmd LINE` gives the monitor one line and prints its answer. `cmd stat` prints the counters of the bus, the console, and the disk; `disk_errors` counts the disk commands that the bridge refused, and the kernel makes one at every start, when it looks for the size of the disk by reading past its end.
* `term` is the terminal of the program that runs.
* `disk put` writes a disk image into the flash of the Pico, and `disk get` reads the disk into a file. The image has the size of the disk, 256 KB.

The script needs Python 3. It finds the serial port by itself when one Pico is plugged in; `--port` names it otherwise. The disk commands also need `picotool`: the monitor tells where the disk is in the flash, and `picotool` restarts the Pico into its boot loader, moves the bytes, and starts the firmware again. The program that was loaded is gone after that.

## Running the System

The kernel of [../asc2-rach2/](../asc2-rach2/) needs its disk in the flash of the Pico. `make` there builds the kernel and the disk image:

```bash
python3 -I host/bridge.py disk put ../asc2-rach2/build/disk.img
python3 -I host/bridge.py run ../asc2-rach2/build/rach2.bin
```

The shell answers after about three seconds, and a command takes about two: the Pico answers about 90,000 requests of the chip a second. The disk keeps the files of a session, so the first command is needed again only for a new disk.

The Pico can keep the kernel as well, and then no script is needed:

```bash
python3 -I host/bridge.py save ../asc2-rach2/build/rach2.bin
```

`save` loads the image and tells the monitor to write it into the flash, below the disk. The Pico runs a kept image at every power-up (`forget` in the monitor removes it), and `make install` in [../asc2-rach2/](../asc2-rach2/) does both steps. The display shows the console of the chip while no program draws on it.

Any serial terminal works too. What is typed goes to the console of the chip. `ctrl+a` starts one line for the monitor, which answers with a line that begins with `ok` or `err`.

## Stopping and Stepping

The monitor can hold the CPU between two instructions and let it run them one at a time. This is the start of the smoke program:

```
bridge> run stopped
ok run
bridge> step 2
fetch 800000: 00000990
fetch 800004: 80020991
ok step 2
bridge> go
ok go
```

* `run stopped` resets the chip and holds its CPU before the first instruction. `stop` holds a CPU that runs, after the instruction that it is in.
* `step` runs one instruction, and `step N` runs N of them, 256 at most. Every line is a request that the CPU has sent: a `fetch` with the address and the instruction, a `read` or a `write` with the address and the value. The addresses are physical. The step that takes the first letter of the greeting shows `read 001000: 48` after its fetch.
* `go` lets the CPU run on.

`regs` reads the debug port of the chip and answers with one line: `ip`, the instruction that ran last (`inst`), the cause of the last trap (`cr5`), `flags`, `cr0`, the state of the CPU, whether it is halted or has stopped after a double fault, the status of the last transaction, the state of the bus unit, `at_rest`, which is 1 between two instructions, and then every register: `r0` to `r7`, `fp`, `ksp` (the kernel's `sp`), `lr`, `usp` (the user's `sp`), and `cr1` to `cr4`. The values are in hex, and the header comment of [src/asc_soc.sv](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/src/asc_soc.sv) in the chip's repository describes the fields. After a step `inst` is the instruction that has run and `ip` the address of the next one. The bits belong to one moment only while the CPU is stopped; `stopped` in the answer says whether it is. The port does not show memory; the bridge holds the memory itself. It needs three wires, from GP18, GP19, and GP20 of the Pico to pins 1, 3, and 5 of the board's header; without them `regs` says that the port shows only zeros. A kernel can lock the port (`cr8`): `regs` then answers `locked=1` and shows no register until the chip gets a reset, and the stepper writes on the display that the port is locked. `cr0` in the answer has all six of its bits; the port carries the two upper ones at its end.

A step itself shows what the CPU asks of the bridge. The timer of the chip counts on while the CPU is held, so a program that allows interrupts often takes the timer's interrupt at a step. A CPU that is halted and waits for an interrupt fetches nothing, and `step` says so. [../asc2-kb/bridge-design.md](../asc2-kb/bridge-design.md) tells how the bridge makes a step.

## The Keyboard

The chip has a receiver for a PS/2 keyboard, and the bridge plays the keyboard on two pins. `key` sends the bytes that follow it, in hex:

```
bridge> key 1c f0 1c
ok key 3
```

These three are a press and a release of the key A. On the bench the two pins need two wires that the bus does not need, from GP16 and GP17 of the Pico to pins 6 and 26 of the header of the DE10-Lite. Without them the chip sees a keyboard that sends nothing.

## A Display and a Button

The Pico serves a display of 128 by 64 pixels (an SSD1306 or an SH1106) and a push button as devices of the machine. A program draws into a frame memory or asks the Pico to draw characters, rectangles, and pictures, shows the frame with one write, reads the button, and can sleep until a press. They are devices of the bridge, as every device of the machine is, so the chip needs no pins for them. [../asc2-kb/screen-and-button.md](../asc2-kb/screen-and-button.md) lists the registers, and [../asc2-demos/](../asc2-demos/) has programs that use them.

The display has four or six wires: its VCC to the 3V3 pin of the Pico (pin 36), its GND to a ground pin (pin 38), SDA to GP26 (pin 31), SCL to GP27 (pin 32), and on a display with six pins RST to GP21 (pin 27) and D/C to GP28 (pin 34). The button joins GP22 (pin 29) to the ground pin next to it (pin 28).

The monitor has commands to check both without a program:

```
bridge> oled Hello
ok oled at 0x3c
bridge> button
ok button down=0 presses=2
```

* `oled` starts the display and shows the text that follows; without a text it draws a border around the screen. It works while the chip is held in reset. From then on the last row shows the button: `up` or `down`, and the number of presses.
* `button` answers with the same two numbers.
* `press` plays a press of the button for 100 ms, and `press N` for N ms, as if a finger did it. The tests use it.
* `frame` prints the frame that went to the display last, as 64 rows of `#` and `.`, and the number of frames. `bridge.py check --frame FILE` compares it with a file.
* `shift N` moves the columns of the picture by N. A display with an SH1106 needs 2, which the firmware starts with since the display of the bench is one (its left two columns were off the glass with 0); a display with an SSD1306 needs `shift 0`.
* `stepper on` makes the button a debugger: the CPU stops, each press runs one instruction, and the display shows the address of the next instruction, the instruction that ran, the flags (a letter for each that is set, of `CZSOIU`), the cause of the last trap, and the count of steps. It needs the three wires of the debug port. `stepper off` gives the button back to the program, and `go` also lets the CPU run on.

A display with six pins is built for one of two ways to talk, and its board does not always say which. The firmware resets the display and asks for it on I2C at both addresses. If no display answers, it sends the same bytes with SCL as a clock and SDA as a data line, which is the other way; a display cannot answer that way, so `oled` then says to look at the display:

```
ok oled sent over the clock and the data line; no display answered at 0x3c or 0x3d, and a display cannot answer this way, so look at it
```

## What the Bridge Gives the Chip

* RAM: 192 KB. It keeps its bytes over a reset of the chip.
* The boot ROM: the text section of the image that the monitor has loaded. At every reset of the chip the data section of the image is copied to its place in RAM.
* The console: the USB serial port. The bridge keeps up to a kilobyte of typed bytes for a program that does not read yet, so the lines of a session can be typed ahead or sent at once. A longer text arrives whole if the program reads: the bridge then reads the port only as fast as the program takes its input. A program that takes nothing for half a second with the kilobyte full loses the bytes that come, so that the monitor can always be reached.
* The disk: 64 sectors of 4 KB in the last 256 KB of the flash of the Pico. They keep their bytes without power.
* The clock of the chip: the system clock of 150 MHz divided by an even number, 12.5 MHz after power-up.
* The reset: 32 clocks long, by rule 5 of the bus.
* A watchdog: when a transaction has broken off and the chip sends nothing for a second, the bridge resets the chip and says so on the console.
* STOP, the pin that holds the CPU between two instructions, and the two lines of a keyboard.
* A screen of 128 by 64 pixels with a frame memory and drawing of characters, rectangles, and pictures, and a push button with an interrupt.

On an RP2040 an image can have 44 KB: the chip's 192 KB of RAM, the frame memory of the screen, and the image share the 264 KB of the RP2040. On the RP2350 of the demo board the limit is 272 KB, and about 28 KB of its 520 KB stay free.

## Checking

```bash
make -C firmware/test
```

This builds [devices.h](firmware/devices.h) for the Mac next to the emulator's model of the RP2040 and gives both the same four million random requests: reads, writes, and fetches of every size at every kind of address, in kernel and in user mode, with typing on the console, disk commands, and resets between them. Every answer, every value read, the interrupt line, the console output, and at the end all of RAM and the disk must be equal. It needs a C compiler and no Pico. The same command checks the pins of the demo board: the chip's [info.yaml](https://github.com/toksaitov/ttsky26e-almurut2/blob/main/info.yaml), from its clone beside this repository ([../chip.mk](../chip.mk)), says what each pin of the chip is, the netlists of the two versions of the board ([../asc2-kb/tt-devboard-netlists.txt](../asc2-kb/tt-devboard-netlists.txt)) say which GPIO the board joins it to, and [firmware/pins.h](firmware/pins.h) must have that number.

The firmware as a whole is checked on the bench: `make board-suite` in [../asc2-tests/](../asc2-tests/) runs the smoke program, the C programs, and the sessions of the kernel on the board and compares what each prints with what it prints on the emulator. It also steps through the start of one program and compares every instruction with the emulator's. `make board-keys` there checks the keyboard and needs its two wires. `make board-button` and `make board-screen` check the button and the screen: the first plays two presses, and the second compares the frame that went to the display with the emulator's.

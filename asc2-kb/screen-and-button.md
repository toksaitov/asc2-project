The Screen and the Push Button (2026-11-24)
===========================================

A display and a push button are added without a change of the chip, with changes only to the programs and to the firmware of the RP2040 (2026-11-24). This note says what was built, why it needs nothing from the chip, and what the board measured.

## Why the Chip Does Not Change

Every read and write of a program is a request to the bridge, the RP2040, and the bridge decides what an address means. A new device is a few addresses that the RP2040 answers differently, and the interrupt line that the chip already has for the devices of the RP2040 (line 2 of its controller). The display and the button hang on free pins of the Pico; no wire of them goes to the chip.

So the hardware description, the layout, and the pins of the chip are as they were. Changed: the firmware ([asc2-bridge/firmware/devices.h](../asc2-bridge/firmware/devices.h), [bridge.c](../asc2-bridge/firmware/bridge.c)), the emulator's model of the RP2040 ([asc2-emulated/asc.h](../asc2-emulated/asc.h)), the test tools, and new programs.

## The Devices

All registers are 32 bits wide, at physical addresses from `0xff0000`, for the kernel only, as the other devices of the RP2040.

| Address    | Name            | Meaning                                                                 |
|------------|-----------------|-------------------------------------------------------------------------|
| `0xff0034` | `RP_BTN_STAT`   | bit 0 is 1 while the button is down                                     |
| `0xff0038` | `RP_BTN_COUNT`  | presses since the reset                                                 |
| `0xff003c` | `RP_SCR_STAT`   | bit 0 is 1 while a frame is on its way to the display                   |
| `0xff0040` | `RP_SCR_CMD`    | write 1 to show the frame memory, 2 to clear it                         |
| `0xff0044` | `RP_SCR_FRAMES` | frames shown since the reset                                            |
| `0xff0048` | `RP_SCR_CHAR`   | write row << 16, column << 8, and the code to draw a character          |
| `0xff004c` | `RP_SCR_RECT`   | write x, y, width - 1, height - 1, and a mode to draw a rectangle       |
| `0xff0050` | `RP_SCR_SRC`    | the physical address of a picture in RAM                                |
| `0xff0054` | `RP_SCR_BLIT`   | write x, y (both signed), and a mode to draw that picture               |
| `0xff1000` | `RP_SCR_BASE`   | the frame memory: 1024 bytes, 16 for each of the 64 rows                |

* The screen has 128 by 64 pixels, one bit each. Pixel x of row y is bit x mod 8 of the byte y * 16 + x / 8 of the frame memory. Text has 8 rows of 21 characters, 5 by 7 pixels in a cell of 6 by 8.
* A program draws into the frame memory, by itself or through the three drawing registers, and then writes 1 to `RP_SCR_CMD`. The RP2040 copies the frame memory and sends the copy to the display while the program goes on. A show that comes while the last one is on its way is dropped; a program that needs every frame waits for `RP_SCR_STAT` to be 0.
* The modes of a rectangle and of a picture: 0 makes the pixels dark, 1 light, 2 flips them, and 3 (pictures only) copies the picture with its dark pixels. A picture is a word with the width and the height << 16, and then the rows, each in ceil(width / 32) words with the leftmost pixel in bit 0. A picture may lie partly outside the screen.
* The button raises the interrupt of the RP2040 at each press: flag 4 of `RP_IRQ_FLAGS`, cleared by writing 4 to it. The firmware takes a level as true after it has held for 500 microseconds and takes no other change for 20 ms, so the bounce of the contact is one press.

The emulator's model and the firmware are the same code in two places, and `make -C asc2-bridge/firmware/test` gives both four million random requests, with presses and pictures among them, and compares every answer.

## The Display on the Bench

The display of the bench has six pins (GND, VCC, SCL, SDA, RST, D/C) and does not answer on I2C. The firmware then sends the bytes with SCL as a clock and SDA as a data line, about 3.5 million bits in a second, with the pins handled one by one (no free hardware unit fits these pins). A frame of 1024 bytes takes about 4.5 ms.

A display cannot answer that way, so the firmware cannot know that the glass shows the frame. The tests compare the frame that the firmware sent; the glass is checked by eye. Displays with an SH1106 need their columns moved by 2; `shift 2` in the monitor does that.

## What the Board Measured

The table is in [../asc2-demos/Readme.md](../asc2-demos/Readme.md). In short, at 12.5 MHz: 3,769 characters, 1,169 rectangles of 16 by 16, or 1,338 pictures of 128 by 24 in a second; 219 frames in a second when little changes, 150 for a film of whole pictures, 20 when the program draws all the text anew, and 18 when it writes every pixel itself. At 25 MHz the numbers are about a quarter higher: most of the time of a request is the RP2040's, not the chip's.

The lesson for the system: the machine runs about 29,000 instructions in a second, so drawing belongs to the RP2040 and the program says what to draw. With that the game `burkut` needs 12 ms for a frame and runs at 30 frames in a second with time to spare.

## Uses of the Button

* For programs: a key that does not need a computer. The game uses it, and a program can sleep until a press ([../asc2-tests/board/button.s](../asc2-tests/board/button.s)).
* For the bench, with no program at all: the stepper. `stepper on` in the monitor stops the CPU; from then on each press runs one instruction, and the display shows the address of the next instruction, the instruction that ran, the flags, the cause of the last trap, and the count of steps, read from the debug port of the chip. With the real chip this is a debugger that needs only the board: no computer and no terminal. `go` or `stepper off` ends it.

## The Console on the Display

After a reset the screen belongs to the console. The RP2040 draws every byte that the CPU prints into the frame memory as a terminal does: 8 rows of 21 characters, a new row at a line feed and at the end of a row, the rows move up when the last one is full, a backspace steps back, a `_` marks the cursor, and escape sequences are left out. The bridge sends the frame memory to the display by itself when it has changed, at most 25 times a second. So the shell of the kernel is on the display with no code in the kernel.

The first write of a program that draws (to the frame memory or to a register that draws, shows, or clears) takes the screen from the console, and the frame memory starts dark. The command 3 of the screen's command register, which only the kernel's side takes, gives the screen back: the frame memory is cleared, and the console goes on at the top.

## The Screen for a User Program

The page at `0xff1000` is the one device page that the RP2040 lets a user program use. It holds the frame memory and, from `0xff1400` on, copies of the registers that a program needs: the status, the commands "show" and "clear", the count of frames, the character, the rectangle, the picture, the level and the count of the button, and the clock. Nothing in the page reaches the memory of another program.

A picture is drawn out of RAM, and a user program does not know where its pages are in RAM. So the kernel's side has four pairs of registers from `0xff0058` on, one pair for a picture: the address in RAM of its start, and the address of the page where it goes on (a picture has at most 1028 bytes, so it lies in one page or two, and the two need not be next to each other). The picture register of the user page takes the number of the pair in its bits 19:18. A pair that is not set draws nothing.

The kernel lends the screen with one system call ([../asc2-rach2/sys/screen.h](../asc2-rach2/sys/screen.h)): `screen(SCREEN_OPEN)` puts the page into the memory of the process at `0x7ef000`, `screen(SCREEN_PICTURE, number, address)` names a picture, and `screen(SCREEN_CLOSE)` gives the screen back. One process has the screen at a time; a child does not get it at `fork`; the kernel takes it back when the process ends, is stopped for a mistake, or becomes another program, and then the console is on the display again. After the loan a program draws with no further call of the kernel, so a frame of the game takes 13 to 14 ms under the kernel on the bench, against 12 on the bare machine.

`burkut` and `scenes` of [../asc2-demos/](../asc2-demos/) are on the disk of the kernel and start from the shell. The game ends when the button is held for two seconds.

## Booting Without a Computer

`save` in the monitor of the bridge writes the loaded image into the flash of the Pico, below the disk, and the Pico runs it at every power-up (`forget` removes it). `make install` in [../asc2-rach2/](../asc2-rach2/) puts the disk and the kernel there. From then on the machine needs no script: any serial terminal on the port of the Pico is the console, and the display shows it too. The FPGA board of the bench forgets its design at power-off, so there the design is loaded first; the chip starts as soon as it is there.

## What Was Not Done

* The emulator with the terminal window (`asc`) does not draw the screen; the trace tool writes the last frame to a file (`asc_reftrace -f`).
* The firmware fills the RAM of the RP2040 (about 170 bytes are free): an image can have 44 KB now, and the queues of printed and typed bytes have 512 bytes each. The RP2350 of the newer boards has twice the RAM.

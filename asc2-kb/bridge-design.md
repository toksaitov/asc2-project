The Design of the Bridge (2026-11-22)
=====================================

This note records how the firmware of the RP2040 and the board target of Almurut II are built and why. It is the outline of phase 7 of the plan of milestone 1, written before the code and corrected afterwards where the code came out differently. The rules of the bus are in [bus-protocol-v1.md](bus-protocol-v1.md), and the memory and the devices that the firmware serves are defined by the emulator, [asc2-emulated/asc.h](../asc2-emulated/asc.h) (the functions `rp_access` and `rp_access_reg`). The note repeats neither.

## What Is There Already

The bench of Bus Test 4 (`asc1/bench/`): a firmware for the Pico that serves memory over the bus (`pico-mem/mem.c`, 1,400 lines, with the byte handshake in PIO), a design for the DE10-Lite that plays the chip and checks every answer (`de10-mem/bench_mem.sv`), and a script for the Mac that runs the two ([host/memtest.py](../asc2-bridge/bench/host/memtest.py)). It ran for four hours without an error. Its firmware already has most of a bridge: RAM, a small ROM, device registers, a disk in flash sectors, an interrupt line, the recovery rules of the protocol, and line commands over USB.

## The Directories

    asc2-bridge/
        firmware/    the firmware of the RP2040: the bridge itself
        host/        a script for the Mac that talks to the firmware
        bench/       the memory test of the bus: its firmware, its DE10-Lite design, its script
    asc2-targets/de10-lite/    the chip in the DE10-Lite, wired to the Pico

The memory test moves as it is, with its own firmware, because it is a test of the wires and of the protocol against a design that is not the chip. The bridge is a second firmware made from the same bus server.

## The Firmware

Two cores, as on the bench.

* Core 1 serves the bus and does nothing else. It runs from RAM and never touches the flash, so a transaction never waits for a flash operation. Its loop is the loop of the bench firmware: take a header, take the address and the data, decide, answer, with the kept answer for a repeated request and the recovery when the chip goes quiet.
* Core 0 does everything slow: the USB serial port, the monitor, the disk's flash work, and the keyboard player.

What changes against the bench firmware:

* The decision about an access follows `rp_access` of the emulator line for line: RAM, ROM, device registers, and the same answer codes. A device register is refused to user mode and to a fetch. The bench's own rules go (a kernel-only top of RAM, a "next" address for data). This part is a file of its own, [devices.h](../asc2-bridge/firmware/devices.h), in plain C, so that a test on the Mac can give it and the emulator the same requests.
* The device registers are the ones of the emulator: identity, scratch, RAM size, time, console status and data, interrupt status and enable, the four disk registers, and the test register that loses an answer.
* The console is the USB serial port. Two rings of bytes join the cores: core 0 puts typed bytes into one and prints from the other, and core 1 gives and takes bytes at the chip's requests. A ring has one writer and one reader, so it needs no lock. Core 0 keeps a kilobyte of typed bytes of its own in front of that ring, for a program that does not read yet: the kernel takes a line only when a process asks for one, and a script types a whole session at once. The key of the monitor is taken out as it arrives, so the monitor does not wait behind these bytes. When the kilobyte is full, core 0 reads no more from the port, so a long pasted text arrives whole. If it stays full for half a second, the program does not take its input, and the bytes that come are dropped: the key of the monitor may be among them. The first form had no such limit, and a program that had stopped reading made the monitor unreachable. The second form kept one byte, and a session lost the lines that were typed while a command ran.
* The interrupt line is a level: high while a device that is allowed to interrupt has something to say. Core 1 alone drives the pin. It sets the level after every access and while it waits for the next request, so a byte typed while the CPU sleeps wakes it.
* The ROM is the text section of a firmware image that the monitor loads into the Pico's RAM. The range is 256 KB as in the emulator, and the bytes after the image read as zeros and take no memory. The data section of the image is copied to its place in RAM at every reset of the chip, as the emulator does; the rest of RAM keeps its bytes. On the RP2040 of the bench, with 264 KB in all, an image can have 48 KB next to the 192 KB of RAM. The kernel of phase 4 has 25 KB; see the open points for a larger one.
* The disk keeps its flash sectors. A command that is running keeps its registers, and its end sets the disk's bit of the interrupt status, as in the emulator.

## The Reset and the Clock

The Pico gives the clock and the reset. It follows rule 5 of the protocol: the reset is held for at least 20 clocks, and during them the Pico neither drives the bus nor listens. The clock is a PWM output, the system clock divided by an even number.

## The Monitor

A person works with a serial terminal on the USB port of the Pico. What is typed goes to the console of the chip. One key, Ctrl-A, leaves the console for a command line:

    load     takes a firmware image (sent by the host script)
    run      resets the chip and lets it run
    halt     holds the chip in reset
    clock N  sets the clock to the system clock divided by N
    peek, poke   read and write a word of RAM or ROM while the chip is held
    stat     numbers about the bus: requests, repeats, transactions that broke off
    trace    the last requests of the chip with their answers
    faults   the last transactions that the bridge gave up, as it saw them
    watchdog 0|1   turns the watchdog off or on
    stop, step, go   hold the CPU between two instructions, run one instruction, let it run
    key      sends bytes as a keyboard does

The header comment of [bridge.c](../asc2-bridge/firmware/bridge.c) is the home of this list. Every answer of the monitor is a line that begins with `ok` or `err`, so that a script can read it.

The host script, [host/bridge.py](../asc2-bridge/host/bridge.py), loads an image and then becomes a plain terminal, so one command runs a program on the board. Its `check` runs a program and compares what it prints with a file.

## The Watchdog

The chip repeats a request that got no answer, so after a broken transaction a healthy chip is back within its timeout. The watchdog counts the time after a transaction that broke off. If no new request comes within a second, the chip has stopped in the middle of its work, and the firmware resets it and says so on the console. A CPU that sleeps until an interrupt breaks no transaction and is left alone. A monitor command turns the watchdog off for work on a chip that is stuck.

## Stopping and Stepping

The chip has a pin, STOP, that holds the CPU between two instructions while it is high. On the bench two things can raise it: the slide switch SW9 of the board, and the Pico. The Pico uses the wire from GP13 to pin 36 of the board's header. The bench memory test had this wire for a signal of its own, and it had no job since, so stopping and stepping need no new wire.

The bridge answers every fetch of the CPU, so it knows where an instruction begins without the debug port of the chip:

* `stop` raises STOP. The CPU ends its instruction and waits.
* `step` lowers STOP and asks the bus server to raise it again at the next fetch. The server does that before it answers the fetch. The answer takes dozens of clocks of the chip, and STOP needs two to get into the chip. So STOP is high long before the instruction ends, and the CPU runs this one instruction and waits again. The monitor then shows the requests of the instruction: the fetch with its address and the instruction word, and the reads and writes with their values.
* `go` lowers STOP.
* `run stopped` resets the chip with STOP high. The first `step` then runs the first instruction of the boot ROM.

A step shows the requests of the CPU. The command `regs` reads the debug port of the chip over three wires of its own and shows `ip`, the instruction, `cr5`, `flags`, `cr0`, the state of the CPU and of the bus unit, and every register. The chip takes the two inputs of the port through two flops of its clock, so the bridge holds every level for six clocks of the chip.

Two things to know:

* A CPU that is halted and waits for an interrupt fetches nothing. A step waits a second for a fetch and then says that none came. The kernel of [asc2-rach2/](../asc2-rach2/) sleeps so at its prompt, and its timer interrupts every two thirds of a second on the bench: a step there runs the first instruction of the timer's handler.
* The timer of the chip counts on while the CPU is stopped. A program that allows interrupts therefore often takes the timer's interrupt at a step: the requests of the step are then the read of the interrupt's vector and the first instruction of the handler.

The watchdog leaves a stopped chip alone.

## The Keyboard Player

The firmware sends a byte as a PS/2 frame on two pins, GP16 for the clock line and GP17 for the data line: 11 bits, each valid while the clock line is low, 80 us a bit. Both lines are outputs and high at rest. The chip only listens, so nothing else drives them. The monitor's `key` sends the bytes that follow it in hex: `key 1c f0 1c` is a press and a release of the key A of a PS/2 keyboard.

The receiver of the chip wants the clock line still for three clocks of the chip and drops a byte whose clock stands still for 32,768 clocks. Both hold for a clock of the chip from 125 kHz up.

On the bench the two lines have two wires of their own, from GP16 and GP17 to pins 6 and 26 of the board's header, placed on 2026-11-23, and the keyboard passed on the board at once. These two are the only free pins of the row of the header that carries the other wires (the manual of the DE10-Lite, table 3-7). The FPGA pulls both pins up, so a board without the two wires sees a keyboard that sends nothing. Not written: a command that turns a typed line into scancodes.

## The Board Target

[asc2-targets/de10-lite/](../asc2-targets/de10-lite/) holds one small top module. It connects the pins of the Tiny Tapeout top module to the same header pins that the bench design uses, so the wires on the table stay where they are. The three wires of the debug port are on the other row of the header; its two inputs have pull-ups in the FPGA, so the port's clock never rises without its wire. The two keyboard lines have pull-ups in the FPGA and read high until their wires are there. STOP is high when the slide switch SW9 is up or the Pico raises its wire.

The clock of the chip is chosen by a second slide switch: the 50 MHz of the board divided by 4, or the clock wire from the Pico. The first plan was the clock wire and nothing else; the next section says why it changed.

One bench safety stays outside the chip: the Pico tells on a wire when it drives the bus, and the top module of the board keeps the FPGA's drivers off during that time. The chip has no such input. The FPGA and the Pico both drive 3.3 V pins, and the guard keeps a mistake in either from being a short circuit. The design is loaded over JTAG as a `.sof` file and never written to the flash of the board.

## What the First Runs on the Board Showed (2026-11-23)

The smoke program, the eight C programs of [asc2-tests/c/](../asc2-tests/c/), and the kernel of phase 4 ran on the board and printed what they print on the emulator, with the clock of the Pico from 6.25 MHz to 37.5 MHz. A kernel boot is about 7 million bus transactions and takes 80 seconds at 12.5 MHz.

With the clock from the Pico, 3 to 7 transactions per kernel boot broke off. The chip repeated each one, and no run printed a wrong byte. The log of the bridge (`faults`) showed where: nearly all were accesses to the stack of a process, at addresses from `0x00ff74` to `0x00fff0`. The address bytes of such an access are `0xff` and then `0x00`, so all eight wires of the bus fall together, and a write of zeros changes them all again. Bus Tests 3 and 4 had found that the jumper wire of the clock shows the FPGA an edge too many when many wires of the bus fall at once. The bus unit of the chip was hardened then against such an edge: its state is checked, and a transaction that went wrong is repeated.

The check: the same design with the chip on the 50 MHz clock of the board divided by 4. Three kernel boots, 21 million transactions, none broke off. So the wires of the bus are clean, the chip needs no change, and the clock wire is a matter of the bench: a Tiny Tapeout board has a trace with a series resistor in its place. The board target now takes the clock of the board unless a slide switch asks for the Pico's.

The second check, on 2026-11-23: another jumper wire went into the place of the clock wire. Two runs of the board suite with the clock of the Pico followed, 123 million transactions, and none broke off. So the first wire was the cause. A run at a clock of 3 kHz, which took 8 seconds for the smoke program and not half a second, showed that the chip did take the Pico's clock.

One weak point of the bridge showed in the log and is left as it is: the bridge does an access when the last byte of its request has come, before it knows that the chip has ended the request. After an edge too many it can do an access that the chip did not ask for. A read of memory does no harm, and the repeat then gets the right answer. A write could change a wrong word. The request has no check of its own in bus protocol v1, so the bridge cannot tell; clean wires are the remedy.

The disk ran on the board too ([asc2-tests/board/disk.s](../asc2-tests/board/disk.s)): a sector written to the flash of the Pico, read back, and a command that ends with an interrupt. The CPU asks the status of the disk again and again while the flash is written, and none of these transactions broke off: core 1 serves the bus from RAM while core 0 writes the flash. The rule for a stall of the flash, one of the open bench decisions, is not needed for this firmware.

## The Order of the Work

1. The memory test moves to [asc2-bridge/bench/](../asc2-bridge/bench/) and passes on the bench from there.
2. The firmware, checked first on the Mac: its function that decides an access is compiled for the Mac and compared with `rp_access` of the emulator on random requests.
3. The board target, checked first in simulation: the board's top module with a model of the Pico.
4. The smoke program on the board: loaded by the monitor, printing on the USB console.

## Open Points

* Memory for the ROM. The RP2040 of the bench has room for an image of 48 KB. The kernel with the shell has 36 KB, and its programs are on the disk. Should it outgrow the room, the ways out are: a Pico 2 on the bench (the RP2350 has 520 KB, and the Tiny Tapeout board has one), less RAM in the RP2040 build (the chip reads the size of RAM from a register), or a ROM served from flash with a rule for the time when the flash is busy, which is one of the open decisions of the bench.
* Decided 2026-11-24: the three wires of the debug port go to the other row of the header, to its first three pins (1, 3, and 5 of JP1; the manual of the DE10-Lite, figure 3-18), from GP18, GP19, and GP20 of the Pico. That row has the 5 V pin (11) and the 3.3 V pin (29) of the board; pins 7 and 9 lie between the three wires and the 5 V pin. The Tiny Tapeout board has these wires on its own.
* The period of the chip's timer is 256 clocks of the chip, 20.48 us at 12.5 MHz.

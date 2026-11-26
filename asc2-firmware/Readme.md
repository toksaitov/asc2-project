Almurut II Firmware
===================

This directory holds example programs for the boot ROM, written in Almurut II assembly. The `.s` files are the sources. The assembled images are kept out of version control by [.gitignore](.gitignore). The overview of the whole platform is in [../Readme.md](../Readme.md).

## Programs

* [hello.s](hello.s): prints a greeting on the console and then prints back every byte that the console receives. It shows the least that a program needs: a stack, a handler in the vector table, the interrupt controller, and the console of the RP2040.

## Building and Running

Build the assembler and the emulator first, then assemble a program and run it:

```bash
make -C ../asc2-assembler
make -C ../asc2-emulated
../asc2-assembler/asa hello.s hello
../asc2-emulated/asc hello
```

`ctrl+c` leaves the emulator. Its other keys are in [../asc2-emulated/Readme.md](../asc2-emulated/Readme.md).

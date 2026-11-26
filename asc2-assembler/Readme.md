Almurut II Assembler
====================

This directory contains `asa`, the Almurut II assembler. It reads one assembly file and writes either a firmware image for the boot ROM or an object file for the linker. The overview of the whole platform is in [../Readme.md](../Readme.md). The assembler takes the instruction encodings from [../asc2-emulated/asc.h](../asc2-emulated/asc.h), so the emulator and the assembler cannot disagree about an instruction.

## Building and Running

```bash
make
./asa path_to_assembly output_file
./asa -c path_to_assembly output_file.o
```

Without options the output is a firmware image, which the emulator runs:

```bash
./asa ../asc2-firmware/hello.s ../asc2-firmware/hello
```

With `-c` the output is an ELF32 object file instead, for the linker of the C compiler. Object Files below describes it.

## Syntax

A line holds a label, a directive, an instruction, or a comment. A comment starts with `#` and takes its whole line.

```asm
# Adds the numbers from 1 to 10.

.section .data
total:
    .long 0x0

.section .text
main:
    li 0x0 r0
    li 0x1 r1
_next:
    add r1 r0
    add 0x1 r1
    cmp 0xa r1
    jle _next
    li total r2
    sr r0 0(r2)
    hlt
```

### Operands

* A register: `r0`-`r7`, `fp`, `sp`, `lr`, `ip`, `flags`.
* A control register: `cr0`-`cr8`.
* A register of the array of the dot products: `w0`-`w2`, `x0`-`x2`.
* A number: `0x1f` (hexadecimal), `0b1010` (binary), or `25` (decimal), with a minus sign if needed.
* A label.
* Memory: a displacement and a register in parentheses, as in `8(fp)` or `-4(sp)`.

The source comes first and the destination last. Operands are separated by spaces.

An immediate value in an instruction has 16 bits and a sign. `li` is the exception: for a label or a value that does not fit, the assembler writes two instructions, `li` with the lower half and `lih` with the upper half.

### Labels

A label is a name followed by a colon. It stands for the address of what follows it. A jump or a `call` to a label is encoded as a distance in instructions from the jump itself, so the code works at any address.

### Directives

* `.section .text` and `.section .data` choose the section for the lines that follow. A file starts in `.text`.
* `.byte value`, `.short value`, `.long value` put a number of 1, 2, or 4 bytes into the section. A second argument repeats it: `.long 0x0 16`. The value of `.long` can be a label.
* `.ascii "text"` puts the bytes of a string; `.asciz "text"` adds a zero byte after them. The escape sequences are `\n`, `\t`, `\r`, `\0`, `\\`, and `\"`.
* `.incbin "path"` puts the bytes of a file into the section as they are. The path starts from the directory where the assembler runs.
* `.align n` adds zero bytes until the size of the section is a multiple of `n`.
* `.global name` makes a label visible to other object files.

## Firmware Images

The text section of a firmware image becomes the boot ROM at the address `0x80800000`, where the CPU starts after a reset. The data section is copied into RAM at `0x80001000`. Every label must be defined in the file.

## Object Files

An object file is an ELF32 relocatable file with the machine number `0xa5c2`. It has the sections `.text` and `.data`, a symbol table, and the relocation sections `.rela.text` and `.rela.data`. A label that the file uses and does not define becomes an undefined symbol for the linker. The relocation types are the `ELF_RELOC_ASC2_` constants of [asc.h](../asc2-emulated/asc.h): a 4-byte address, the lower and the upper half of an address for `li` and `lih`, and the distance of a jump.

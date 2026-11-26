Almurut II C Compiler
=====================

This directory contains the C compiler of Almurut II: TinyCC 0.9.27 with a code generator for the machine. The compiler runs on the computer that builds the software and writes programs for Almurut II. The overview of the whole platform is in [../Readme.md](../Readme.md).

* [tcc-0.9.27.tar.bz2](tcc-0.9.27.tar.bz2): the TinyCC release as it was published.
* [asc2-gen.c](asc2-gen.c): the code generator. It turns the operations of C into instructions of the machine.
* [asc2-link.c](asc2-link.c): the relocation types and how the linker fills them in.
* [asc2-target.patch](asc2-target.patch): the changes to the files of TinyCC that all of its targets share.
* [lib/](lib/): the runtime library, the functions that the compiler calls for what the machine has no instruction for: division ([division.c](lib/division.c)), 64-bit numbers ([llong.c](lib/llong.c)), and the copying and filling of memory ([memory.c](lib/memory.c), and [memory_asm.s](lib/memory_asm.s) in assembly). Each file is a member of its own in the library, so a program gets only the ones that it uses.
* [Makefile](Makefile): builds everything into `build/`.

## Building

```bash
make
```

The build needs Make, a C compiler, `tar` with bzip2, `patch`, and `shasum` or `sha256sum`. It checks the archive against its SHA-256, unpacks it, applies the patch, and compiles TinyCC as one source file together with [asc2-gen.c](asc2-gen.c) and [asc2-link.c](asc2-link.c). The new compiler then builds its runtime library, and the assembler of [../asc2-assembler/](../asc2-assembler/) builds the part of it that is in assembly.

* `build/tcc`: the compiler and the linker
* `build/libtcc1.a`: the runtime library
* `build/include/`: `stdarg.h`, `stdbool.h`, and `stddef.h` of TinyCC

The path of `build/` is written into `tcc`, so the program finds its headers by itself and must be rebuilt if the directory moves. `make clean` and then `make` do that.

## Compiling a Program

```bash
build/tcc -c -o main.o main.c
../asc2-assembler/asa -c start.s start.o
build/tcc -nostdlib -Wl,-Ttext=0x80800000 -Wl,-Tdata=0x80001000 -o program.elf start.o main.o build/libtcc1.a
../asc2-tools/elf2fw/asc_elf2fw program.elf program
```

* `tcc -c` compiles one C file into an object file. Assembly files go through `asa -c`: the assembler of TinyCC is not built.
* The second `tcc` command links. `-nostdlib` is needed because there is no C library and no start-up code of TinyCC's own. `-Wl,-Ttext=` gives the address of the code and `-Wl,-Tdata=` the address of the variables; here they are the boot ROM and the RAM of a firmware image. The object file with the first instructions of the program goes first, and `libtcc1.a` goes last.
* The linker writes an ELF file. `asc_elf2fw` turns it into a firmware image for the emulator.

The linker defines the symbols `_etext`, `_edata`, and `_end` at the ends of the code, of the variables with a starting value, and of all variables. The start-up code uses the last two to set the variables without a starting value to zero. [../asc2-tests/c/start.s](../asc2-tests/c/start.s) is such start-up code, and the C suite in [../asc2-tests/Makefile](../asc2-tests/Makefile) is a complete example of the commands.

## What the Compiler Accepts

The language is C99 as TinyCC 0.9.27 implements it, with these rules for the machine:

* `char` is 1 byte and has a sign, `short` is 2 bytes, `int`, `long`, and a pointer are 4 bytes, and `long long` is 8 bytes.
* There is no floating point. A `float` or a `double` in use is a compile error.
* There is no `asm` in C code.
* The local variables of one function can take up to 32 KB, and so can the arguments of one call.
* A jump inside a function and a call reach 32767 instructions in either direction. The compiler or the linker reports one that does not.
* The macro `__asc2__` is defined.

## The Calling Convention

* The caller pushes the arguments, the last one first, and removes them after the call. Each argument takes a whole number of 4-byte words. A `char` or a `short` is passed as an `int`. A 64-bit argument takes two words, the lower half at the lower address. A structure is copied onto the stack in place of an argument.
* `call` leaves the return address in `lr`.
* The result comes back in `r0`. A 64-bit result has its upper half in `r1`. A `char` or a `short` comes back extended to 32 bits.
* A function that returns a structure gets the address of a place for it as a hidden first argument. The caller removes this argument too.
* A function may change `r0`-`r3` and the flags. It restores `r4`-`r7`, `fp`, and `sp`.

A function that the compiler writes starts and ends like this:

```asm
    push lr
    push fp
    mov sp fp
    sub 0x10 sp

    mov fp sp
    pop fp
    pop lr
    ret
```

`0x10` stands for the size of the local variables. Inside the function the stack looks like this:

```text
    12(fp)   the second argument
     8(fp)   the first argument
     4(fp)   the return address
     0(fp)   the fp of the caller
    -4(fp)   local variables
       ...
     0(sp)   the top of the stack
```

## How the Code Is Generated

TinyCC compiles in one pass and keeps the values of an expression on a stack of its own. A value there is a constant, a place in memory, a register, or the result of a comparison that is still in the flags. [asc2-gen.c](asc2-gen.c) supplies the small steps: load a value into a register, store a register, combine the two values on top, call, jump.

* `r0`, `r1`, and `r2` hold values. `r3` holds an address for a moment when a store to a global variable needs one. `r4`-`r7` are not used.
* A constant that fits 16 bits with a sign takes one `li`. Any other constant takes `li` and `lih`. The address of a global variable or a function is known only to the linker, so it always takes both, and a read of a global variable is three instructions.
* A local variable is one load or store at a distance from `fp`.
* A comparison is a `cmp`. What follows uses the flags: a conditional jump for an `if` or a loop, a `set` instruction for a comparison used as a number.
* 64-bit additions and subtractions use the carry: `add` then `adc`, `sub` then `sbb`.

The machine has no instruction for the following, so the compiler calls the runtime library:

* [/](./) and `%`: `__divsi3`, `__modsi3`, `__udivsi3`, `__umodsi3`
* 64-bit [/](./) and `%`: `__divdi3`, `__moddi3`, `__udivdi3`, `__umoddi3`
* a 64-bit product: `__umulsidi3`
* a 64-bit shift by a count that is not a constant: `__ashldi3`, `__lshrdi3`, `__ashrdi3`
* a copy of a structure and the zeros of a partly initialized local array: `memmove`, `memset`

A division by zero does not stop the program. The quotient is all ones, and the remainder is the dividend.

## Object Files

The compiler writes the same ELF32 object files as the assembler: the machine number `0xa5c2`, relocation sections with addends, and the four relocation types of `ELF_RELOC_ASC2_` in [../asc2-emulated/asc.h](../asc2-emulated/asc.h). The linker therefore joins files from both.

## The Patch

[asc2-target.patch](asc2-target.patch) changes six files of TinyCC:

* `tcc.h`: names the target, includes its two files, and selects relocation records with addends. It also adds the address for `-Wl,-Tdata=` to the state of the compiler.
* `libtcc.c`: includes the two files, defines `__asc2__`, links statically, and reads `-Wl,-Tdata=`.
* `tccelf.c`: starts the writable sections at the address of `-Wl,-Tdata=`.
* `tccgen.c`: writes the addend of an address in data into the relocation record. TinyCC did this for its 64-bit targets only.
* `tcctok.h`: the names of the runtime functions for division and the 64-bit product.
* `tcc.c`: the name of the target for `tcc -v`.

To change the patch, edit the files in `build/tcc-0.9.27/`, unpack the archive a second time elsewhere, and write the differences of the six files with `diff -u`, with the names `a/file` and `b/file` in the headers. The next build in a clean `build/` applies the new patch.

## Tests

```bash
cd ../asc2-tests/
make c-suite
```

Each program in [../asc2-tests/c/](../asc2-tests/c/) is built twice, for the machine and for the computer that runs the tests, and the two outputs must be equal. [../asc2-tests/Readme.md](../asc2-tests/Readme.md) describes them.

## License

TinyCC is distributed under the GNU Lesser General Public License; the archive has its text in `COPYING`. [asc2-gen.c](asc2-gen.c) and [asc2-link.c](asc2-link.c) are modelled on the i386 files of TinyCC and carry the same license.

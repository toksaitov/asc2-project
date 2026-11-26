/*
    Output of the C test programs.

    A test prints its results as text. The same C file is built for the
    machine and for the computer that runs the tests, and the two outputs must
    be equal. `put_char` is the only part that differs: `console.s` on the
    machine, `host.c` on the computer.

    The tests use only the types that have the same size on both: `char`,
    `short`, `int`, and `long long`.
*/

#ifndef OUT_H
#define OUT_H

void put_char(int c);

void put_str(const char *str);
void put_hex(unsigned int value);
void put_hex64(unsigned long long value);
void put_dec(int value);

/* Prints a line with a name and a value: "name = 0x0000002a" */
void show(const char *name, unsigned int value);
void show64(const char *name, unsigned long long value);

#endif

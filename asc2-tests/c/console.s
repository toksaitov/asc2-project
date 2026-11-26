# void put_char(int c)
#
# Prints a byte on the console of the RP2040: waits until the status register
# has the bit 0x2 (the console can take a byte), then writes the byte to the
# data register.
#
# This is a C function written in assembly. The caller has pushed the
# argument, and `call` has put the return address in `lr`, so the argument is
# on the top of the stack. A function may change `r0`-`r3` freely.

.global put_char

.section .text
put_char:
    lr 0(sp) r0
    li 0x80ff0000 r1
_put_char_wait:
    lr 0x10(r1) r2
    test 0x2 r2
    je _put_char_wait
    sr r0 0x14(r1)
    ret

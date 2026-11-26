# `ssat`, `usat`, and `popcnt`.
#
# The sum of a neuron is a 32-bit number, and the next layer takes signed
# bytes. `ssat rd` holds `rd` in the range of a signed byte: a value below
# -128 becomes -128, and a value above 127 becomes 127. `usat rd` holds it
# between 0 and 127: a negative value becomes 0, which is the rule of a
# rectified linear unit (ReLU). `popcnt ra rd` counts the bits of `ra` that
# are set. None of the three changes the flags.
#
# An entry of the table is four words: a value, the value after `ssat`,
# after `usat`, and the count of its bits.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
entries:
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000001
    .long 0x00000001
    .long 0x00000001
    .long 0x00000001
    .long 0xffffffff
    .long 0xffffffff
    .long 0x00000000
    .long 0x00000020
    .long 0x0000007e
    .long 0x0000007e
    .long 0x0000007e
    .long 0x00000006
    .long 0x0000007f
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000007
    .long 0x00000080
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000001
    .long 0x00000081
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000002
    .long 0xffffff81
    .long 0xffffff81
    .long 0x00000000
    .long 0x0000001a
    .long 0xffffff80
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000019
    .long 0xffffff7f
    .long 0xffffff80
    .long 0x00000000
    .long 0x0000001f
    .long 0xffffff7e
    .long 0xffffff80
    .long 0x00000000
    .long 0x0000001e
    .long 0x000000ff
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000008
    .long 0x00000100
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000001
    .long 0xffffff00
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000018
    .long 0x7fffffff
    .long 0x0000007f
    .long 0x0000007f
    .long 0x0000001f
    .long 0x80000000
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000001
    .long 0x00010000
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000001
    .long 0xffff0000
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000010
    .long 0x40000000
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000001
    .long 0xc0000000
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000002
    .long 0x0000007f
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000007
    .long 0x00000080
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000001
    .long 0x0000ff80
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000009
    .long 0x0000ffff
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000010
    .long 0x12345678
    .long 0x0000007f
    .long 0x0000007f
    .long 0x0000000d
    .long 0xedcba988
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000011
    .long 0xd837115f
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000011
    .long 0xea8ff5f8
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000015
    .long 0x026f4c84
    .long 0x0000007f
    .long 0x0000007f
    .long 0x0000000c
    .long 0x50a74cff
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000012
    .long 0x73e1d036
    .long 0x0000007f
    .long 0x0000007f
    .long 0x00000010
    .long 0x8f8de0e9
    .long 0xffffff80
    .long 0x00000000
    .long 0x00000011
entries_end:

.section .text
main:
    li 0x80020000 sp

    # The table, over flags that are all clear and over flags that are all
    # set in turn. `popcnt` runs with two registers and with one.
    li 0x1 r7
    li entries r5
    li 0x0 r6
_entry:
    lr 0(r5) r1
    mov r6 flags
    ssat r1
    mov flags r4
    cmp r6 r4
    jne fail
    lr 4(r5) r0
    cmp r0 r1
    jne fail

    lr 0(r5) r1
    mov r6 flags
    usat r1
    mov flags r4
    cmp r6 r4
    jne fail
    lr 8(r5) r0
    cmp r0 r1
    jne fail

    lr 0(r5) r1
    li -1 r2
    mov r6 flags
    popcnt r1 r2
    mov flags r4
    cmp r6 r4
    jne fail
    lr 12(r5) r0
    cmp r0 r2
    jne fail
    lr 0(r5) r0
    cmp r0 r1
    jne fail
    popcnt r1 r1
    lr 12(r5) r0
    cmp r0 r1
    jne fail

    xor 0xf r6
    add 0x10 r5
    li entries_end r0
    cmp r0 r5
    jne _entry

    # One bit at a time: a single bit counts as 1 wherever it is, `usat`
    # gives the bit itself up to 64 and 127 above, except for the top bit,
    # which makes the value negative.
    li 0x2 r7
    li 0x0 r5
    li 0x1 r4
_bit:
    popcnt r4 r0
    cmp 0x1 r0
    jne fail
    mov r4 r0
    not r0
    popcnt r0 r0
    cmp 0x1f r0
    jne fail
    mov r4 r1
    usat r1
    mov r4 r2
    ssat r2
    cmp 0x7 r5
    jge _bit_large
    cmp r4 r1
    jne fail
    cmp r4 r2
    jne fail
    jmp _bit_next
_bit_large:
    cmp 0x1f r5
    je _bit_top
    cmp 0x7f r1
    jne fail
    cmp 0x7f r2
    jne fail
    jmp _bit_next
_bit_top:
    cmp 0x0 r1
    jne fail
    cmp -128 r2
    jne fail
_bit_next:
    shl 0x1 r4
    add 0x1 r5
    cmp 0x20 r5
    jne _bit

    # `sp` as an operand: 0x80010004 has three bits, is far below -128, and
    # is negative.
    li 0x3 r7
    mov sp r6
    li 0x80010004 sp
    popcnt sp r0
    popcnt sp sp
    mov sp r1
    li 0x80010004 sp
    ssat sp
    mov sp r2
    li 0x80010004 sp
    usat sp
    mov sp r3
    mov r6 sp
    cmp 0x3 r0
    jne fail
    cmp 0x3 r1
    jne fail
    cmp -128 r2
    jne fail
    cmp 0x0 r3
    jne fail

    # A network of one bit a weight. A bit that is set stands for +1, a clear
    # one for -1, and `xor` multiplies 32 of them at once: a bit of the
    # result is set where the product is -1. With `count` such bits the sum
    # of the 32 products is 32 - 2 * count. The inputs 0xffff0000 and the
    # weights 0xff00ff0f differ in 20 places: the sum is -8.
    li 0x4 r7
    li 0xffff0000 r1
    li 0xff00ff0f r2
    xor r1 r2
    popcnt r2 r2
    shl 0x1 r2
    li 0x20 r0
    sub r2 r0
    cmp -8 r0
    jne fail

    # After `mul`, `dot4`, and a shift, which use the same counter inside the
    # chip.
    li 0x5 r7
    li 0x7 r3
    mul 0x6 r3
    li 0x0f0f0f0f r1
    popcnt r1 r2
    cmp 0x10 r2
    jne fail
    cmp 0x2a r3
    jne fail
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 r3
    popcnt r3 r4
    ssat r3
    cmp 0x7f r3
    jne fail
    cmp 0x2 r4
    jne fail
    li 0x1 r3
    shl 0x1f r3
    popcnt r3 r4
    usat r3
    cmp 0x1 r4
    jne fail
    cmp 0x0 r3
    jne fail
    li -1 r3
    popcnt r3 r4
    li 0x7 r3
    mul 0x6 r3
    cmp 0x20 r4
    jne fail
    cmp 0x2a r3
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

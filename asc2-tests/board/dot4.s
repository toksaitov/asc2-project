# The dot-product instruction on the board.
#
# `dot4 ra rb rd` treats `ra` and `rb` as four signed bytes each, multiplies
# the bytes in the same positions, and adds the four products to `rd`.
# `../asm/cpu_mul_dot4.s` checks its rules on the emulator and on the chip in
# simulation. This program runs the instruction on the chip in the FPGA. For
# every entry of a table it prints the sum that `dot4` gives as eight hex
# digits, and it computes the same sum a second time, byte by byte with
# `mul`. A sum that differs ends the program with a message.
# `make board-suite` compares the lines with `dot4.expect`.
#
# An entry of the table is three words: the two words of four bytes each,
# and the number that the sum is added to.
#
# `r7` holds the address of the entry, `r4` and `r5` its two words, `r6` the
# sum of `dot4`, and `r2` the sum of the products of `mul`. The program ends
# as the tests of `../asm/` do: with 0x600d in `r0`, or with 1 if a sum
# differs.

.section .data
# The words of the table come first in the section, so that each one is at an
# address that is a multiple of four.
entries:
    # 1*5 + 2*6 + 3*7 + 4*8 = 70, added to 100
    .long 0x04030201
    .long 0x08070605
    .long 0x00000064
    # Bytes are signed: (-1)*10 + 2*(-20) + (-3)*30 + 4*(-40) = -300
    .long 0x04fd02ff
    .long 0xd81eec0a
    .long 0x00000000
    # The largest sum: four times (-128)*(-128) is 65536
    .long 0x80808080
    .long 0x80808080
    .long 0x00000000
    # The smallest sum: four times 127*(-128) is -65024
    .long 0x7f7f7f7f
    .long 0x80808080
    .long 0x00000000
    # The sum passes the largest positive number and wraps around
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7fffffff
    # Four products that are zero leave the number as it is
    .long 0x00000000
    .long 0x12345678
    .long 0xdeadbeef
    # Mixed signs: 127*1 + (-1)*(-128) + (-128)*127 + 1*(-1) = -16002, added to 5
    .long 0x0180ff7f
    .long 0xff7f8001
    .long 0x00000005
entries_end:

digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "dot4: ready\n"
line_differs:
    .asciz "dot4: the sum of mul differs\n"
line_done:
    .asciz "dot4: done\n"

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3

    li line_ready r2
    call print

    li entries r7
_next_entry:
    lr 0(r7) r4
    lr 4(r7) r5
    lr 8(r7) r6
    mov r6 r2

    dot4 r4 r5 r6

    # The same sum with `mul`, one pair of bytes at a time
    call add_product
    call add_product
    call add_product
    call add_product
    cmp r2 r6
    jne _differs

    call print_word

    add 0xc r7
    li entries_end r0
    cmp r0 r7
    jne _next_entry

    li line_done r2
    call print
    li 0x600d r0
    hlt

_differs:
    li line_differs r2
    call print
    li 0x1 r0
    hlt

# Adds the product of the lowest bytes of `r4` and `r5`, taken as signed
# numbers, to `r2`, and moves the next bytes of both words to the lowest
# place. A shift to the left by 24 and a sign-filling shift back turn a byte
# into a signed number.
add_product:
    mov r4 r0
    shl 0x18 r0
    sar 0x18 r0
    mov r5 r1
    shl 0x18 r1
    sar 0x18 r1
    mul r1 r0
    add r0 r2
    shr 0x8 r4
    shr 0x8 r5
    ret

# Prints the word in `r6` as eight hex digits and ends the line. `r5` holds
# the digits that are still to print, the next one in its upper four bits,
# and `r4` counts them.
print_word:
    push lr
    mov r6 r5
    li 0x8 r4
_print_word_next:
    mov r5 r0
    shr 0x1c r0
    call print_digit
    shl 0x4 r5
    sub 0x1 r4
    cmp 0x0 r4
    jne _print_word_next
    li 0xa r0
    call print_byte
    pop lr
    ret

# Prints the hex digit of the number in `r0`, which is from 0 to 15.
print_digit:
    push lr
    li digits r1
    add r0 r1
    lrbu 0(r1) r0
    call print_byte
    pop lr
    ret

# Prints the string at the address in `r2`. A zero byte ends the string.
print:
    push lr
_print_next:
    lrbu 0(r2) r0
    cmp 0x0 r0
    je _print_end
    call print_byte
    add 0x1 r2
    jmp _print_next
_print_end:
    pop lr
    ret

# Prints the byte in `r0`: waits until the console is ready, then writes the
# byte to the data register.
print_byte:
    lr 0x10(r3) r1
    test 0x2 r1
    je print_byte
    sr r0 0x14(r3)
    ret

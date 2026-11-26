# The dot-product instruction, every pair of bytes.
#
# `dot4 ra rb rd` treats `ra` and `rb` as four signed bytes each, multiplies
# the bytes in the same positions, and adds the four products to `rd`.
# `../asm/cpu_mul_dot4.s` checks its rules. This program goes through all 65536
# pairs of bytes in each of the four positions, with zeros in the other
# three: 262144 instructions `dot4`. Each one adds its product to `r6`, and
# `mul` adds the product of the same two bytes to `r2`. After every pair the
# two sums must be equal.
#
# The sum of the products of all pairs of signed bytes is (-128) * (-128),
# 16384: the 256 bytes add up to -128. The sums start at 0x12345678, so the
# program prints 0x12349678 after the first position, then 0x1234d678,
# 0x12351678, and 0x12355678.
#
# `r7` holds the number of bits that a byte is moved left to its position,
# `r4` the first byte, and `r5` the second. On a failure the program prints a
# line and parks the CPU with 1 in `r0`; at the end it parks with 0x600d in
# `r0`.

.section .data
digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "dot4 lanes: ready\n"
line_differs:
    .asciz "dot4 lanes: the sum of mul differs\n"
line_done:
    .asciz "dot4 lanes: done\n"

.section .text
main:
    li 0x80020000 sp

    li line_ready r2
    call print

    li 0x12345678 r6
    mov r6 r2
    li 0x0 r7
_lane:
    li 0x0 r4
_first:
    li 0x0 r5
_second:
    mov r4 r0
    shl r7 r0
    mov r5 r1
    shl r7 r1
    dot4 r0 r1 r6

    # The same product with `mul`. A shift to the left by 24 and a
    # sign-filling shift back turn a byte into a signed number.
    mov r4 r0
    shl 0x18 r0
    sar 0x18 r0
    mov r5 r1
    shl 0x18 r1
    sar 0x18 r1
    mul r1 r0
    add r0 r2
    cmp r2 r6
    jne _differs

    add 0x1 r5
    cmp 0x100 r5
    jne _second
    add 0x1 r4
    cmp 0x100 r4
    jne _first

    push r2
    mov r6 r0
    call print_word
    pop r2

    add 0x8 r7
    cmp 0x20 r7
    jne _lane

    li line_done r2
    call print
    li 0x600d r0
    hlt

_differs:
    li line_differs r2
    call print
    li 0x1 r0
    hlt

# Prints the word in `r0` as eight hex digits and ends the line. `r5` holds
# the digits that are still to print, the next one in its upper four bits,
# and `r4` counts them.
print_word:
    push lr
    push r4
    push r5
    mov r0 r5
    li 0x8 r4
_print_word_next:
    mov r5 r0
    shr 0x1c r0
    li digits r1
    add r0 r1
    lrbu 0(r1) r0
    call print_byte
    shl 0x4 r5
    sub 0x1 r4
    cmp 0x0 r4
    jne _print_word_next
    li 0xa r0
    call print_byte
    pop r5
    pop r4
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
    li 0x80ff0000 r3
_print_byte_wait:
    lr 0x10(r3) r1
    test 0x2 r1
    je _print_byte_wait
    sr r0 0x14(r3)
    ret

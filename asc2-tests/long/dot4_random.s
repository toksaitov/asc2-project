# The dot-product instruction on 16384 random pairs of words.
#
# `dot4 ra rb rd` treats `ra` and `rb` as four signed bytes each, multiplies
# the bytes in the same positions, and adds the four products to `rd`.
# `../asm/cpu_mul_dot4.s` checks its rules, and `dot4_lanes.s` goes through
# every pair of bytes in one position at a time. Here all four positions work
# at once: both words are random, and a third random word is added to the sum
# before each `dot4`, so the sum that `dot4` adds to is random too. `mul` adds
# the four products to a second sum, one pair of bytes at a time, and after
# every `dot4` the two sums must be equal.
#
# The random words come from the generator xorshift32 with the start value
# 0x2545f491: x ^= x << 13, x ^= x >> 17, x ^= x << 5. `dot4_expect.py`
# computes the last sum, which the program prints, in another way.
#
# `fp` holds the state of the generator, `r4` and `r5` the two words, `r6`
# the sum of `dot4`, `r2` the sum of `mul`, and `r7` the number of pairs that
# are left. On a failure the program prints a line and parks the CPU with 1
# in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "dot4 random: ready\n"
line_differs:
    .asciz "dot4 random: the sum of mul differs\n"
line_done:
    .asciz "dot4 random: done\n"

.section .text
main:
    li 0x80020000 sp

    li line_ready r2
    call print

    li 0x2545f491 fp
    li 0x0 r6
    li 0x0 r2
    li 0x4000 r7
_next:
    call random
    mov r0 r4
    call random
    mov r0 r5
    call random
    add r0 r6
    add r0 r2

    dot4 r4 r5 r6

    call add_product
    call add_product
    call add_product
    call add_product
    cmp r2 r6
    jne _differs

    sub 0x1 r7
    jne _next

    mov r6 r0
    call print_word

    li line_done r2
    call print
    li 0x600d r0
    hlt

_differs:
    li line_differs r2
    call print
    li 0x1 r0
    hlt

# Puts the next random word in `r0`.
random:
    mov fp r0
    shl 0xd r0
    xor r0 fp
    mov fp r0
    shr 0x11 r0
    xor r0 fp
    mov fp r0
    shl 0x5 r0
    xor r0 fp
    mov fp r0
    ret

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

# The array of the dot products on 4096 rounds of random words.
#
# `../asm/cpu_array.s`, `../asm/cpu_learn.s`, and `../asm/cpu_sat.s` check
# the rules of the instructions on chosen words. Here every word is random.
# A round
#
# * fills the six registers of the array, `w0` to `w2` and `x0` to `x2`;
# * runs `dotv` into a random sum, and takes the sum and the flags C, Z, S,
#   and O after it;
# * runs `dotm` into three random sums, and takes the three;
# * runs `learn` with random errors, and takes the three registers of the
#   weights;
# * runs `xsh` with a random byte, and takes the three registers of the
#   inputs;
# * runs `ssat`, `usat`, and `popcnt` on one more word, and takes the three
#   results.
#
# Everything that a round takes is added to one total, which the program
# prints at the end. The program cannot tell whether the total is right:
# `array_expect.py` computes it by the rules of the instructions, with the
# integers of Python.
#
# The random words come from the generator xorshift32 with the start value
# 0x2545f491: x ^= x << 13, x ^= x >> 17, x ^= x << 5.
#
# `fp` holds the state of the generator, `r6` the total, and `r7` the number
# of rounds that are left. At the end the program parks the CPU with 0x600d
# in `r0`.

.section .data
digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "array random: ready\n"
line_done:
    .asciz "array random: done\n"

.section .text
main:
    li 0x80020000 sp

    li line_ready r2
    call print

    li 0x2545f491 fp
    li 0x0 r6
    li 0x1000 r7
_next:
    call random
    mov r0 w0
    call random
    mov r0 w1
    call random
    mov r0 w2
    call random
    mov r0 x0
    call random
    mov r0 x1
    call random
    mov r0 x2

    call random
    mov r0 r1
    dotv r1
    mov flags r0
    and 0xf r0
    add r0 r6
    add r1 r6

    call random
    mov r0 r1
    call random
    mov r0 r2
    call random
    mov r0 r3
    dotm r1
    add r1 r6
    add r2 r6
    add r3 r6

    call random
    learn r0
    mov w0 r1
    add r1 r6
    mov w1 r1
    add r1 r6
    mov w2 r1
    add r1 r6

    call random
    xsh r0
    mov x0 r1
    add r1 r6
    mov x1 r1
    add r1 r6
    mov x2 r1
    add r1 r6

    call random
    mov r0 r1
    ssat r1
    add r1 r6
    mov r0 r1
    usat r1
    add r1 r6
    popcnt r0 r1
    add r1 r6

    sub 0x1 r7
    jne _next

    mov r6 r0
    call print_word

    li line_done r2
    call print
    li 0x600d r0
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

# The dot-product instruction between timer interrupts.
#
# The program does what `dot4_random.s` does on 4096 random pairs of words,
# while the timer interrupts it every 37 steps. The handler runs a `dot4` of
# its own between two instructions of the program, also between the `dot4` of
# the program and the instructions before and after it. The cells of `dot4`
# keep nothing from one instruction to the next, so neither sum may suffer.
#
# The handler adds the dot product of the word 0x7f80ff01 with itself,
# 1 + 1 + 16384 + 16129 = 32515, to a sum in memory and counts its calls. At
# the end the sum must be the number of calls times 32515, and there must
# have been calls. Their number differs between the emulator, where the timer
# steps once per instruction, and the chip, where it steps once in 256
# clocks, so the program does not print it.
#
# `fp` holds the state of the generator, `r4` and `r5` the two words, `r6`
# the sum of `dot4`, `r2` the sum of `mul`, and `r7` the number of pairs that
# are left. On a failure the program prints a line and parks the CPU with 1
# in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
# The two words of the handler come first in the section, so that each one is
# at an address that is a multiple of four: the number of its calls, and its
# sum.
handler_calls:
    .long 0x00000000
handler_sum:
    .long 0x00000000

digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "dot4 irq: ready\n"
line_differs:
    .asciz "dot4 irq: the sum of mul differs\n"
line_handler:
    .asciz "dot4 irq: the sum of the handler is wrong\n"
line_done:
    .asciz "dot4 irq: done\n"

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)

    li line_ready r2
    call print

    # Open line 0 of the interrupt controller and start the timer.
    li 0xfe r0
    call set_irq_mask
    li 0x25 r0
    call set_timer

    li 0x2545f491 fp
    li 0x0 r6
    li 0x0 r2
    li 0x1000 r7
    sti
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

    cli

    li handler_calls r4
    lr 0(r4) r0
    cmp 0x0 r0
    je _handler_wrong
    mul 0x7f03 r0
    lr 4(r4) r1
    cmp r0 r1
    jne _handler_wrong

    mov r6 r0
    call print_word

    li line_done r2
    call print
    li 0x600d r0
    hlt

_handler_wrong:
    li line_handler r2
    call print
    li 0x2 r0
    hlt

_differs:
    cli
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

# The handler of the timer. It leaves every register as it was.
on_timer:
    push r0
    push r1
    push r2
    li handler_calls r2
    lr 0(r2) r0
    add 0x1 r0
    sr r0 0(r2)
    lr 4(r2) r1
    li 0x7f80ff01 r0
    dot4 r0 r0 r1
    sr r1 4(r2)
    pop r2
    pop r1
    pop r0
    iret

# Sets the mask of the interrupt controller to the low byte of `r0`: a 1
# masks a line.
set_irq_mask:
    li 0x20 r1
    li 0x0 r2
    outb r2 r1
    li 0x21 r1
    outb r0 r1
    ret

# Starts the timer with the period in the low 16 bits of `r0`.
set_timer:
    li 0x43 r1
    li 0x0 r2
    outb r2 r1
    li 0x40 r1
    mov r0 r2
    shr 0x8 r2
    outb r2 r1
    li 0x43 r1
    li 0x1 r2
    outb r2 r1
    li 0x40 r1
    outb r0 r1
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

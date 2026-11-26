# The keyboard on the board.
#
# The Pico plays a PS/2 keyboard on two wires: the `key` command of the
# bridge's monitor sends bytes as a keyboard does. The receiver of the chip
# turns the two wires into bytes for the keyboard controller.
# `../asm/cpu_irq_kbd.s` explains the controller and checks its rules on the
# emulator. This program checks the wires and the receiver: it prints every
# byte that arrives as two hex digits on a line of its own, and after the
# third byte it stops. `make board-keys` sends three bytes and compares the
# lines with `keys.expect`.
#
# The program asks the controller again and again and uses no interrupt.
#
# The I/O ports of the keyboard controller:
#
#     0x60  data: the last byte received; reading it clears the bit 0x1 of
#           the status
#     0x64  status: 0x1 = a byte has arrived and has not been read,
#                   0x2 = a byte was lost because it was not read in time
#
# `r6` counts the bytes that are still to come, and `r4` holds the last one.
# The program ends as the tests of `../asm/` do: with 0x600d in `r0`, or
# with 1 if the controller says that a byte was lost.

.section .data
digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "keys: ready\n"
line_lost:
    .asciz "keys: a byte was lost\n"
line_done:
    .asciz "keys: done\n"

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3

    li line_ready r2
    call print

    li 0x3 r6
_wait_for_key:
    li 0x64 r1
    inb r1 r0
    test 0x1 r0
    je _wait_for_key

    li 0x60 r1
    inb r1 r4

    # The upper four bits first
    mov r4 r0
    shr 0x4 r0
    call print_digit
    mov r4 r0
    and 0xf r0
    call print_digit
    li 0xa r0
    call print_byte

    sub 0x1 r6
    cmp 0x0 r6
    jne _wait_for_key

    li 0x64 r1
    inb r1 r0
    test 0x2 r0
    jne _lost

    li line_done r2
    call print
    li 0x600d r0
    hlt

_lost:
    li line_lost r2
    call print
    li 0x1 r0
    hlt

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

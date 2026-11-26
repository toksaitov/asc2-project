# The devices of the RP2040: the push button.
#
# One button stands next to the RP2040. The chip has no pin for it: the
# RP2040 reads it and gives it two registers (see `rp_console.s` for the
# others):
#
#     0x80ff0018  interrupt status: 0x4 = the button was pressed; writing
#                 0x4 clears the bit
#     0x80ff001c  interrupt enable: the same bit
#     0x80ff0034  button: 0x1 while the button is down
#     0x80ff0038  presses: the number of presses since the reset
#
# A program that only asks for the level misses a press that begins and ends
# between two questions. The count of presses misses none, however seldom
# the program asks. A program that has nothing to do until the next press
# sleeps in `hlt` and lets the RP2040 request an interrupt (line 2, the
# vector 10).
#
# The file `rp_button.events` presses the button twice. The handler of the
# RP2040's interrupt counts in `r6` and leaves the interrupt status it saw
# in `r4`. The handler of the timer does nothing.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80030000 sp

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)
    li on_rp r1
    sr r1 40(r0)

    li 0x0 r6
    li 0x80ff0000 r3

    # Nobody has touched the button yet.
    li 0x1 r7
    lr 0x34(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x38(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    # The first press: the count goes up while the button is still down.
    li 0x2 r7
    li 0x2710 r1
_wait_2:
    lr 0x38(r3) r0
    cmp 0x0 r0
    jne _pressed_2
    sub 0x1 r1
    jne _wait_2
    jmp fail
_pressed_2:
    cmp 0x1 r0
    jne fail
    lr 0x34(r3) r0
    cmp 0x1 r0
    jne fail

    # The release changes the level and not the count.
    li 0x3 r7
    li 0x2710 r1
_wait_3:
    lr 0x34(r3) r0
    cmp 0x0 r0
    je _released_3
    sub 0x1 r1
    jne _wait_3
    jmp fail
_released_3:
    lr 0x38(r3) r0
    cmp 0x1 r0
    jne fail

    # The press shows in the interrupt status, enabled or not, until the
    # program clears it.
    li 0x4 r7
    lr 0x18(r3) r0
    cmp 0x4 r0
    jne fail
    li 0x4 r0
    sr r0 0x18(r3)
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    # The program sleeps in `hlt` until the next press. The line of the
    # timer is open too, for the reason that `rp_disk.s` gives: the press
    # may come between the `sti` and the `hlt`.
    li 0x5 r7
    li 0x4 r0
    sr r0 0x1c(r3)
    li 0xfa r0
    call set_irq_mask
    li 0x2710 r1
_sleep_5:
    sti
    hlt
    cli
    cmp 0x0 r6
    jne _woken_5
    sub 0x1 r1
    jne _sleep_5
    jmp fail
_woken_5:
    cmp 0x1 r6
    jne fail
    cmp 0x4 r4
    jne fail
    lr 0x38(r3) r0
    cmp 0x2 r0
    jne fail
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    cli
    mov r7 r0
    hlt

# Sets the mask of the interrupt controller to the low byte of `r0`.
set_irq_mask:
    li 0x20 r1
    li 0x0 r2
    outb r2 r1
    li 0x21 r1
    outb r0 r1
    ret

on_timer:
    iret

on_rp:
    push r0
    add 0x1 r6
    lr 0x18(r3) r4
    li 0x4 r0
    sr r0 0x18(r3)
    pop r0
    iret

# The push button on the board.
#
# The program sleeps in `hlt` and lets the button wake it: the RP2040 requests
# an interrupt at every press (see `../asm/rp_button.s` for the registers).
# It prints a line for each of two presses and then the count of presses.
#
# On the emulator `button.events` presses the button; on the board the
# bridge does it with its command `press`, which holds the pin of the button
# as a finger would, or somebody presses the real one. `button.expect` is
# the output.

.section .data
waiting:
    .asciz "waiting\n"
press_1:
    .asciz "press 1\n"
press_2:
    .asciz "press 2\n"
count_2:
    .asciz "count 2\n"
count_other:
    .asciz "another count\n"

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)
    li on_rp r1
    sr r1 40(r0)

    # The button may request interrupts; the lines of the timer and of the
    # RP2040 are open (a 0 bit in the mask). The timer wakes the CPU again
    # and again, so a press between a `sti` and its `hlt` is not slept over.
    li 0x0 r6
    li 0x4 r0
    sr r0 0x18(r3)
    sr r0 0x1c(r3)
    li 0x20 r1
    li 0x0 r0
    outb r0 r1
    li 0x21 r1
    li 0xfa r0
    outb r0 r1

    li waiting r2
    call print

    li 0x1 r5
    call sleep_until
    li press_1 r2
    call print

    li 0x2 r5
    call sleep_until
    li press_2 r2
    call print

    li count_2 r2
    lr 0x38(r3) r0
    cmp 0x2 r0
    je _count_end
    li count_other r2
_count_end:
    call print

    cli
    li 0x600d r0
    hlt

# Sleeps until the handler has counted as many presses as `r5` says.
sleep_until:
    sti
    hlt
    cli
    cmp r5 r6
    jb sleep_until
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

on_timer:
    iret

# Counts a press in `r6` and clears the request of the button.
on_rp:
    push r0
    add 0x1 r6
    li 0x4 r0
    sr r0 0x18(r3)
    pop r0
    iret

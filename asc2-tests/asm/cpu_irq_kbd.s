# Interrupts: the keyboard controller.
#
# The keyboard sends one byte for a key going down and more for a key going
# up. The keyboard controller receives a byte, keeps it in its data register,
# and requests an interrupt on line 1 of the interrupt controller (the vector
# 9). It has room for one byte: a byte that arrives before the last one was
# read takes its place.
#
# The I/O ports:
#
#     0x60  data: the last byte received; reading it clears the bit 0x1 of
#           the status
#     0x64  status: 0x1 = a byte has arrived and has not been read,
#                   0x2 = a byte was lost because it was not read in time;
#           a write with the bit 0x2 clears that bit
#
# The bit 0x2 stays through a read of the data. A driver reads the status
# first and the data second, and a byte may be lost between the two reads:
# the driver finds the bit at its next look and clears it then.
#
# The bytes of this test come from the file `cpu_irq_kbd.events`, which says
# after how many instructions each byte arrives. The test waits for each of
# them in a loop with a limit.
#
# The handlers use these registers:
#
# * `r6`: the number of keyboard interrupts so far;
# * `r3`, `r4`, `r2`: the status before the read, the byte, and the status
#   after the read in the last keyboard interrupt;
# * `r5`: the order of the handlers: each one shifts `r5` left by four bits
#   and adds its mark, 1 for the timer and 2 for the keyboard.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)
    li on_keyboard r1
    sr r1 36(r0)

    li 0x0 r6
    li 0x0 r5

    # No byte has arrived yet.
    li 0x1 r7
    li 0x64 r1
    inb r1 r0
    cmp 0x0 r0
    jne fail

    # Open line 1 and wait for the first byte with interrupts on.
    li 0x2 r7
    li 0xfd r0
    call set_irq_mask
    sti
    li 0x1 r1
    call wait_for_count
    cli
    # The handler saw "a byte has arrived", read the byte, and saw the status
    # go back to zero.
    cmp 0x1 r3
    jne fail
    cmp 0x1c r4
    jne fail
    cmp 0x0 r2
    jne fail

    # The second byte.
    li 0x3 r7
    sti
    li 0x2 r1
    call wait_for_count
    cli
    cmp 0x1 r3
    jne fail
    cmp 0x32 r4
    jne fail

    # Two bytes arrive while interrupts are off, and nobody reads the first
    # one: the status shows both bits, and the data register has the second
    # byte.
    li 0x4 r7
    li 0x2 r0
    call wait_for_status
    li 0x64 r1
    inb r1 r0
    cmp 0x3 r0
    jne fail
    li 0x60 r1
    inb r1 r0
    cmp 0x44 r0
    jne fail
    # The read has cleared "a byte has arrived" and has kept "a byte was lost".
    li 0x64 r1
    inb r1 r0
    cmp 0x2 r0
    jne fail
    # A write without the bit 0x2 clears nothing; a write with it clears it.
    li 0x1 r0
    outb r0 r1
    inb r1 r0
    cmp 0x2 r0
    jne fail
    li 0x2 r0
    outb r0 r1
    inb r1 r0
    cmp 0x0 r0
    jne fail

    # The interrupt controller still remembers the request. With interrupts
    # on the handler runs once and finds that no byte is waiting.
    li 0x5 r7
    sti
    cli
    cmp 0x3 r6
    jne fail
    cmp 0x0 r3
    jne fail

    # The timer (line 0) and the keyboard (line 1) both request an interrupt
    # while interrupts are off. The lower line goes first.
    li 0x6 r7
    li 0xfc r0
    call set_irq_mask
    li 0x40 r0
    call set_timer
    li 0x1 r0
    call wait_for_status
    # By now the timer has requested its interrupt too; no more of them.
    li 0xffff r0
    call set_timer
    li 0x0 r5
    sti
    cli
    cmp 0x12 r5
    jne fail
    cmp 0x55 r4
    jne fail

    li 0x600d r0
    hlt

fail:
    cli
    mov r7 r0
    hlt

# Waits until the number of keyboard interrupts is the number in `r1`.
wait_for_count:
    li 0x4e20 r0
_wait_for_count_next:
    cmp r1 r6
    je _wait_for_count_end
    sub 0x1 r0
    jne _wait_for_count_next
    jmp fail
_wait_for_count_end:
    ret

# Waits until the status of the keyboard controller has the bits in `r0`.
wait_for_status:
    li 0x64 r1
    li 0x4e20 r2
_wait_for_status_next:
    inb r1 r3
    and r0 r3
    cmp r0 r3
    je _wait_for_status_end
    sub 0x1 r2
    jne _wait_for_status_next
    jmp fail
_wait_for_status_end:
    ret

# Sets the mask of the interrupt controller to the low byte of `r0`.
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

on_timer:
    shl 0x4 r5
    or 0x1 r5
    iret

on_keyboard:
    add 0x1 r6
    shl 0x4 r5
    or 0x2 r5
    push r0
    li 0x64 r0
    inb r0 r3
    li 0x60 r0
    inb r0 r4
    li 0x64 r0
    inb r0 r2
    pop r0
    iret

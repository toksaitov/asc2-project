# Interrupts: the interrupt controller and the timer.
#
# The timer counts down from its period and requests an interrupt on line 0
# of the interrupt controller each time it reaches the end. The controller
# has 8 lines. A request is remembered until it is delivered, also while its
# line is masked. The CPU takes a request between two instructions if
#
# * the line is not masked in the controller,
# * the interrupt flag (0x10 in `flags`) is set, and
# * the CPU is not inside a handler that has not saved its trap record yet.
#
# The handler of line n is the vector 8 + n unless the kernel moves the lines
# to other vectors. The handler runs with interrupts off and returns with
# `iret` to the instruction that was about to run.
#
# The I/O ports:
#
#     0x20  controller, command: 0 = the next data byte is the mask,
#                                1 = the next data byte is the first vector
#     0x21  controller, data; reads as the mask (1 = the line is masked)
#     0x43  timer, command: 0 = the next data byte is the high byte of the
#                               period,
#                           1 = the next data byte is the low byte, and the
#                               timer starts over with the new period
#     0x40  timer, data; reads as the low byte of the counter
#
# How long a step of the timer takes differs between the emulator and the
# chip, so the test never counts on exact times: it waits in loops with a
# limit for things that must happen, and it stops the timer (by giving it the
# longest period) where an extra interrupt would spoil a count.
#
# The handlers use these registers:
#
# * `r6`: the number of timer interrupts so far;
# * `r4`, `r3`, `r2`: the cause (`cr5`), the saved flags (`cr4`), and the
#   saved address (`cr1`) of the last timer interrupt;
# * `fp`: the flags inside the handler of the last timer interrupt;
# * `r5`: the number of timer interrupts on the moved vector.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_int r1
    sr r1 24(r0)
    li on_timer r1
    sr r1 32(r0)
    li on_timer_moved r1
    sr r1 0x80(r0)

    li 0x0 r6
    li 0x0 r5

    # After a reset every line is masked: the mask reads as 0xff.
    li 0x1 r7
    li 0x21 r1
    inb r1 r0
    cmp 0xff r0
    jne fail

    # Open line 0 and start the timer with a period of 20 steps. The mask
    # reads back.
    li 0x2 r7
    li 0xfe r0
    call set_irq_mask
    li 0x21 r1
    inb r1 r0
    cmp 0xfe r0
    jne fail
    li 0x14 r0
    call set_timer

    # With the interrupt flag set the handler runs again and again.
    li 0x3 r7
    sti
    li 0x2710 r0
_wait_3:
    cmp 0x3 r6
    jge _done_3
    sub 0x1 r0
    jne _wait_3
    jmp fail
_done_3:
    cli
    # The cause is the vector alone, and the saved flags have the interrupt
    # flag: `iret` turns interrupts on again.
    cmp 0x8 r4
    jne fail
    test 0x10 r3
    je fail
    # Inside the handler the interrupt flag was clear.
    test 0x10 fp
    jne fail
    mov flags r0
    test 0x10 r0
    jne fail

    # A handler changes the flags for itself; `iret` puts the flags of the
    # interrupted code back.
    li 0x4 r7
    sti
    li 0xc8 r0
_loop_4:
    li 0xf r1
    mov r1 flags
    nop
    nop
    mov flags r1
    cmp 0x1f r1
    jne fail
    sub 0x1 r0
    jne _loop_4
    cli

    # `hlt` waits for an interrupt. The handler returns to the instruction
    # after the `hlt`.
    li 0x5 r7
    li 0x0 r6
    sti
    hlt
_after_hlt:
    cli
    cmp 0x0 r6
    je fail
    li _after_hlt r0
    cmp r0 r2
    jne fail

    # A request that arrives while the interrupt flag is clear is delivered
    # once the flag is set, and only once however many periods have passed.
    li 0x6 r7
    li 0x0 r6
    li 0xc8 r0
_loop_6:
    sub 0x1 r0
    jne _loop_6
    cmp 0x0 r6
    jne fail
    call stop_timer
    sti
    cli
    cmp 0x1 r6
    jne fail

    # The same with a masked line and the interrupt flag set.
    li 0x7 r7
    li 0x0 r6
    li 0xff r0
    call set_irq_mask
    li 0x14 r0
    call set_timer
    sti
    li 0xc8 r0
_loop_7:
    sub 0x1 r0
    jne _loop_7
    cmp 0x0 r6
    jne fail
    cli
    call stop_timer
    sti
    li 0xfe r0
    call set_irq_mask
    cli
    cmp 0x1 r6
    jne fail

    # The lines can be moved to other vectors: line 0 to the vector 0x20.
    li 0x8 r7
    li 0x0 r6
    li 0x20 r0
    call set_irq_offset
    li 0x14 r0
    call set_timer
    sti
    li 0x2710 r0
_wait_8:
    cmp 0x2 r5
    jge _done_8
    sub 0x1 r0
    jne _wait_8
    jmp fail
_done_8:
    cli
    cmp 0x0 r6
    jne fail
    li 0x8 r0
    call set_irq_offset

    # The counter of the timer can be read: it stays between 1 and the period
    # (20) and it moves.
    li 0x9 r7
    li 0x40 r1
    inb r1 r2
    li 0x10 r3
    li 0x0 r4
_loop_9:
    inb r1 r0
    cmp 0x1 r0
    jl fail
    cmp 0x14 r0
    jg fail
    cmp r2 r0
    je _same_9
    li 0x1 r4
_same_9:
    sub 0x1 r3
    jne _loop_9
    cmp 0x1 r4
    jne fail

    # Only the low byte of a port number counts, a port with no device reads
    # as zero, and a write to it does nothing.
    li 0xa r7
    li 0x121 r1
    inb r1 r0
    cmp 0xfe r0
    jne fail
    li 0x99 r1
    li 0x55 r0
    outb r0 r1
    inb r1 r0
    cmp 0x0 r0
    jne fail

    # Interrupts wait while a handler has not saved its trap record, even if
    # the handler sets the interrupt flag. The handler of `int` checks that
    # and leaves 1 in `fp`.
    li 0xb r7
    li 0x0 r6
    li 0x0 fp
    li 0x14 r0
    call set_timer
    int 0x1
    cmp 0x1 fp
    jne fail
    mov flags r0
    test 0x10 r0
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

# Sets the vector of line 0 to the low byte of `r0`; the other lines follow.
set_irq_offset:
    li 0x20 r1
    li 0x1 r2
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

# Gives the timer the longest period, so that no new request arrives for a
# long while. A request that is already remembered stays.
stop_timer:
    push lr
    li 0xffff r0
    call set_timer
    pop lr
    ret

on_timer:
    mov flags fp
    add 0x1 r6
    mov cr5 r4
    mov cr4 r3
    mov cr1 r2
    iret

on_timer_moved:
    add 0x1 r5
    iret

on_int:
    push r0
    push r1

    # Save the trap record: this handler is about to take an interrupt.
    mov cr1 r0
    push r0
    mov cr4 r0
    push r0
    mov cr5 r0
    push r0

    # The interrupt flag is set, but T (0x8 in `cr0`) still is too: the timer
    # has to wait.
    sti
    li 0xc8 r0
_on_int_loop:
    sub 0x1 r0
    jne _on_int_loop
    cmp 0x0 r6
    jne _on_int_end

    # With T clear the waiting request is delivered at once.
    mov cr0 r0
    and -9 r0
    mov r0 cr0
    nop
    cli
    cmp 0x0 r6
    je _on_int_end
    li 0x1 fp

_on_int_end:
    cli
    pop r0
    mov r0 cr5
    pop r0
    mov r0 cr4
    pop r0
    mov r0 cr1

    pop r1
    pop r0
    iret

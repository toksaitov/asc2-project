# A handler that takes another interrupt or exception.
#
# When the CPU enters a handler, it writes the trap record: the address to
# return to in `cr1`, the flags in `cr4`, the cause in `cr5`, and for some
# exceptions an address in `cr2`. There is one record. A second interrupt or
# exception would write over it, and the first handler could never return.
#
# So the CPU sets bit 3 (T) of `cr0` when it enters a handler and clears it
# on `iret`. While T is set, interrupts wait, and an exception stops the CPU
# for good (see `cpu_double_fault.s`).
#
# A handler that wants to take interrupts or may raise exceptions saves the
# record on its stack and clears T by itself. Before `iret` it puts the
# record back, with interrupts off.
#
# The handler of `int` here does that: `int 0x1` from the test enters it, and
# inside it runs `int 0x2` and an invalid instruction.
#
# `r6` collects what has run: 0x1 for the outer `int`, 0x10 for the inner
# `int`, 0x100 for the invalid instruction.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_invalid r1
    sr r1 0(r0)
    li on_int r1
    sr r1 24(r0)

    li 0x1 r7
    li 0x0 r6
    li 0x1111 r0
    li 0x2222 r1
    mov sp r5
    li 0xf r2
    mov r2 flags
    int 0x1
_after_int:
    # The flags are back, and so are the registers and the stack pointer.
    mov flags r2
    cmp 0xf r2
    jne fail
    cmp 0x1111 r0
    jne fail
    cmp 0x2222 r1
    jne fail
    cmp r5 sp
    jne fail

    # All three have run.
    li 0x2 r7
    cmp 0x111 r6
    jne fail

    # T is clear (`cr0` has the F bit alone, as after the reset), and the
    # trap record is the one of the outer `int` again.
    li 0x3 r7
    mov cr0 r0
    cmp 0x4 r0
    jne fail
    mov cr1 r0
    li _after_int r1
    cmp r1 r0
    jne fail
    mov cr5 r0
    cmp 0x106 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

on_int:
    push r0

    # The number of the `int` is in bits 15-8 of the cause.
    mov cr5 r0
    shr 0x8 r0
    cmp 0x2 r0
    je _on_int_inner

    # Save the trap record.
    mov cr1 r0
    push r0
    mov cr4 r0
    push r0
    mov cr5 r0
    push r0

    # Clear T (0x8). From here on the CPU can enter another handler.
    mov cr0 r0
    and -9 r0
    mov r0 cr0

    add 0x1 r6
    int 0x2
    # A number that is not an instruction.
    .long 0x000000ff

    # Put the trap record back. Interrupts are off in this handler; a handler
    # that has turned them on turns them off before this point.
    pop r0
    mov r0 cr5
    pop r0
    mov r0 cr4
    pop r0
    mov r0 cr1

    pop r0
    iret

_on_int_inner:
    add 0x10 r6
    pop r0
    iret

on_invalid:
    add 0x100 r6
    push r0
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret

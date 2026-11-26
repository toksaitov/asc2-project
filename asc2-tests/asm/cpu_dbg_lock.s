# The lock of the debug port (`cr8`).
#
# The debug port shows the registers of the CPU to whoever holds its three
# wires. A kernel that does not want this writes the key 0x4c4f434b, the
# letters "LOCK", to `cr8`. The port then shows that it is locked and
# nothing else, until the next reset. `cr8` reads as 1 while the port is
# locked and as 0 otherwise.
#
# The lock must never close by accident, so only the whole key closes it: a
# value that differs from the key in one bit changes nothing. And nothing
# opens it but a reset: no later write does. The moves of `cr8` are
# privileged as the moves of every control register are.
#
# What the port shows is checked on the chip (`../chip/Makefile`); the
# emulator has no debug port.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp
    mov sp fp

    li 0x80000000 r0
    li on_privileged r1
    sr r1 4(r0)
    li on_int r1
    sr r1 24(r0)

    li 0x0 r6

    # A reset leaves the port open.
    li 0x1 r7
    mov cr8 r0
    cmp 0x0 r0
    jne fail

    # A user program can neither read `cr8` nor lock the port.
    li 0x2 r7
    li _back_2 r5
    li user_2 r0
    jmp enter_user
user_2:
    li 0x4c4f434b r1
    li 0x55 r2
    mov r1 cr8
    mov cr8 r2
    int 0x80
_back_2:
    cmp 0x2 r6
    jne fail
    cmp 0x55 r2
    jne fail
    mov cr8 r0
    cmp 0x0 r0
    jne fail

    # Values that are not the key: 0, 1, -1, and the key with one bit wrong,
    # for each of its 32 bits.
    li 0x3 r7
    li 0x0 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x0 r0
    jne fail
    li 0x1 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x0 r0
    jne fail
    li -1 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x0 r0
    jne fail
    li 0x1 r4
    li 0x20 r3
_wrong:
    li 0x4c4f434b r1
    xor r4 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x0 r0
    jne fail
    shl 0x1 r4
    sub 0x1 r3
    jne _wrong

    # An open port shows the bits C (0x10) and A (0x20) of `cr0`. Both are
    # set for sixteen instructions that run once each, so the instruction
    # cache has nothing to give back, and the test on the chip reads the
    # port while they are set.
    li 0x4 r7
    li 0x34 r0
    mov r0 cr0
    li 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    add 0x1 r3
    li 0x4 r0
    mov r0 cr0
    cmp 0xf r3
    jne fail

    # The key locks the port.
    li 0x5 r7
    li 0x4c4f434b r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x1 r0
    jne fail

    # No write opens it: not 0, not 1, not the key once more, not the key
    # with every bit turned.
    li 0x6 r7
    li 0x0 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x1 r0
    jne fail
    li 0x1 r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x1 r0
    jne fail
    li 0x4c4f434b r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x1 r0
    jne fail
    not r1
    mov r1 cr8
    mov cr8 r0
    cmp 0x1 r0
    jne fail

    # The lock touches nothing else: `cr0` is as the reset has left it, with
    # the F bit (0x4) alone, and the CPU computes as before.
    li 0x7 r7
    mov cr0 r0
    cmp 0x4 r0
    jne fail
    li 0x7 r3
    mul 0x6 r3
    cmp 0x2a r3
    jne fail

    # The lock hides `cr0` as well. The A bit (0x20) is set from here on:
    # an open port would show it, and the test on the chip reads the locked
    # port while it is set.
    li 0x8 r7
    li 0x24 r0
    mov r0 cr0
    mov cr0 r0
    cmp 0x24 r0
    jne fail
    li 0x9 r3
    mul 0x5 r3
    cmp 0x2d r3
    jne fail
    mov cr8 r0
    cmp 0x1 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Enters user mode at the kernel address in `r0`.
enter_user:
    li 0x80000000 r1
    sub r1 r0
    mov r0 cr1
    li 0x20 r0
    mov r0 cr4
    iret

# Continues the test in kernel mode at the address in `r5`.
on_int:
    mov r5 cr1
    push r0
    li 0x0 r0
    mov r0 cr4
    pop r0
    iret

# Counts the exception and returns to the instruction after the refused one.
on_privileged:
    add 0x1 r6
    push r0
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret

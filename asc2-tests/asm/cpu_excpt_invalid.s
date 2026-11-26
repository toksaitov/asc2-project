# The invalid instruction exception (vector 0).
#
# A 32-bit number is not an instruction if no instruction has its opcode, if
# it names a register that does not exist, or if a field that its instruction
# does not use is not zero. The CPU then changes nothing, saves the address of
# the number in `cr1` and the flags in `cr4`, and calls the handler whose
# address is in the vector table at physical address 4 * 0.
#
# The handler here counts the exceptions in `r6`, leaves the saved address in
# `r5`, and returns to the instruction after the bad number.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    # The vector table starts at physical address 0. The kernel reaches
    # physical memory at 0x80000000 and above.
    li 0x80000000 r0
    li on_invalid r1
    sr r1 0(r0)

    li 0x0 r6

    # An opcode that no instruction has. This first check looks at everything
    # the exception does.
    li 0x1 r7
    li 0x1111 r0
    li 0x2222 r1
    li 0x3333 r2
    li 0x4444 r3
    li 0xf r4
    mov r4 flags
_bad_1:
    .long 0x000000ff
    # The flags are back, and the handler has returned to this instruction.
    mov flags r4
    cmp 0xf r4
    jne fail
    # The registers that the handler does not use are untouched.
    cmp 0x1111 r0
    jne fail
    cmp 0x2222 r1
    jne fail
    # The handler stored what it saw in `r2` (the control register `cr0`
    # inside the handler) and in `r3` (the flags inside the handler). In
    # `cr0` the T bit (0x8) was set next to the F bit (0x4), which is set
    # from the reset on.
    cmp 0xc r2
    jne fail
    cmp 0xf r3
    jne fail
    # `iret` has cleared the T bit of `cr0`.
    mov cr0 r0
    cmp 0x4 r0
    jne fail
    # The saved flags and the cause are still in the trap record.
    mov cr4 r0
    cmp 0xf r0
    jne fail
    mov cr5 r0
    cmp 0x0 r0
    jne fail
    li _bad_1 r1
    call check

    # The opcode of `inb` with the immediate bit: `inb` has no immediate form.
    li 0x2 r7
_bad_2:
    .long 0x00001286
    li _bad_2 r1
    call check

    # `mov r13 r0`: there are 13 registers, with the numbers 0-12.
    li 0x3 r7
    li 0x1111 r0
_bad_3:
    .long 0x0000d010
    cmp 0x1111 r0
    jne fail
    li _bad_3 r1
    call check

    # `mov r0 r15`.
    li 0x4 r7
_bad_4:
    .long 0x00000f10
    li _bad_4 r1
    call check

    # `nop` with a number in the immediate field.
    li 0x5 r7
_bad_5:
    .long 0x00010001
    li _bad_5 r1
    call check

    # `hlt` with a register in a field it does not use. `hlt` is the opcode 0,
    # so the test would stop here if the CPU took this number for a `hlt`.
    li 0x6 r7
_bad_6:
    .long 0x00000100
    li _bad_6 r1
    call check

    # `ret` with a register.
    li 0x7 r7
_bad_7:
    .long 0x00001032
    li _bad_7 r1
    call check

    # `int 256`: the number of an `int` has 8 bits.
    li 0x8 r7
_bad_8:
    .long 0x01000085
    li _bad_8 r1
    call check

    # `dot4 r1 r2 r13`.
    li 0x9 r7
_bad_9:
    .long 0x000d122e
    li _bad_9 r1
    call check

    # `mov cr9 r0`: there are 9 control registers, with the numbers 0-8.
    li 0xa r7
_bad_10:
    .long 0x00009008
    li _bad_10 r1
    call check

    # `mov r0 cr9`.
    li 0xb r7
_bad_11:
    .long 0x00000909
    li _bad_11 r1
    call check

    # `add r1 r0` with a number in the immediate field.
    li 0xc r7
    li 0x1 r0
_bad_12:
    .long 0x00011020
    cmp 0x1 r0
    jne fail
    li _bad_12 r1
    call check

    # `pop r0` with a register in the field of the first operand.
    li 0xd r7
    mov sp r0
_bad_13:
    .long 0x00001013
    cmp r0 sp
    jne fail
    li _bad_13 r1
    call check

    # `li 0 r0` with a register in the field of the first operand.
    li 0xe r7
_bad_14:
    .long 0x00001090
    li _bad_14 r1
    call check

    # The opcode of `lih` without the immediate bit: `lih` has no register form.
    li 0xf r7
_bad_15:
    .long 0x00000011
    li _bad_15 r1
    call check

    # The opcode of `int` without the immediate bit.
    li 0x10 r7
_bad_16:
    .long 0x00000005
    li _bad_16 r1
    call check

    # A conditional jump with a condition number that does not exist (10).
    li 0x11 r7
_bad_17:
    .long 0x0000004a
    li _bad_17 r1
    call check

    # `dot4 r1 r2 r2` with a bit set above the number of the third register:
    # the whole field must be the number of a register.
    li 0x12 r7
_bad_18:
    .long 0x0012122e
    li _bad_18 r1
    call check

    # `dot4 r1 r2 r3` with the bit of the opcode that marks a form with a
    # number in place of a register: `dot4` has no such form.
    li 0x13 r7
_bad_19:
    .long 0x000312ae
    li _bad_19 r1
    call check

    # `mov r0 w3`: the array has three rows, so its registers are `w0` to
    # `w2` and `x0` to `x2`.
    li 0x14 r7
_bad_20:
    .long 0x00000361
    li _bad_20 r1
    call check

    # `mov x3 r0`.
    li 0x15 r7
_bad_21:
    .long 0x00007060
    li _bad_21 r1
    call check

    # A move to the register of the array with the number 8: the numbers of
    # the weights are 0-3 and those of the inputs 4-7.
    li 0x16 r7
_bad_22:
    .long 0x00000861
    li _bad_22 r1
    call check

    # `dotm r6`: the three rows would add to `r6`, `r7`, and `fp`, and every
    # row must add to a register from `r0` to `r7`.
    li 0x17 r7
_bad_23:
    .long 0x00000663
    li _bad_23 r1
    call check

    # `dotv r13`.
    li 0x18 r7
_bad_24:
    .long 0x00000d62
    li _bad_24 r1
    call check

    # `dotv r1` with a number in the field of the other register.
    li 0x19 r7
_bad_25:
    .long 0x00001162
    li _bad_25 r1
    call check

    # `learn r1` with a number in the immediate field.
    li 0x1a r7
_bad_26:
    .long 0x00011064
    li _bad_26 r1
    call check

    # `xsh` and `popcnt` with a number in place of the first register: they
    # have no such form.
    li 0x1b r7
_bad_27:
    .long 0x000100e5
    li _bad_27 r1
    call check
    li 0x1c r7
_bad_28:
    .long 0x000101af
    li _bad_28 r1
    call check

    # `ssat r1` with a number in the field of the other register.
    li 0x1d r7
_bad_29:
    .long 0x00001166
    li _bad_29 r1
    call check

    # The opcode after `usat`.
    li 0x1e r7
_bad_30:
    .long 0x00000068
    li _bad_30 r1
    call check

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Checks that the last exception was raised at the address in `r1` and that
# every check so far has raised exactly one exception.
check:
    cmp r1 r5
    jne fail
    cmp r7 r6
    jne fail
    ret

on_invalid:
    mov cr0 r2
    mov flags r3
    add 0x1 r6
    mov cr1 r5
    # Without this the handler would return to the bad number again.
    mov r5 r4
    add 0x4 r4
    mov r4 cr1
    iret

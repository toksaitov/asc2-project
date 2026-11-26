# Shifts: `shl`, `shr`, `sar`, and their flags.
#
# The lowest four bits of `flags` are C (0x1), Z (0x2), S (0x4), and O (0x8).
# C is the last bit shifted out.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    # `shl` by one moves the top bit into C.
    li 0x1 r7
    li 0x80000001 r0
    shl 0x1 r0
    mov flags r2
    cmp 0x9 r2
    jne fail
    cmp 0x2 r0
    jne fail

    # `shl` by four.
    li 0x2 r7
    li 0x0fffffff r0
    shl 0x4 r0
    mov flags r2
    cmp 0xc r2
    jne fail
    cmp -16 r0
    jne fail

    # `shl` until nothing is left.
    li 0x3 r7
    li 0x2 r0
    shl 0x1f r0
    mov flags r2
    cmp 0xb r2
    jne fail
    cmp 0x0 r0
    jne fail

    # `shr` fills with zeros.
    li 0x4 r7
    li 0x80000001 r0
    shr 0x1 r0
    mov flags r2
    cmp 0x9 r2
    jne fail
    li 0x40000000 r2
    cmp r2 r0
    jne fail

    # `sar` fills with the sign bit.
    li 0x5 r7
    li 0x80000001 r0
    sar 0x1 r0
    mov flags r2
    cmp 0x5 r2
    jne fail
    li 0xc0000000 r2
    cmp r2 r0
    jne fail

    # `sar` by 31 leaves only copies of the sign bit.
    li 0x6 r7
    li 0x80000000 r0
    sar 0x1f r0
    mov flags r2
    cmp 0x4 r2
    jne fail
    cmp -1 r0
    jne fail

    # `sar` of a positive number fills with zeros.
    li 0x7 r7
    li 0x40000000 r0
    sar 0x4 r0
    mov flags r2
    cmp 0x0 r2
    jne fail
    li 0x04000000 r2
    cmp r2 r0
    jne fail

    # A zero shift changes neither the register nor the flags.
    li 0x8 r7
    li 0xf r1
    mov r1 flags
    li 0x1234 r0
    shl 0x0 r0
    shr 0x0 r0
    sar 0x0 r0
    mov flags r2
    cmp 0xf r2
    jne fail
    cmp 0x1234 r0
    jne fail

    # Only the low five bits of the count are used: 32 shifts by nothing, 33 by one.
    li 0x9 r7
    li 0xf r1
    mov r1 flags
    li 0x1234 r0
    li 0x20 r1
    shl r1 r0
    mov flags r2
    cmp 0xf r2
    jne fail
    cmp 0x1234 r0
    jne fail
    li 0x21 r1
    shl r1 r0
    cmp 0x2468 r0
    jne fail

    # The count can come from a register.
    li 0xa r7
    li 0xf0 r0
    li 0x4 r1
    shr r1 r0
    cmp 0xf r0
    jne fail
    li -256 r0
    sar r1 r0
    cmp -16 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

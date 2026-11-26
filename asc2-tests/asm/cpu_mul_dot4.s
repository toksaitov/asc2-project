# Multiplication: `mul` and `dot4`.
#
# `dot4 ra rb rd` treats `ra` and `rb` as four signed bytes each, multiplies
# the bytes in the same positions, and adds the four products to `rd`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    # A small product.
    li 0x1 r7
    li 0x6 r0
    li 0x7 r1
    mul r1 r0
    cmp 0x2a r0
    jne fail

    # The low 32 bits are right for negative numbers too.
    li 0x2 r7
    li -3 r0
    li 0x5 r1
    mul r1 r0
    cmp -15 r0
    jne fail
    li -1 r0
    mul r0 r0
    cmp 0x1 r0
    jne fail

    # The bits above bit 31 are lost.
    li 0x3 r7
    li 0x10000 r0
    mul r0 r0
    cmp 0x0 r0
    jne fail
    li 0x12345 r0
    li 0x10001 r1
    mul r1 r0
    li 0x23462345 r2
    cmp r2 r0
    jne fail

    # `mul` takes an immediate value and leaves the flags alone.
    li 0x4 r7
    li 0xf r1
    mov r1 flags
    li 0x64 r0
    mul -2 r0
    mov flags r2
    cmp 0xf r2
    jne fail
    cmp -200 r0
    jne fail

    # 1*5 + 2*6 + 3*7 + 4*8 = 70, added to 100.
    li 0x5 r7
    li 0x04030201 r1
    li 0x08070605 r2
    li 0x64 r3
    dot4 r1 r2 r3
    cmp 0xaa r3
    jne fail

    # Bytes are signed: (-1)*10 + 2*(-20) + (-3)*30 + 4*(-40) = -300.
    li 0x6 r7
    li 0x04fd02ff r1
    li 0xd81eec0a r2
    li 0x0 r3
    dot4 r1 r2 r3
    cmp -300 r3
    jne fail

    # The largest sum: four times (-128)*(-128) is 65536.
    li 0x7 r7
    li 0x80808080 r1
    li 0x0 r3
    dot4 r1 r1 r3
    li 0x10000 r2
    cmp r2 r3
    jne fail

    # The sum wraps around at 32 bits.
    li 0x8 r7
    li 0x04030201 r1
    li 0x08070605 r2
    li -1 r3
    dot4 r1 r2 r3
    cmp 0x45 r3
    jne fail

    # All three registers are read before the destination is written.
    li 0x9 r7
    li 0x01010101 r1
    dot4 r1 r1 r1
    li 0x01010105 r2
    cmp r2 r1
    jne fail

    # `dot4` leaves the flags alone.
    li 0xa r7
    li 0xf r1
    mov r1 flags
    li 0x04030201 r1
    li 0x0 r3
    dot4 r1 r1 r3
    mov flags r2
    cmp 0xf r2
    jne fail
    cmp 0x1e r3
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

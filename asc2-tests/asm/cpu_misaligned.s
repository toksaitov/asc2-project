# The misaligned address exception (vector 2).
#
# A 4-byte access needs an address that is a multiple of 4, a 2-byte access an
# even address. An instruction is 4 bytes, so its address is a multiple of 4
# too. The CPU checks this before anything else about the address, changes
# nothing, and puts the address in `cr2`.
#
# The cause in `cr5` has the vector in bits 7-0, 0x20000 for a write, and
# 0x80000 for a fetch.
#
# The handler counts the exceptions in `r6` and leaves the cause in `r4` and
# the address in `r3`. After a read or a write it returns to the next
# instruction; after a fetch it continues the test at the address in `r5`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
words:
    .long 0x44332211
    .long 0x88776655

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_misaligned r1
    sr r1 8(r0)
    # A bus error (vector 5) would mean that the CPU has looked at the range
    # of the address before its alignment.
    li fail r1
    sr r1 20(r0)

    li 0x0 r6
    li words r2

    # A 4-byte load from an address that is one byte off.
    li 0x1 r7
    li 0x55 r0
    lr 1(r2) r0
    cmp 0x1 r6
    jne fail
    cmp 0x2 r4
    jne fail
    mov r2 r1
    add 0x1 r1
    cmp r1 r3
    jne fail
    # The destination register keeps its value.
    cmp 0x55 r0
    jne fail

    # A 4-byte load from an address that is two and three bytes off.
    li 0x2 r7
    lr 2(r2) r0
    lr 3(r2) r0
    cmp 0x3 r6
    jne fail

    # A 4-byte store. The memory keeps its value.
    li 0x3 r7
    li 0x0 r6
    li -1 r0
    sr r0 2(r2)
    cmp 0x1 r6
    jne fail
    li 0x20002 r1
    cmp r1 r4
    jne fail
    lr 0(r2) r0
    li 0x44332211 r1
    cmp r1 r0
    jne fail
    lr 4(r2) r0
    li 0x88776655 r1
    cmp r1 r0
    jne fail

    # 2-byte loads and stores at odd addresses.
    li 0x4 r7
    li 0x0 r6
    lrh 1(r2) r0
    lrhu 3(r2) r0
    srh r0 1(r2)
    cmp 0x3 r6
    jne fail

    # 2-byte loads at even addresses and 1-byte loads at any address are fine.
    li 0x5 r7
    li 0x0 r6
    lrhu 2(r2) r0
    li 0x4433 r1
    cmp r1 r0
    jne fail
    lrbu 1(r2) r0
    cmp 0x22 r0
    jne fail
    lrbu 3(r2) r0
    cmp 0x44 r0
    jne fail
    srb r0 3(r2)
    cmp 0x0 r6
    jne fail

    # `push` and `pop` with a stack pointer that is not a multiple of 4. The
    # stack pointer keeps its value.
    li 0x6 r7
    li 0x0 r6
    mov sp r1
    add 0x2 sp
    push r0
    pop r0
    sub 0x2 sp
    cmp 0x2 r6
    jne fail
    cmp r1 sp
    jne fail

    # A misaligned address outside every address range is still reported as
    # misaligned.
    li 0x7 r7
    li 0x0 r6
    li 0x90000001 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    cmp 0x2 r4
    jne fail
    cmp r1 r3
    jne fail

    # A jump to an address that is not a multiple of 4. The exception is
    # raised when the CPU fetches from the address, so the saved address and
    # the fault address are both the target of the jump.
    li 0x8 r7
    li 0x0 r6
    li _back_8 r5
    li _back_8 r0
    add 0x2 r0
    jmp r0
_back_8:
    cmp 0x1 r6
    jne fail
    li 0x80002 r0
    cmp r0 r4
    jne fail
    mov r5 r0
    add 0x2 r0
    cmp r0 r3
    jne fail
    cmp r0 r1
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# The handler keeps its own values in `fp` and does not touch the stack: some
# checks give it a stack pointer that cannot be used.
on_misaligned:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    mov r4 fp
    shr 0x13 fp
    test 0x1 fp
    jne _on_misaligned_fetch
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret
_on_misaligned_fetch:
    mov cr1 r1
    mov r5 cr1
    iret

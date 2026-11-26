# Data movement: `li`, `lih`, `mov`, `push`, `pop`, the loads, and the stores.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
word:
    .long 0x11223344
bytes:
    .byte 0x80
    .byte 0x7f
half:
    .short 0x8001
scratch:
    .long 0x0
    .long 0x0

.section .text
main:
    li 0x80020000 sp

    # A small value takes one instruction.
    li 0x1 r7
    li 0x1234 r0
    cmp 0x1234 r0
    jne fail

    # A negative value fills the upper half with ones.
    li 0x2 r7
    li -2 r0
    shr 0x10 r0
    li 0xffff r1
    cmp r1 r0
    jne fail

    # A wide value takes `li` and `lih`.
    li 0x3 r7
    li 0x12345678 r0
    mov r0 r1
    shr 0x10 r1
    cmp 0x1234 r1
    jne fail
    mov r0 r1
    shl 0x10 r1
    shr 0x10 r1
    cmp 0x5678 r1
    jne fail

    # The lower half 0x8000 must not leave ones in the upper half.
    li 0x4 r7
    li 0x8000 r0
    shr 0xf r0
    cmp 0x1 r0
    jne fail

    # `lih` replaces the upper half and keeps the lower half.
    li 0x5 r7
    li -1 r0
    lih 0x0 r0
    add 0x1 r0
    shr 0x10 r0
    cmp 0x1 r0
    jne fail

    # `mov` copies a register, and none of the loads changes the flags.
    li 0x6 r7
    li 0x55 r0
    cmp r0 r0
    mov r0 r1
    li 0x12345678 r2
    lih 0x4321 r2
    jne fail
    cmp 0x55 r1
    jne fail

    # `push` and `pop` work in the opposite order.
    li 0x7 r7
    mov sp r3
    li 0x111 r0
    push r0
    li 0x222 r0
    push r0
    pop r1
    pop r2
    cmp 0x222 r1
    jne fail
    cmp 0x111 r2
    jne fail
    cmp r3 sp
    jne fail

    # `push` takes an immediate value too.
    li 0x8 r7
    push -5
    pop r0
    cmp -5 r0
    jne fail

    # `push sp` stores the value before the decrement.
    li 0x9 r7
    mov sp r3
    push sp
    pop r0
    cmp r3 r0
    jne fail

    # `pop sp` keeps the popped value.
    li 0xa r7
    mov sp r3
    li 0x80010000 r0
    push r0
    pop sp
    cmp r0 sp
    jne fail
    mov r3 sp

    # A 4-byte load, with and without the parentheses.
    li 0xb r7
    li word r1
    li 0x11223344 r2
    lr r1 r0
    cmp r2 r0
    jne fail
    lr 0x0(r1) r3
    cmp r2 r3
    jne fail

    # The bytes of a word are stored from the lowest one.
    li 0xc r7
    lrbu 0x0(r1) r0
    cmp 0x44 r0
    jne fail
    lrbu 0x3(r1) r0
    cmp 0x11 r0
    jne fail
    lrhu 0x2(r1) r0
    cmp 0x1122 r0
    jne fail

    # Narrow loads extend the sign or fill with zeros.
    li 0xd r7
    li bytes r1
    lrb 0x0(r1) r0
    cmp -128 r0
    jne fail
    lrbu 0x0(r1) r0
    cmp 0x80 r0
    jne fail
    lrb 0x1(r1) r0
    cmp 0x7f r0
    jne fail
    li half r1
    lrh 0x0(r1) r0
    cmp -32767 r0
    jne fail
    lrhu 0x0(r1) r0
    li 0x8001 r2
    cmp r2 r0
    jne fail

    # A negative displacement reaches below the base register.
    li 0xe r7
    li scratch r1
    li 0x11223344 r2
    lr -8(r1) r0
    cmp r2 r0
    jne fail

    # Narrow stores change only their own bytes.
    li 0xf r7
    li scratch r1
    li 0x55667788 r0
    sr r0 0x0(r1)
    sr r0 0x4(r1)
    li 0xaabbccdd r0
    srb r0 0x1(r1)
    srh r0 0x6(r1)
    lr 0x0(r1) r3
    li 0x5566dd88 r2
    cmp r2 r3
    jne fail
    lr 0x4(r1) r3
    li 0xccdd7788 r2
    cmp r2 r3
    jne fail

    # A store through a plain register uses no displacement.
    li 0x10 r7
    li 0x1 r0
    sr r0 r1
    lr r1 r3
    cmp 0x1 r3
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

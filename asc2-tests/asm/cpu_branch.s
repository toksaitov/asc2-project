# Conditions: the conditional jumps in both forms and the `set` instructions.
#
# `cmp b a` followed by `jl` jumps if `a` is less than `b`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li fail r6

    # Jumps to a label, 5 compared with 5.
    li 0x1 r7
    li 0x5 r0
    li 0x5 r1
    cmp r1 r0
    je _taken_1_e
    jmp fail
_taken_1_e:
    jne fail
    jl fail
    jge _taken_1_ge
    jmp fail
_taken_1_ge:
    jle _taken_1_le
    jmp fail
_taken_1_le:
    jg fail
    jb fail
    jae _taken_1_ae
    jmp fail
_taken_1_ae:
    jbe _taken_1_be
    jmp fail
_taken_1_be:
    ja fail

    # Jumps through a register, 5 compared with 5.
    li 0x2 r7
    li _taken_2_e r2
    je r2
    jmp fail
_taken_2_e:
    jne r6
    jl r6
    li _taken_2_ge r2
    jge r2
    jmp fail
_taken_2_ge:
    li _taken_2_le r2
    jle r2
    jmp fail
_taken_2_le:
    jg r6
    jb r6
    li _taken_2_ae r2
    jae r2
    jmp fail
_taken_2_ae:
    li _taken_2_be r2
    jbe r2
    jmp fail
_taken_2_be:
    ja r6

    # The `set` instructions, 5 compared with 5. They leave the flags alone.
    li 0x3 r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x199 r3
    jne fail

    # Jumps to a label, 3 compared with 5.
    li 0x4 r7
    li 0x3 r0
    li 0x5 r1
    cmp r1 r0
    je fail
    jne _taken_4_ne
    jmp fail
_taken_4_ne:
    jl _taken_4_l
    jmp fail
_taken_4_l:
    jge fail
    jle _taken_4_le
    jmp fail
_taken_4_le:
    jg fail
    jb _taken_4_b
    jmp fail
_taken_4_b:
    jae fail
    jbe _taken_4_be
    jmp fail
_taken_4_be:
    ja fail

    # Jumps through a register, 3 compared with 5.
    li 0x5 r7
    je r6
    li _taken_5_ne r2
    jne r2
    jmp fail
_taken_5_ne:
    li _taken_5_l r2
    jl r2
    jmp fail
_taken_5_l:
    jge r6
    li _taken_5_le r2
    jle r2
    jmp fail
_taken_5_le:
    jg r6
    li _taken_5_b r2
    jb r2
    jmp fail
_taken_5_b:
    jae r6
    li _taken_5_be r2
    jbe r2
    jmp fail
_taken_5_be:
    ja r6

    # The `set` instructions, 3 compared with 5. They leave the flags alone.
    li 0x6 r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x156 r3
    jne fail

    # Jumps to a label, 7 compared with 5.
    li 0x7 r7
    li 0x7 r0
    li 0x5 r1
    cmp r1 r0
    je fail
    jne _taken_7_ne
    jmp fail
_taken_7_ne:
    jl fail
    jge _taken_7_ge
    jmp fail
_taken_7_ge:
    jle fail
    jg _taken_7_g
    jmp fail
_taken_7_g:
    jb fail
    jae _taken_7_ae
    jmp fail
_taken_7_ae:
    jbe fail
    ja _taken_7_a
    jmp fail
_taken_7_a:

    # Jumps through a register, 7 compared with 5.
    li 0x8 r7
    je r6
    li _taken_8_ne r2
    jne r2
    jmp fail
_taken_8_ne:
    jl r6
    li _taken_8_ge r2
    jge r2
    jmp fail
_taken_8_ge:
    jle r6
    li _taken_8_g r2
    jg r2
    jmp fail
_taken_8_g:
    jb r6
    li _taken_8_ae r2
    jae r2
    jmp fail
_taken_8_ae:
    jbe r6
    li _taken_8_a r2
    ja r2
    jmp fail
_taken_8_a:

    # The `set` instructions, 7 compared with 5. They leave the flags alone.
    li 0x9 r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x2aa r3
    jne fail

    # Jumps to a label, -1 compared with 1: less as a signed number, above as an unsigned one.
    li 0xa r7
    li -1 r0
    li 0x1 r1
    cmp r1 r0
    je fail
    jne _taken_10_ne
    jmp fail
_taken_10_ne:
    jl _taken_10_l
    jmp fail
_taken_10_l:
    jge fail
    jle _taken_10_le
    jmp fail
_taken_10_le:
    jg fail
    jb fail
    jae _taken_10_ae
    jmp fail
_taken_10_ae:
    jbe fail
    ja _taken_10_a
    jmp fail
_taken_10_a:

    # Jumps through a register, -1 compared with 1: less as a signed number, above as an unsigned one.
    li 0xb r7
    je r6
    li _taken_11_ne r2
    jne r2
    jmp fail
_taken_11_ne:
    li _taken_11_l r2
    jl r2
    jmp fail
_taken_11_l:
    jge r6
    li _taken_11_le r2
    jle r2
    jmp fail
_taken_11_le:
    jg r6
    jb r6
    li _taken_11_ae r2
    jae r2
    jmp fail
_taken_11_ae:
    jbe r6
    li _taken_11_a r2
    ja r2
    jmp fail
_taken_11_a:

    # The `set` instructions, -1 compared with 1: less as a signed number, above as an unsigned one. They leave the flags alone.
    li 0xc r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x296 r3
    jne fail

    # Jumps to a label, 1 compared with -1: greater as a signed number, below as an unsigned one.
    li 0xd r7
    li 0x1 r0
    li -1 r1
    cmp r1 r0
    je fail
    jne _taken_13_ne
    jmp fail
_taken_13_ne:
    jl fail
    jge _taken_13_ge
    jmp fail
_taken_13_ge:
    jle fail
    jg _taken_13_g
    jmp fail
_taken_13_g:
    jb _taken_13_b
    jmp fail
_taken_13_b:
    jae fail
    jbe _taken_13_be
    jmp fail
_taken_13_be:
    ja fail

    # Jumps through a register, 1 compared with -1: greater as a signed number, below as an unsigned one.
    li 0xe r7
    je r6
    li _taken_14_ne r2
    jne r2
    jmp fail
_taken_14_ne:
    jl r6
    li _taken_14_ge r2
    jge r2
    jmp fail
_taken_14_ge:
    jle r6
    li _taken_14_g r2
    jg r2
    jmp fail
_taken_14_g:
    li _taken_14_b r2
    jb r2
    jmp fail
_taken_14_b:
    jae r6
    li _taken_14_be r2
    jbe r2
    jmp fail
_taken_14_be:
    ja r6

    # The `set` instructions, 1 compared with -1: greater as a signed number, below as an unsigned one. They leave the flags alone.
    li 0xf r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x16a r3
    jne fail

    # Jumps to a label, the smallest negative number compared with 1: the subtraction overflows.
    li 0x10 r7
    li 0x80000000 r0
    li 0x1 r1
    cmp r1 r0
    je fail
    jne _taken_16_ne
    jmp fail
_taken_16_ne:
    jl _taken_16_l
    jmp fail
_taken_16_l:
    jge fail
    jle _taken_16_le
    jmp fail
_taken_16_le:
    jg fail
    jb fail
    jae _taken_16_ae
    jmp fail
_taken_16_ae:
    jbe fail
    ja _taken_16_a
    jmp fail
_taken_16_a:

    # Jumps through a register, the smallest negative number compared with 1: the subtraction overflows.
    li 0x11 r7
    je r6
    li _taken_17_ne r2
    jne r2
    jmp fail
_taken_17_ne:
    li _taken_17_l r2
    jl r2
    jmp fail
_taken_17_l:
    jge r6
    li _taken_17_le r2
    jle r2
    jmp fail
_taken_17_le:
    jg r6
    jb r6
    li _taken_17_ae r2
    jae r2
    jmp fail
_taken_17_ae:
    jbe r6
    li _taken_17_a r2
    ja r2
    jmp fail
_taken_17_a:

    # The `set` instructions, the smallest negative number compared with 1: the subtraction overflows. They leave the flags alone.
    li 0x12 r7
    li 0x0 r3
    sete r2
    shl 0x0 r2
    or r2 r3
    cmp r1 r0
    setne r2
    shl 0x1 r2
    or r2 r3
    cmp r1 r0
    setl r2
    shl 0x2 r2
    or r2 r3
    cmp r1 r0
    setge r2
    shl 0x3 r2
    or r2 r3
    cmp r1 r0
    setle r2
    shl 0x4 r2
    or r2 r3
    cmp r1 r0
    setg r2
    shl 0x5 r2
    or r2 r3
    cmp r1 r0
    setb r2
    shl 0x6 r2
    or r2 r3
    cmp r1 r0
    setae r2
    shl 0x7 r2
    or r2 r3
    cmp r1 r0
    setbe r2
    shl 0x8 r2
    or r2 r3
    cmp r1 r0
    seta r2
    shl 0x9 r2
    or r2 r3
    cmp r1 r0
    cmp 0x296 r3
    jne fail

    # A jump backward.
    li 0x13 r7
    li 0x0 r0
_loop:
    add 0x1 r0
    cmp 0x5 r0
    jl _loop
    cmp 0x5 r0
    jne fail

    # `jmp` through a register.
    li 0x14 r7
    li _jmp_target r2
    jmp r2
    jmp fail
_jmp_target:

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

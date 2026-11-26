# Subroutines: `call`, `ret`, and the calling convention.
#
# The calling convention:
#
# * The caller pushes the arguments, the last one first, and removes them
#   after the call.
# * `call` leaves the return address in `lr`. A routine that calls another one
#   saves `lr` on the stack first.
# * After `push lr`, `push fp`, `mov sp fp` the first argument is at `8(fp)`.
# * The result is returned in `r0`.
# * A routine may change `r0`-`r3`. It must restore `r4`-`r7` and `fp`.
#
# `mul_add` is written twice: as numbers encoded by hand from the instruction
# format, and as assembly. The test calls the first one and checks that the
# assembler has produced the same numbers for the second one.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    # `call` leaves the address of the next instruction in `lr`.
    li 0x1 r7
    li _after_call r1
    call leaf
_after_call:
    cmp r1 lr
    jne fail
    cmp 0x5 r0
    jne fail

    # `call` through a register.
    li 0x2 r7
    li 0x0 r0
    li leaf r2
    call r2
    cmp 0x5 r0
    jne fail

    # The routine encoded by hand: 6 * 7 + 6 is 48.
    li 0x3 r7
    li 0x1234 r4
    li 0x5678 fp
    mov sp r5
    push 0x7
    push 0x6
    call mul_add
    add 0x8 sp
    cmp 0x30 r0
    jne fail

    # The routine has restored `r4`, `fp`, and the stack pointer.
    li 0x4 r7
    cmp 0x1234 r4
    jne fail
    cmp 0x5678 fp
    jne fail
    cmp r5 sp
    jne fail

    # The arguments are still where the caller pushed them.
    li 0x5 r7
    lr -8(sp) r0
    cmp 0x6 r0
    jne fail
    lr -4(sp) r0
    cmp 0x7 r0
    jne fail

    # The assembler encodes the same routine into the same numbers.
    li 0x6 r7
    li mul_add r1
    li mul_add_asm r2
    li mul_add_asm_end r3
_compare:
    lr 0(r1) r0
    lr 0(r2) r4
    cmp r4 r0
    jne fail
    add 0x4 r1
    add 0x4 r2
    cmp r3 r2
    jne _compare

    # A routine that calls itself: the factorial of 5 is 120.
    li 0x7 r7
    mov sp r5
    push 0x5
    call factorial
    add 0x4 sp
    cmp 0x78 r0
    jne fail
    cmp r5 sp
    jne fail

    # `call` through `lr` itself: the CPU reads the target before it writes
    # the return address.
    li 0x8 r7
    li 0x0 r0
    li leaf lr
    call lr
    cmp 0x5 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Returns 5. A routine that calls nothing can leave `lr` where it is.
leaf:
    li 0x5 r0
    ret

# mul_add(a, b) returns a * b + a.
#
# An instruction is one 32-bit number: the opcode in bits 7-0, the second
# operand's register in bits 11-8, the first operand's register in bits 15-12,
# and the immediate value in bits 31-16. `fp` is register 8, `sp` is 9, `lr`
# is 10.
mul_add:
    # push lr
    .long 0x0000a012
    # push fp
    .long 0x00008012
    # mov sp fp
    .long 0x00009810
    # push r4
    .long 0x00004012
    # lr 8(fp) r4
    .long 0x00088418
    # lr 12(fp) r0
    .long 0x000c8018
    # mul r4 r0
    .long 0x0000402d
    # add r4 r0
    .long 0x00004020
    # pop r4
    .long 0x00000413
    # mov fp sp
    .long 0x00008910
    # pop fp
    .long 0x00000813
    # pop lr
    .long 0x00000a13
    # ret
    .long 0x00000032

mul_add_asm:
    push lr
    push fp
    mov sp fp
    push r4
    lr 8(fp) r4
    lr 12(fp) r0
    mul r4 r0
    add r4 r0
    pop r4
    mov fp sp
    pop fp
    pop lr
    ret
mul_add_asm_end:

# factorial(n) returns n * factorial(n - 1), or 1 if n is less than 2.
factorial:
    push lr
    push fp
    mov sp fp
    lr 8(fp) r0
    cmp 0x2 r0
    jl _factorial_one
    sub 0x1 r0
    push r0
    call factorial
    add 0x4 sp
    lr 8(fp) r1
    mul r1 r0
    jmp _factorial_end
_factorial_one:
    li 0x1 r0
_factorial_end:
    mov fp sp
    pop fp
    pop lr
    ret

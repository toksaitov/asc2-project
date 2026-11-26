# The assembly side of `calls.c`. The comment at the top of that file has the
# calling convention.

.global asm_weigh
.global asm_call_c
.global asm_call_ptr
.global asm_keeps_regs
.global asm_wide
.global asm_halves
.global asm_low_byte
.global asm_bump
.global asm_number
.global asm_handler

# int asm_number, and a pointer to the C function `c_mix`. The linker writes
# the address of the function into the word.
.section .data
asm_number:
    .long 0x12345
asm_handler:
    .long c_mix

.section .text

# int asm_weigh(int a, int b, int c)
#
# Returns a + 2 * b + 4 * c, a result that tells the arguments apart. The
# function calls nothing, so it leaves `lr` alone and reads the arguments
# from the top of the stack.
asm_weigh:
    lr 0(sp) r0
    lr 4(sp) r1
    shl 0x1 r1
    add r1 r0
    lr 8(sp) r1
    shl 0x2 r1
    add r1 r0
    ret

# int asm_call_c(int a, int b)
#
# Returns c_mix(a, b, 3) + 1, where `c_mix` is a C function. After the first
# three instructions, which every C function starts with too, the saved `fp`
# is at 0(fp), the saved `lr` at 4(fp), and the first argument at 8(fp).
asm_call_c:
    push lr
    push fp
    mov sp fp
    push 0x3
    lr 12(fp) r0
    push r0
    lr 8(fp) r0
    push r0
    call c_mix
    add 0xc sp
    add 0x1 r0
    mov fp sp
    pop fp
    pop lr
    ret

# int asm_call_ptr(int (*func)(int, int), int a, int b)
#
# Returns func(a, b). `call` with a register jumps to the address in it.
asm_call_ptr:
    push lr
    push fp
    mov sp fp
    lr 16(fp) r0
    push r0
    lr 12(fp) r0
    push r0
    lr 8(fp) r0
    call r0
    add 0x8 sp
    mov fp sp
    pop fp
    pop lr
    ret

# int asm_keeps_regs(int (*func)(int), int x)
#
# Returns func(x), or -1 if `func` has changed one of `r4`-`r7`. The function
# itself must keep them for its caller too, so it saves them first.
asm_keeps_regs:
    push lr
    push fp
    mov sp fp
    push r4
    push r5
    push r6
    push r7
    li 0x4444 r4
    li 0x5555 r5
    li 0x6666 r6
    li 0x7777 r7
    lr 12(fp) r0
    push r0
    lr 8(fp) r0
    call r0
    add 0x4 sp
    cmp 0x4444 r4
    jne _keeps_regs_changed
    cmp 0x5555 r5
    jne _keeps_regs_changed
    cmp 0x6666 r6
    jne _keeps_regs_changed
    cmp 0x7777 r7
    je _keeps_regs_end
_keeps_regs_changed:
    li -1 r0
_keeps_regs_end:
    pop r7
    pop r6
    pop r5
    pop r4
    pop fp
    pop lr
    ret

# long long asm_wide(int low, int high)
#
# Returns the 64-bit number with these halves: the lower one in `r0`, the
# upper one in `r1`.
asm_wide:
    lr 0(sp) r0
    lr 4(sp) r1
    ret

# int asm_halves(long long value, int extra)
#
# Returns (the lower half of value ^ (the upper half << 1)) + extra. The
# 64-bit argument takes two words, the lower half first.
asm_halves:
    lr 0(sp) r0
    lr 4(sp) r1
    shl 0x1 r1
    xor r1 r0
    lr 8(sp) r1
    add r1 r0
    ret

# signed char asm_low_byte(int x)
#
# Returns the lowest byte of x as a number with a sign. `lrb` extends the
# sign of the byte it loads, and the caller counts on a full 32-bit result.
asm_low_byte:
    lrb 0(sp) r0
    ret

# int asm_bump(int by)
#
# Adds `by` to the C variable `c_total` and returns the new value. `li` with
# a label of another file becomes two instructions, and the linker writes
# the two halves of the address into them.
asm_bump:
    lr 0(sp) r1
    li c_total r2
    lr 0(r2) r0
    add r1 r0
    sr r0 0(r2)
    ret

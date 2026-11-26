# The memory functions of the Almurut II C compiler that are written in
# assembly: `memset` and `memcpy`. `memory.c` has `memmove`.
#
# The compiler calls `memset` for a local array or structure that starts
# with zeros, and a program may call both functions. A kernel clears and
# copies whole pages with them, and a loop that the C compiler makes takes
# three times as many instructions as a loop written by hand. Both functions
# move whole words where the addresses are multiples of 4, because every
# access to memory costs the same on this machine, whatever its size.
#
# The C compiler passes arguments on the stack, the first one on the top, and
# takes the result from `r0`. A function may change `r0`-`r3`.

.global memset
.global memcpy

.section .text

# void *memset(void *dst, int val, unsigned size)
#
# Fills `size` bytes at `dst` with the low byte of `val`. Returns `dst`.
memset:
    lr 0(sp) r1
    lr 4(sp) r2
    lr 8(sp) r3

    # Words, if the address is a multiple of 4 and the area has a word:
    # `r0` counts them
    test 0x3 r1
    jne _set_bytes
    mov r3 r0
    shr 0x2 r0
    je _set_bytes

    # The byte four times in a word
    and 0xff r2
    mov r2 r3
    shl 0x8 r3
    or r3 r2
    mov r2 r3
    shl 0x10 r3
    or r3 r2

_set_word:
    sr r2 0(r1)
    add 0x4 r1
    sub 0x1 r0
    jne _set_word

    # The bytes that are left after the words
    lr 8(sp) r3
    and 0x3 r3

_set_bytes:
    cmp 0x0 r3
    je _set_end
_set_byte:
    srb r2 0(r1)
    add 0x1 r1
    sub 0x1 r3
    jne _set_byte

_set_end:
    lr 0(sp) r0
    ret

# void *memcpy(void *dst, const void *src, unsigned size)
#
# Copies `size` bytes from `src` to `dst`. The two areas must not overlap.
# Returns `dst`.
memcpy:
    lr 0(sp) r1
    lr 4(sp) r2
    lr 8(sp) r3

    # `r4` holds the word or the byte on its way. A function must give this
    # register back as it was.
    push r4

    # Words, if both addresses are multiples of 4 and the area has a word:
    # `r0` counts them, and `r3` keeps the number of bytes that are left
    mov r1 r0
    or r2 r0
    test 0x3 r0
    jne _copy_bytes
    mov r3 r0
    shr 0x2 r0
    je _copy_bytes
    and 0x3 r3

_copy_word:
    lr 0(r2) r4
    sr r4 0(r1)
    add 0x4 r2
    add 0x4 r1
    sub 0x1 r0
    jne _copy_word

_copy_bytes:
    cmp 0x0 r3
    je _copy_end
_copy_byte:
    lrb 0(r2) r4
    srb r4 0(r1)
    add 0x1 r2
    add 0x1 r1
    sub 0x1 r3
    jne _copy_byte

_copy_end:
    pop r4
    lr 0(sp) r0
    ret

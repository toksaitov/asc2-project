# The parts of `film.c` that are written in assembly: short loops that run
# out of the cache of the chip, so that only their data crosses the bus.
# The models of the film are drawn by the functions of `shapes_asm.s`.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global draw_marks_at
.global draw_marks_wrap
.global draw_lines_at

.section .text

# void draw_marks_at(volatile unsigned int *rect, const unsigned int *marks, int count, unsigned int offset)
#
# Draws rectangles of the RP2040 somewhere else than where they are kept:
# adds `offset` to every word of `marks` and writes the sum to the register
# for a rectangle. The offset is the number of columns, and the number of
# rows from its bit 7 on; the caller keeps the sums on the screen. `count`
# may be zero.
draw_marks_at:
    push r4
    lr 4(sp) r1
    lr 8(sp) r2
    lr 12(sp) r3
    lr 16(sp) r4
    cmp 0x1 r3
    jl _draw_marks_at_done
_draw_marks_at_next:
    lr 0(r2) r0
    add r4 r0
    sr r0 0(r1)
    add 0x4 r2
    sub 0x1 r3
    jne _draw_marks_at_next
_draw_marks_at_done:
    pop r4
    ret

# void draw_marks_wrap(volatile unsigned int *rect, const unsigned int *marks, int count, unsigned int shift)
#
# Draws rectangles of the RP2040 that pass over the screen: every one is
# moved to the right by `shift` columns, and one that leaves the screen
# there comes back at the left. `count` may be zero.
draw_marks_wrap:
    push r4
    push r5
    lr 8(sp) r1
    lr 12(sp) r2
    lr 16(sp) r3
    lr 20(sp) r4
    cmp 0x1 r3
    jl _draw_marks_wrap_done
_draw_marks_wrap_next:
    lr 0(r2) r0
    mov r0 r5
    add r4 r5
    and 0x7f r5
    and -128 r0
    or r5 r0
    sr r0 0(r1)
    add 0x4 r2
    sub 0x1 r3
    jne _draw_marks_wrap_next
_draw_marks_wrap_done:
    pop r5
    pop r4
    ret

# void draw_lines_at(volatile unsigned int *move, const unsigned int *points, int count, unsigned int offset)
#
# Draws `count` lines of the RP2040, each from a point to the next one of
# `points`: two words a line, the column in the lower half of a word and the
# row in the upper half. `offset`, a point of the same form, is added to
# every point; no column of `points` and of `offset` is negative, so that a
# sum is a point again. `move` is the register that keeps a point, and the
# register behind it draws a line to the next one. `count` may be zero.
draw_lines_at:
    push r4
    lr 4(sp) r1
    lr 8(sp) r2
    lr 12(sp) r3
    lr 16(sp) r4
    cmp 0x1 r3
    jl _draw_lines_at_done
_draw_lines_at_next:
    lr 0(r2) r0
    add r4 r0
    sr r0 0(r1)
    lr 4(r2) r0
    add r4 r0
    sr r0 4(r1)
    add 0x8 r2
    sub 0x1 r3
    jne _draw_lines_at_next
_draw_lines_at_done:
    pop r4
    ret

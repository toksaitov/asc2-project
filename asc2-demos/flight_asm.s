# The parts of `flight.c` that are written in assembly. Every loop here is
# at most 25 instructions long, so that it runs out of the cache of the chip
# wherever the linker puts it, and only its data crosses the bus.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global draw_marks
.global fly

.section .text

# void draw_marks(volatile unsigned int *rect, const unsigned int *marks, int count)
#
# Draws rectangles of the RP2040: writes every word of `marks` to its
# register for a rectangle.
draw_marks:
    lr 0(sp) r1
    lr 4(sp) r2
    lr 8(sp) r3
_draw_marks_next:
    lr 0(r2) r0
    sr r0 0(r1)
    add 0x4 r2
    sub 0x1 r3
    jne _draw_marks_next
    ret

# void fly(const unsigned int *job)
#
# Draws the land ahead of the eye, from the farthest row to the nearest. The
# job is nine words:
#
#      0  the heights of the land: rows of 64 bytes, 32 rows that repeat
#      4  the side of a square of the land in 256ths of a pixel at every
#         distance, a word each; a distance is in 16ths of a square
#      8  how many squares are seen to each side at every distance, a byte
#     12  room for the places of the points of two rows, 64 words each
#     16  how far the eye has flown, in 256ths of a square
#     20  how far the eye is to the right of the column 32, in the same unit
#     24  not zero if the land hides what is behind it
#     28  the register of the RP2040 that draws a rectangle
#     32  its register that keeps a point; the next one draws a line
#
# The eye looks along the rows, eight of which are drawn. It is 56 high (a
# height is in 32nds of a square), and what is as high as the eye lies on
# the row 20 of the screen, in the column 64 if it is straight ahead. A row
# of the land that is less than 6 sixteenths of a square away is left out.
#
# A row is drawn in four steps, each a loop that runs out of the cache:
#
# 1. The place of every point of the row is found: two multiplications by
#    the scale of the row. A place is a word for the lines of the RP2040,
#    the column in the lower half and the row in the upper half.
# 2. If the land hides what is behind it, the screen is made dark below
#    every point, as wide as the points are apart: a rectangle from the row
#    under the point to the bottom. The rows before were farther away, so
#    what they drew there cannot be seen.
# 3. A line is drawn from each point of the row to the next one.
# 4. A line is drawn from each point of the row before, which is one square
#    farther away, to the point of this row in the same column. The three
#    farthest rows go without these lines: they would lie too close to each
#    other to be told apart.
#
# The stack holds, from `sp`: the number of the row, 1 once a row has been
# drawn, and the number of the points of the row.
fly:
    push r4
    push r5
    push r6
    push r7
    push fp
    push lr
    lr 24(sp) fp
    sub 0xc sp
    li 0x7 r0
    sr r0 0(sp)
    li 0x0 r0
    sr r0 4(sp)
_fly_row:
    lr 0(sp) r7
    lr 16(fp) r6
    mov r6 r5
    and 0xff r5
    shr 0x4 r5
    mov r7 r4
    add 0x1 r4
    shl 0x4 r4
    sub r5 r4
    cmp 0x6 r4
    jl _fly_row_next
    mov r4 r0
    shl 0x2 r0
    lr 4(fp) r1
    add r1 r0
    lr 0(r0) r3
    lr 8(fp) r1
    add r4 r1
    lrbu 0(r1) r2
    lr 20(fp) r5
    mov r5 r1
    sar 0x8 r1
    add 0x20 r1
    mov r1 r0
    sub r2 r0
    shl 0x1 r2
    add 0x2 r2
    sr r2 8(sp)
    shr 0x8 r6
    add r7 r6
    add 0x1 r6
    and 0x1f r6
    shl 0x6 r6
    add r0 r6
    lr 0(fp) r1
    add r1 r6
    mov r0 r4
    sub 0x20 r4
    shl 0x8 r4
    sub r5 r4
    and 0x1 r7
    shl 0x8 r7
    shl 0x2 r0
    add r0 r7
    lr 12(fp) r1
    add r1 r7
    li 0x140040 r5
_fly_point:
    lrbu 0(r6) r0
    li 0x38 r1
    sub r0 r1
    mul r3 r1
    sar 0xd r1
    shl 0x10 r1
    mov r4 r0
    mul r3 r0
    sar 0x10 r0
    add r0 r1
    add r5 r1
    sr r1 0(r7)
    add 0x1 r6
    add 0x100 r4
    add 0x4 r7
    sub 0x1 r2
    jne _fly_point
    lr 24(fp) r0
    cmp 0x0 r0
    je _fly_lines
    shr 0x9 r3
    add 0x1 r3
    lr 8(sp) r6
    shl 0x2 r6
    mov r7 r4
    sub r6 r4
    lr 28(fp) r5
    li 0xffe00080 r6
    li 0x7e00000 lr
_fly_dark:
    lr 0(r4) r0
    mov r0 r1
    shl 0x10 r1
    sar 0x10 r1
    sar 0x10 r0
    add 0x1 r0
    usat r0
    mul r6 r0
    add lr r0
    mov r1 r2
    add r3 r2
    sub r3 r1
    usat r1
    sub r1 r2
    sub 0x1 r2
    jl _fly_dark_next
    shl 0xd r2
    or r2 r0
    or r1 r0
    sr r0 0(r5)
_fly_dark_next:
    add 0x4 r4
    cmp r7 r4
    jne _fly_dark
_fly_lines:
    lr 8(sp) r6
    shl 0x2 r6
    mov r7 r4
    sub r6 r4
    lr 32(fp) r5
    lr 0(r4) r0
    sr r0 0(r5)
    add 0x4 r4
_fly_line:
    lr 0(r4) r0
    sr r0 4(r5)
    add 0x4 r4
    cmp r7 r4
    jne _fly_line
    lr 4(sp) r0
    cmp 0x0 r0
    je _fly_row_drawn
    lr 0(sp) r0
    cmp 0x5 r0
    jge _fly_row_drawn
    mov r7 r4
    sub r6 r4
    lr 0(sp) r0
    and 0x1 r0
    shl 0x9 r0
    mov r4 r1
    add 0x100 r1
    sub r0 r1
_fly_depth:
    lr 0(r1) r0
    sr r0 0(r5)
    lr 0(r4) r0
    sr r0 4(r5)
    add 0x4 r1
    add 0x4 r4
    cmp r7 r4
    jne _fly_depth
_fly_row_drawn:
    li 0x1 r0
    sr r0 4(sp)
_fly_row_next:
    lr 0(sp) r0
    sub 0x1 r0
    sr r0 0(sp)
    cmp 0x0 r0
    jge _fly_row
    add 0xc sp
    pop lr
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

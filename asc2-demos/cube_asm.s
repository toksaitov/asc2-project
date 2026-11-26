# The two loops of `cube.c` that are written in assembly.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global turn_points
.global line_pixels

.section .text

# void turn_points(const unsigned int *points, int *sums, int count)
#
# Multiplies the matrix in the weights of the array (a row in each of
# `w0`-`w2`) by every point and stores the three sums of the point. A point
# is a word with x, y, z, and 0 as signed bytes, x in the low byte. One
# `dotm` computes all three sums.
turn_points:
    push r4
    push r5
    push r6
    lr 12(sp) r4
    lr 16(sp) r5
    lr 20(sp) r6
_turn_points_next:
    lr 0(r4) r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x0 r1
    li 0x0 r2
    li 0x0 r3
    dotm r1
    sr r1 0(r5)
    sr r2 4(r5)
    sr r3 8(r5)
    add 0x4 r4
    add 0xc r5
    sub 0x1 r6
    jne _turn_points_next
    pop r6
    pop r5
    pop r4
    ret

# void line_pixels(volatile unsigned int *reg, unsigned int pixel, unsigned int last,
#                  int dx, int dy, int step_x, int step_y)
#
# Draws a line pixel by pixel: writes the word of every pixel from `pixel` to
# `last` to the register at `reg`. `dx` is the distance in columns, `dy` the
# distance in rows as a negative number, `step_x` and `step_y` are what the
# word changes by from a pixel to the next. The steps are those of the lines
# of the RP2040 (Bresenham), so the two draw the same pixels.
line_pixels:
    push r4
    push r5
    push r6
    push r7
    push fp
    lr 20(sp) r3
    lr 24(sp) r0
    lr 28(sp) r1
    lr 32(sp) r4
    lr 36(sp) r5
    lr 40(sp) r6
    lr 44(sp) r7

    # r2 is the error of the line, fp twice that
    mov r4 r2
    add r5 r2
_line_pixels_next:
    sr r0 0(r3)
    cmp r1 r0
    je _line_pixels_end
    mov r2 fp
    add r2 fp
    cmp r5 fp
    jl _line_pixels_no_x
    add r5 r2
    add r6 r0
_line_pixels_no_x:
    cmp r4 fp
    jg _line_pixels_next
    add r4 r2
    add r7 r0
    jmp _line_pixels_next
_line_pixels_end:
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

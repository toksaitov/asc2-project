# The parts of `shapes.c` that are written in assembly. Every loop here is
# at most 25 instructions long, so that it runs out of the cache of the chip
# wherever the linker puts it, and only its data crosses the bus. The code
# between the loops is fetched over the bus, an instruction at a time, and
# is kept short for that reason.
#
# The caller has pushed the arguments, the first one on the top of the stack.
# A function may change `r0`-`r3` and keeps every other register.

.global set_turn
.global turn_points
.global place_points
.global draw_edges
.global style_sides
.global sort_sides
.global fill_sides

.section .text

# void set_turn(unsigned int around, unsigned int tilt, int size, const signed char *sines, unsigned int light, unsigned int *seen)
#
# Makes the matrix of a turn around the upright axis by the angle `around`
# and then a tilt of the top towards the eye by the angle `tilt`, and puts
# it into the weights of the array, a row in each of `w0`-`w2`, for
# `turn_points`. An angle is in 256ths of a circle, and `sines` has the sine
# of every angle in 64ths. With s and c for the sine and the cosine of
# `around`, and S and C for those of `tilt`, the rows of the matrix are
#
#     c     0    s
#     sS    C   -cS
#    -sC    S    cC
#
# The first two rows go into the weights multiplied by `size`, in 64ths, so
# that the model is as large on the screen as it shall be, and the second
# row with the other sign, because the rows of the screen grow downwards.
#
# `seen` gets two words for `style_sides`, each three signed bytes: where
# the light comes from, and where the eye is, both as the model sees them,
# which is the turn taken back. `light` is where the light comes from as the
# eye sees it, in 64ths; a `dot4` with a column of the matrix takes it back.
# The eye is 90 away from the middle of the model, on the third axis, and
# the fourth byte of its word is -32 (see `style_sides`).
#
# The inputs of the array, `x0`-`x2`, keep the columns of the matrix for a
# moment: the function has more numbers at work than the CPU has registers.
set_turn:
    push r4
    push r5
    push r6
    push r7
    push fp
    push lr
    lr 36(sp) r3
    lr 24(sp) r0
    mov r0 r1
    and 0xff r0
    add r3 r0
    lrb 0(r0) r4
    add 0x40 r1
    and 0xff r1
    add r3 r1
    lrb 0(r1) r5
    lr 28(sp) r0
    mov r0 r1
    and 0xff r0
    add r3 r0
    lrb 0(r0) r6
    add 0x40 r1
    and 0xff r1
    add r3 r1
    lrb 0(r1) r7
    mov r4 r0
    mul r6 r0
    sar 0x6 r0
    mov r5 r1
    mul r6 r1
    sar 0x6 r1
    not r1
    add 0x1 r1
    mov r4 r2
    mul r7 r2
    sar 0x6 r2
    not r2
    add 0x1 r2
    mov r5 r3
    mul r7 r3
    sar 0x6 r3
    mov r2 fp
    and 0xff fp
    shl 0x8 fp
    mov r0 lr
    and 0xff lr
    or lr fp
    shl 0x8 fp
    mov r5 lr
    and 0xff lr
    or lr fp
    mov fp x0
    mov r6 fp
    and 0xff fp
    shl 0x8 fp
    mov r7 lr
    and 0xff lr
    or lr fp
    shl 0x8 fp
    mov fp x1
    mov r3 fp
    and 0xff fp
    shl 0x8 fp
    mov r1 lr
    and 0xff lr
    or lr fp
    shl 0x8 fp
    mov r4 lr
    and 0xff lr
    or lr fp
    mov fp x2
    mov r3 fp
    and 0xff fp
    shl 0x8 fp
    mov r6 lr
    and 0xff lr
    or lr fp
    shl 0x8 fp
    mov r2 lr
    and 0xff lr
    or lr fp
    mov fp w2
    mul 90 r3
    sar 0x6 r3
    and 0xff r3
    mul 90 r6
    sar 0x6 r6
    and 0xff r6
    mul 90 r2
    sar 0x6 r2
    and 0xff r2
    li 0xe0 fp
    shl 0x8 fp
    or r3 fp
    shl 0x8 fp
    or r6 fp
    shl 0x8 fp
    or r2 fp
    lr 44(sp) lr
    sr fp 4(lr)
    lr 32(sp) r2
    mul r2 r4
    sar 0x6 r4
    and 0xff r4
    shl 0x10 r4
    mul r2 r5
    sar 0x6 r5
    and 0xff r5
    or r5 r4
    mov r4 w0
    mul r2 r1
    sar 0x6 r1
    not r1
    add 0x1 r1
    and 0xff r1
    shl 0x8 r1
    mul r2 r7
    sar 0x6 r7
    not r7
    add 0x1 r7
    and 0xff r7
    or r7 r1
    shl 0x8 r1
    mul r2 r0
    sar 0x6 r0
    not r0
    add 0x1 r0
    and 0xff r0
    or r0 r1
    mov r1 w1
    lr 40(sp) r3
    mov x0 r0
    li 0x0 r4
    dot4 r0 r3 r4
    sar 0x6 r4
    and 0xff r4
    mov x1 r0
    li 0x0 r5
    dot4 r0 r3 r5
    sar 0x6 r5
    and 0xff r5
    shl 0x8 r5
    or r5 r4
    mov x2 r0
    li 0x0 r6
    dot4 r0 r3 r6
    sar 0x6 r6
    and 0xff r6
    shl 0x10 r6
    or r6 r4
    sr r4 0(lr)
    pop lr
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

# void turn_points(const unsigned int *points, int *sums, int count)
#
# Multiplies the matrix in the weights of the array (a row in each of
# `w0`-`w2`; see `set_turn`) by every point and stores the three sums of the
# point. A point
# is a word with x, y, z, and 0 as signed bytes, x in the low byte. One
# `dotm` computes all three sums: nine multiplications.
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

# void place_points(const int *sums, unsigned int *places, int count, const short *zoom, unsigned int middle)
#
# Finds the place of every turned point on the screen and stores it as one
# word: the column in the lower half and the row in the upper half, which is
# what the registers of the RP2040 for lines take. The sums of a point are
# x, y, and z in 64ths, with y growing downwards. `zoom` has what a size is
# multiplied by at each depth from -32 to 31, in 64ths: a point that is
# nearer to the eye lies farther from the middle. `middle` is the place of
# the middle of the model in the same form as a place; `shapes.c` puts it
# at the column 64 of the row 36. A row that is more than 28 rows above the
# middle or more than 27 below it becomes that row, so that with the middle
# in the row 36 nothing is drawn into the first row of text or beyond the
# screen; a model is made to stay between them, and to stay between the
# columns 0 and 127.
place_points:
    push r4
    push r5
    push r6
    push r7
    lr 16(sp) r4
    lr 20(sp) r5
    lr 24(sp) r6
    lr 28(sp) r7
    add 0x40 r7
    lr 32(sp) r3
_place_points_next:
    lr 8(r4) r0
    sar 0x5 r0
    and -2 r0
    add r7 r0
    lrh 0(r0) r0
    lr 4(r4) r1
    mul r0 r1
    sar 0xc r1
    cmp -28 r1
    jge _place_points_low
    li -28 r1
_place_points_low:
    cmp 27 r1
    jle _place_points_high
    li 27 r1
_place_points_high:
    shl 0x10 r1
    lr 0(r4) r2
    mul r0 r2
    sar 0xc r2
    add r2 r1
    add r3 r1
    sr r1 0(r5)
    add 0xc r4
    add 0x4 r5
    sub 0x1 r6
    jne _place_points_next
    pop r7
    pop r6
    pop r5
    pop r4
    ret

# void draw_edges(volatile unsigned int *move, const unsigned int *places, const unsigned char *edges, int count)
#
# Draws the edges of a model as lines of the RP2040. An edge is two bytes,
# the numbers of its two points. `move` is the register that keeps a point,
# and the register behind it draws a line to the next one.
draw_edges:
    push r4
    push r5
    push r6
    lr 12(sp) r3
    lr 16(sp) r4
    lr 20(sp) r5
    lr 24(sp) r6
_draw_edges_next:
    lrbu 0(r5) r0
    shl 0x2 r0
    add r4 r0
    lr 0(r0) r0
    sr r0 0(r3)
    lrbu 1(r5) r0
    shl 0x2 r0
    add r4 r0
    lr 0(r0) r0
    sr r0 4(r3)
    add 0x2 r5
    sub 0x1 r6
    jne _draw_edges_next
    pop r6
    pop r5
    pop r4
    ret

# void style_sides(const unsigned int *sides, unsigned int *styles, int count)
#
# Decides for every side whether the eye sees it and how bright it is. The
# first word of a side is the direction that it faces and how far its plane
# lies from the middle of the model. The array has the direction of the
# light in its first row of weights, and in the second row the place of the
# eye with -32 as the fourth weight. One `dotm` then gives both answers: how
# much light falls on the side, and whether the eye is in front of its plane
# (the sum is above zero) or behind it.
#
# A style is 0 for a side that is not seen, 1 for a side in the shade, and 2
# to 6 for a side in the light, the brighter the higher. A side of which both
# faces can be seen is seen from behind too, with the light of its other
# face.
style_sides:
    push r4
    push r5
    push r6
    lr 12(sp) r4
    lr 16(sp) r5
    lr 20(sp) r6
_style_sides_next:
    lr 0(r4) r0
    mov r0 x0
    mov r0 x1
    li 0x0 r0
    li 0x0 r1
    dotm r0
    li 0x0 r3
    cmp 0x0 r1
    jg _style_sides_seen
    lrb 4(r4) r2
    cmp 0x0 r2
    jge _style_sides_store
    not r0
    add 0x1 r0
_style_sides_seen:
    li 0x1 r3
    cmp 0x0 r0
    jle _style_sides_store
    sar 0xb r0
    add 0x2 r0
    mov r0 r3
_style_sides_store:
    sr r3 0(r5)
    add 0xc r4
    add 0x4 r5
    sub 0x1 r6
    jne _style_sides_next
    pop r6
    pop r5
    pop r4
    ret

# void sort_sides(const unsigned int *sides, const int *sums, int *depths, unsigned char *order, int count)
#
# Puts the numbers of the sides in `order` so that the farthest side comes
# first. The depth of a side is the sum of the depths of its first three
# points. The sides change places seldom, so the order of the last picture
# is nearly right, and a side is moved back past the few sides that are
# nearer.
sort_sides:
    push r4
    push r5
    push r6
    push r7
    push lr
    lr 20(sp) r4
    lr 24(sp) r5
    lr 28(sp) r6
    lr 36(sp) r7
_sort_sides_depth:
    lrbu 5(r4) r0
    mul 0xc r0
    add r5 r0
    lr 8(r0) r1
    lrbu 6(r4) r0
    mul 0xc r0
    add r5 r0
    lr 8(r0) r2
    add r2 r1
    lrbu 7(r4) r0
    mul 0xc r0
    add r5 r0
    lr 8(r0) r2
    add r2 r1
    sr r1 0(r6)
    add 0xc r4
    add 0x4 r6
    sub 0x1 r7
    jne _sort_sides_depth
    lr 28(sp) r3
    lr 32(sp) r7
    mov r7 r1
    lr 36(sp) r6
    add r7 r6
    add 0x1 r1
    cmp r6 r1
    je _sort_sides_done
_sort_sides_next:
    lrbu 0(r1) r4
    mov r4 r0
    shl 0x2 r0
    add r3 r0
    lr 0(r0) r5
    mov r1 r2
_sort_sides_back:
    cmp r7 r2
    je _sort_sides_put
    lrbu -1(r2) r0
    mov r0 lr
    shl 0x2 lr
    add r3 lr
    lr 0(lr) lr
    cmp r5 lr
    jle _sort_sides_put
    srb r0 0(r2)
    sub 0x1 r2
    jmp _sort_sides_back
_sort_sides_put:
    srb r4 0(r2)
    add 0x1 r1
    cmp r6 r1
    jne _sort_sides_next
_sort_sides_done:
    pop lr
    pop r7
    pop r6
    pop r5
    pop r4
    ret

# void fill_sides(const unsigned int *job)
#
# Draws the sides of a model that are seen, in the order of `order`. The
# job is eleven words:
#
#      0  the sides, three words each (see `shapes_models.h`)
#      4  the places of the points (see `place_points`)
#      8  the styles of the sides (see `style_sides`); 7 is a side that is
#         always dark and covers what is behind it
#     12  the numbers of the sides in the order of drawing, a byte each
#     16  the number of the sides
#     20  not zero if a side may cover another one: then every side is made
#         dark before its lit rows are drawn
#     24  65536 divided by every number of rows from 1 to 63, a word each
#     28  room for two bytes for every row of the screen
#     32  the register of the RP2040 that draws a rectangle
#     36  its register that keeps a point; the next one draws a line
#     40  the lit rows for a style and a first row (see `lit_rows`)
#
# A side is drawn in four steps, each a loop that runs out of the cache:
#
# 1. The columns and the rows of its points are copied to the stack, the
#    first point once more at the end, and the top row and the bottom row
#    are found.
# 2. Every edge writes its column in each of its rows into the room for the
#    rows: an edge that leads down into the first byte of a row, and an edge
#    that leads up into the second one. The column moves by a step of 16
#    bits behind the point from a row to the next. An edge has the rows
#    from its upper end up to but without its lower end, so each row of the
#    side gets both bytes.
# 3. If sides cover each other, every row of the side is made dark from one
#    column to the other: a rectangle of the RP2040, which is a word with
#    the column, the row from bit 7, the width less one from bit 13, and 1
#    for lit in bit 27.
# 4. The lit rows of the style are drawn a pixel shorter at each end, and
#    the top row is left out, so that a dark seam parts the side from its
#    neighbours.
#
# A side in the shade (the styles 1, 2, and 7) gets its edges drawn as lines
# after that. A side of the style 1 has no lit rows; if nothing is covered,
# only its edges are drawn.
#
# The stack holds, from `sp`: five points of two words, the place in
# `order`, the number of the sides that are left, the style and the address
# of the side, and its top row and bottom row.
fill_sides:
    push r4
    push r5
    push r6
    push r7
    push fp
    push lr
    lr 24(sp) fp
    sub 0x40 sp
    lr 12(fp) r1
    lr 16(fp) r2
_fill_sides_find:
    lr 8(fp) r3
_fill_sides_look:
    lrbu 0(r1) r0
    add 0x1 r1
    mov r0 r4
    shl 0x2 r4
    add r3 r4
    lr 0(r4) r7
    cmp 0x0 r7
    jne _fill_sides_found
    sub 0x1 r2
    jne _fill_sides_look
    jmp _fill_sides_end
_fill_sides_found:
    sr r1 40(sp)
    sr r2 44(sp)
    mov r0 r1
    mul 0xc r1
    lr 0(fp) r0
    add r0 r1
    lr 20(fp) r0
    cmp 0x0 r0
    jne _fill_sides_fill
    cmp 0x1 r7
    je _fill_sides_edges
_fill_sides_fill:
    sr r7 48(sp)
    sr r1 52(sp)
    lrbu 4(r1) r2
    and 0x7 r2
    mov r2 lr
    add 0x1 r2
    add 0x5 r1
    mov sp r3
    lr 4(fp) r4
    li 0x3f r5
    li 0x8 r6
_fill_sides_point:
    lrbu 0(r1) r0
    shl 0x2 r0
    add r4 r0
    lr 0(r0) r0
    mov r0 r7
    shr 0x10 r7
    and 0xff r0
    sr r0 0(r3)
    sr r7 4(r3)
    cmp r5 r7
    jge _fill_sides_low
    mov r7 r5
_fill_sides_low:
    cmp r6 r7
    jle _fill_sides_high
    mov r7 r6
_fill_sides_high:
    add 0x1 r1
    add 0x8 r3
    sub 0x1 r2
    jne _fill_sides_point
    cmp r5 r6
    je _fill_sides_shade
    sr r5 56(sp)
    sr r6 60(sp)
    mov lr r2
    mov sp r1
    lr 28(fp) r3
    lr 24(fp) lr
_fill_sides_edge:
    lr 0(r1) r4
    lr 4(r1) r5
    lr 8(r1) r6
    lr 12(r1) r7
    add 0x8 r1
    sub r5 r7
    je _fill_sides_edge_next
    jl _fill_sides_up
    sub r4 r6
    mov r7 r0
    shl 0x2 r0
    add lr r0
    lr 0(r0) r0
    mul r0 r6
    shl 0x10 r4
    add 0x7fff r4
    shl 0x1 r5
    add r3 r5
_fill_sides_down_row:
    mov r4 r0
    shr 0x10 r0
    srb r0 0(r5)
    add r6 r4
    add 0x2 r5
    sub 0x1 r7
    jne _fill_sides_down_row
    jmp _fill_sides_edge_next
_fill_sides_up:
    add r7 r5
    not r7
    add 0x1 r7
    sub r6 r4
    mov r7 r0
    shl 0x2 r0
    add lr r0
    lr 0(r0) r0
    mul r0 r4
    shl 0x10 r6
    add 0x7fff r6
    shl 0x1 r5
    add 0x1 r5
    add r3 r5
_fill_sides_up_row:
    mov r6 r0
    shr 0x10 r0
    srb r0 0(r5)
    add r4 r6
    add 0x2 r5
    sub 0x1 r7
    jne _fill_sides_up_row
_fill_sides_edge_next:
    sub 0x1 r2
    jne _fill_sides_edge
    lr 56(sp) r5
    lr 60(sp) r2
    shl 0x1 r2
    add r3 r2
    lr 48(sp) r7
    lr 32(fp) lr
    lr 20(fp) r0
    cmp 0x0 r0
    jne _fill_sides_cover
    cmp 0x7 r7
    jne _fill_sides_lit
_fill_sides_cover:
    mov r5 r1
    shl 0x1 r1
    add r3 r1
    mov r5 r4
    shl 0x7 r4
_fill_sides_cover_row:
    lrhu 0(r1) r0
    mov r0 r6
    shr 0x8 r6
    and 0xff r0
    sub r0 r6
    jge _fill_sides_cover_right
    add r6 r0
    not r6
    add 0x1 r6
_fill_sides_cover_right:
    shl 0xd r6
    or r6 r0
    or r4 r0
    sr r0 0(lr)
    add 0x2 r1
    add 0x80 r4
    cmp r2 r1
    jne _fill_sides_cover_row
_fill_sides_lit:
    add 0x1 r5
    mov r5 r1
    shl 0x1 r1
    add r3 r1
    cmp r2 r1
    jge _fill_sides_shade
    mov r7 r0
    shl 0x2 r0
    mov r5 r4
    and 0x3 r4
    add r4 r0
    shl 0x2 r0
    lr 40(fp) r4
    add r4 r0
    lr 0(r0) r4
    cmp 0x0 r4
    je _fill_sides_shade
    shl 0x7 r5
    li 0x7ffc001 r0
    add r0 r5
_fill_sides_lit_row:
    cmp 0x0 r4
    jge _fill_sides_lit_next
    lrhu 0(r1) r0
    mov r0 r6
    shr 0x8 r6
    and 0xff r0
    sub r0 r6
    jge _fill_sides_lit_right
    add r6 r0
    not r6
    add 0x1 r6
_fill_sides_lit_right:
    cmp 0x2 r6
    jl _fill_sides_lit_next
    shl 0xd r6
    or r6 r0
    add r5 r0
    sr r0 0(lr)
_fill_sides_lit_next:
    add r4 r4
    adc 0x0 r4
    add 0x2 r1
    add 0x80 r5
    cmp r2 r1
    jne _fill_sides_lit_row
_fill_sides_shade:
    lr 48(sp) r7
    lr 52(sp) r1
    cmp 0x2 r7
    jle _fill_sides_edges
    cmp 0x7 r7
    jne _fill_sides_next
_fill_sides_edges:
    lrbu 4(r1) r2
    and 0x7 r2
    add 0x5 r1
    lr 4(fp) r4
    lr 36(fp) r3
    lrbu 0(r1) r0
    shl 0x2 r0
    add r4 r0
    lr 0(r0) r0
    sr r0 0(r3)
_fill_sides_line:
    add 0x1 r1
    lrbu 0(r1) r0
    shl 0x2 r0
    add r4 r0
    lr 0(r0) r0
    sr r0 4(r3)
    sub 0x1 r2
    jne _fill_sides_line
_fill_sides_next:
    lr 40(sp) r1
    lr 44(sp) r2
    sub 0x1 r2
    jne _fill_sides_find
_fill_sides_end:
    add 0x40 sp
    pop lr
    pop fp
    pop r7
    pop r6
    pop r5
    pop r4
    ret

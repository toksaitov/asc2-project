# The devices of the RP2040: the screen.
#
# The screen has 128 by 64 pixels, each on or off. It is a device of the
# bridge: the RP2040 serves it as it serves the console and the disk, with a
# frame memory and a few registers (see `rp_console.s` for the others):
#
#     0x80ff003c  screen status: 0x1 = a frame is on its way to the screen
#     0x80ff0040  screen command: 1 = show the frame memory on the screen,
#                                 2 = clear the frame memory
#     0x80ff0044  frames: the number of frames that have reached the screen
#     0x80ff0048  character: a write draws one character into the frame
#                 memory; the character is in bits 7:0, its column (0 to 20)
#                 in bits 15:8, and its row (0 to 7) in bits 23:16
#     0x80ff004c  rectangle: a write draws a rectangle into the frame
#                 memory; its left column is in bits 6:0, its top row in
#                 bits 12:7, its width less 1 in bits 20:13, its height less
#                 1 in bits 26:21, and the way to draw in bits 28:27
#     0x80ff0050  picture: the physical address of a picture in RAM
#     0x80ff0054  draw picture: a write draws that picture; the column of
#                 its left edge is in bits 7:0 and the row of its top edge
#                 in bits 15:8, both signed, and the way to draw in bits
#                 17:16
#     0x80ff1000  the frame memory: 1024 bytes, read and written in words
#
# The frame memory holds 64 rows of 16 bytes. The pixel in column x of row y
# is bit x % 8 of the byte at y * 16 + x / 8, so the leftmost pixel of a word
# is its bit 0. A program draws there and sees nothing change: the screen
# shows a frame only after the command "show". The RP2040 then takes a copy
# of the frame memory and sends it to the screen, which takes a while; the
# program may draw the next frame meanwhile. A "show" that comes while a
# frame is on its way is ignored.
#
# A character is 5 pixels wide and 7 high and takes a place of 6 by 8, so a
# row of text has 21 characters, and the screen has 8 rows of text. The font
# has the characters from the space (0x20) to the tilde (0x7e); any other
# byte is drawn as a question mark.
#
# The CPU reaches the frame memory one word at a time, and every word is a
# talk over the bus. So the RP2040 also draws rectangles and pictures by
# itself, each with one write. The ways to draw are 0 = the pixels go dark,
# 1 = they light up, 2 = each becomes its opposite, and for a picture 3 =
# they become the pixels of the picture. A picture is a word with its width
# in bits 15:0 and its height in bits 31:16, then its rows from the top,
# each in as many words as the width needs, the leftmost pixel in bit 0.
# With the ways 0, 1, and 2 only its lit pixels count. What lies outside the
# screen is left out.
#
# The file `rp_screen.frame` is what the screen must show at the end.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80030000 sp
    li 0x80ff0000 r3
    li 0x80ff1000 r5

    # After a reset the frame memory is dark, no frame is on its way, and
    # none has reached the screen.
    li 0x1 r7
    lr 0(r5) r0
    cmp 0x0 r0
    jne fail
    lr 0x3fc(r5) r0
    cmp 0x0 r0
    jne fail
    lr 0x3c(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x44(r3) r0
    cmp 0x0 r0
    jne fail

    # The frame memory keeps what is written, from its first word to its
    # last.
    li 0x2 r7
    li 0x12345678 r0
    sr r0 0(r5)
    li 0x9abcdef0 r0
    sr r0 0x3fc(r5)
    lr 0(r5) r0
    li 0x12345678 r1
    cmp r1 r0
    jne fail
    lr 0x3fc(r5) r0
    li 0x9abcdef0 r1
    cmp r1 r0
    jne fail

    # The command "clear" makes the whole frame memory dark.
    li 0x3 r7
    li 0x2 r0
    sr r0 0x40(r3)
    lr 0(r5) r0
    cmp 0x0 r0
    jne fail
    lr 0x3fc(r5) r0
    cmp 0x0 r0
    jne fail

    # The letter A at the first place of the screen. Its rows from the top:
    #
    #     .###.   0x0e
    #     #...#   0x11
    #     #...#   0x11
    #     #...#   0x11
    #     #####   0x1f
    #     #...#   0x11
    #     #...#   0x11
    #     .....   0x00
    li 0x4 r7
    li 0x41 r0
    sr r0 0x48(r3)
    lr 0(r5) r0
    cmp 0xe r0
    jne fail
    lr 0x10(r5) r0
    cmp 0x11 r0
    jne fail
    lr 0x40(r5) r0
    cmp 0x1f r0
    jne fail
    lr 0x60(r5) r0
    cmp 0x11 r0
    jne fail
    lr 0x70(r5) r0
    cmp 0x0 r0
    jne fail

    # A place takes its 6 by 8 pixels and no others: the pixels of the place
    # that the character does not light go dark, and the neighbours stay.
    # The place of column 5 lies in two words: its pixels are the columns 30
    # to 35 of the screen. Row 1 of text starts at the row 8 of pixels.
    li 0x5 r7
    li -1 r0
    sr r0 0x80(r5)
    sr r0 0x84(r5)
    li 0x10520 r0
    sr r0 0x48(r3)
    lr 0x80(r5) r0
    li 0x3fffffff r1
    cmp r1 r0
    jne fail
    lr 0x84(r5) r0
    li 0xfffffff0 r1
    cmp r1 r0
    jne fail
    # The top row of an A there: the pixels 31, 32, and 33
    li 0x10541 r0
    sr r0 0x48(r3)
    lr 0x80(r5) r0
    li 0xbfffffff r1
    cmp r1 r0
    jne fail
    lr 0x84(r5) r0
    li 0xfffffff3 r1
    cmp r1 r0
    jne fail

    # A column or a row that the screen does not have draws nothing, and a
    # byte that the font does not have is drawn as a question mark, whose
    # top row is 0x0e like that of the A.
    li 0x6 r7
    li 0x2 r0
    sr r0 0x40(r3)
    li 0x1541 r0
    sr r0 0x48(r3)
    li 0x80041 r0
    sr r0 0x48(r3)
    li 0x0 r1
    li 0x0 r2
_sum_6:
    mov r5 r0
    add r1 r0
    lr 0(r0) r0
    or r0 r2
    add 0x4 r1
    cmp 0x400 r1
    jne _sum_6
    cmp 0x0 r2
    jne fail
    li 0x7f r0
    sr r0 0x48(r3)
    lr 0(r5) r0
    cmp 0xe r0
    jne fail
    lr 0x60(r5) r0
    cmp 0x4 r0
    jne fail

    # The picture of this test: a line around the screen and two words.
    li 0x7 r7
    li 0x2 r0
    sr r0 0x40(r3)
    li -1 r0
    sr r0 0(r5)
    sr r0 4(r5)
    sr r0 8(r5)
    sr r0 12(r5)
    sr r0 0x3f0(r5)
    sr r0 0x3f4(r5)
    sr r0 0x3f8(r5)
    sr r0 0x3fc(r5)
    li 0x10 r1
_sides_7:
    mov r5 r2
    add r1 r2
    li 0x1 r0
    sr r0 0(r2)
    li 0x80000000 r0
    sr r0 12(r2)
    add 0x10 r1
    cmp 0x3f0 r1
    jne _sides_7
    li text r4
    li 0x20500 r6
_text_7:
    lrbu 0(r4) r0
    cmp 0x0 r0
    je _text_end_7
    or r6 r0
    sr r0 0x48(r3)
    add 0x1 r4
    add 0x100 r6
    jmp _text_7
_text_end_7:

    # "Show" sends the frame on its way: the status says so at once, and the
    # count of frames goes up when the frame has arrived.
    li 0x8 r7
    li 0x1 r0
    sr r0 0x40(r3)
    lr 0x3c(r3) r0
    cmp 0x1 r0
    jne fail
    lr 0x44(r3) r0
    cmp 0x0 r0
    jne fail
    call wait_for_screen
    lr 0x44(r3) r0
    cmp 0x1 r0
    jne fail

    # The screen shows the copy that "show" took. What the program draws
    # afterwards waits for the next "show", and a second "show" while the
    # frame is on its way is ignored: the star below never reaches the
    # screen, and only two frames are counted.
    li 0x9 r7
    li 0x1 r0
    sr r0 0x40(r3)
    li 0x4092a r0
    sr r0 0x48(r3)
    li 0x1 r0
    sr r0 0x40(r3)
    call wait_for_screen
    lr 0x44(r3) r0
    cmp 0x2 r0
    jne fail
    lr 0x3c(r3) r0
    cmp 0x0 r0
    jne fail

    # A rectangle of 4 by 2 pixels at the column 30 of the row 1 lies in two
    # words of each of its rows.
    li 0xa r7
    li 0x2 r0
    sr r0 0x40(r3)
    li 0x0820609e r0
    sr r0 0x4c(r3)
    lr 0x10(r5) r0
    li 0xc0000000 r1
    cmp r1 r0
    jne fail
    lr 0x14(r5) r0
    cmp 0x3 r0
    jne fail
    lr 0x24(r5) r0
    cmp 0x3 r0
    jne fail
    lr 0x0(r5) r0
    cmp 0x0 r0
    jne fail
    lr 0x34(r5) r0
    cmp 0x0 r0
    jne fail
    # The opposite of the columns 31 and 32 of the row 1
    li 0x1000209f r0
    sr r0 0x4c(r3)
    lr 0x10(r5) r0
    li 0x40000000 r1
    cmp r1 r0
    jne fail
    lr 0x14(r5) r0
    cmp 0x2 r0
    jne fail
    # A rectangle of 8 by 8 at the column 126 of the row 62: only its 2 by 2
    # pixels on the screen are drawn.
    li 0x08e0ff7e r0
    sr r0 0x4c(r3)
    lr 0x3ec(r5) r0
    li 0xc0000000 r1
    cmp r1 r0
    jne fail
    lr 0x3fc(r5) r0
    cmp r1 r0
    jne fail
    lr 0x3dc(r5) r0
    cmp 0x0 r0
    jne fail
    lr 0x3e0(r5) r0
    cmp 0x0 r0
    jne fail

    # A picture of 3 by 2 pixels, drawn one column left of the screen: its
    # first column is left out.
    #
    #     #.#
    #     .#.
    li 0xb r7
    li 0x2 r0
    sr r0 0x40(r3)
    li picture r0
    li 0x80000000 r1
    sub r1 r0
    sr r0 0x50(r3)
    lr 0x50(r3) r1
    cmp r0 r1
    jne fail
    li 0x100ff r0
    sr r0 0x54(r3)
    lr 0x0(r5) r0
    cmp 0x2 r0
    jne fail
    lr 0x10(r5) r0
    cmp 0x1 r0
    jne fail
    # The way "copy" also makes the pixels under its dark pixels dark, and
    # touches nothing next to the picture.
    li -1 r0
    sr r0 0x0(r5)
    li 0x30000 r0
    sr r0 0x54(r3)
    lr 0x0(r5) r0
    li 0xfffffffd r1
    cmp r1 r0
    jne fail
    # An address that is not a multiple of 4 draws nothing.
    li 0x2 r0
    sr r0 0x40(r3)
    lr 0x50(r3) r0
    add 0x2 r0
    sr r0 0x50(r3)
    li 0x10000 r0
    sr r0 0x54(r3)
    lr 0x0(r5) r0
    cmp 0x0 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Waits until no frame is on its way to the screen.
wait_for_screen:
    li 0x2710 r1
_wait_for_screen_next:
    lr 0x3c(r3) r0
    test 0x1 r0
    je _wait_for_screen_end
    sub 0x1 r1
    jne _wait_for_screen_next
    jmp fail
_wait_for_screen_end:
    ret

.section .data
text:
    .asciz "Almurut II"

.align 4
picture:
    .long 0x00020003
    .long 0x5
    .long 0x2

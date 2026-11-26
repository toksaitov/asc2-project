# `learn`: the array corrects its weights.
#
# `learn ra` is the delta rule. The array is three rows of four cells; the
# weights of the rows are the registers `w0`, `w1`, and `w2`, and the inputs
# `x0`, `x1`, and `x2`. The byte of `ra` with the number of a row is the
# error of that row, a signed number of 128ths. Every weight becomes
#
#   weight + (error * input + 64) / 128
#
# with the division rounded down, and stays between -128 and 127 if the sum
# leaves that range. The fourth byte of `ra` belongs to no row. `learn`
# changes neither an input, nor `ra`, nor the flags.
#
# The cases of the table:
#
# * an error of zero changes nothing;
# * products next to a half of a step: 63 and 64, -64 and -65, 191 and 192,
#   -192 and -193;
# * an error of -128, which takes the input away exactly, and of 127;
# * weights at the ends of the range and one step from them;
# * a fourth byte of the errors that is not zero;
# * an error of its own for each row;
# * hard bytes and random words.
#
# An entry of the table is ten words: `w0`, `w1`, `w2`, `x0`, `x1`, `x2`,
# the errors, and the three weights after the instruction.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
entries:
    .long 0x7f80ff01
    .long 0x12345678
    .long 0x80808080
    .long 0x7f7f7f7f
    .long 0x80808080
    .long 0x01ff7f80
    .long 0x00000000
    .long 0x7f80ff01
    .long 0x12345678
    .long 0x80808080
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x413f403f
    .long 0xbfc1c0c1
    .long 0x40bf4141
    .long 0xffff0101
    .long 0x01000100
    .long 0xff000000
    .long 0x0001ffff
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x40c03fc1
    .long 0x403f40c0
    .long 0x0101ffff
    .long 0x00c04003
    .long 0x02ff01ff
    .long 0x202020e0
    .long 0x00000101
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x7f80ff01
    .long 0x7f80ff01
    .long 0x40c02010
    .long 0x007f7f80
    .long 0x817f01ff
    .long 0x7e81ff01
    .long 0x40c12010
    .long 0x7f80ff01
    .long 0x7f80ff01
    .long 0x40c02010
    .long 0x7f80ff01
    .long 0x7f80ff01
    .long 0x40c02010
    .long 0x00808080
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x7f7f7f7f
    .long 0x80808080
    .long 0x7f7f8080
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x80808080
    .long 0x00807f7f
    .long 0x7f7f7f7f
    .long 0xfefefefe
    .long 0x7f7f0000
    .long 0x7e7f7d7f
    .long 0x81808280
    .long 0x7f7e8081
    .long 0x01020304
    .long 0x01020304
    .long 0xfffefdfc
    .long 0x0040c040
    .long 0x7f7f7f7f
    .long 0x81808180
    .long 0x7f7d8080
    .long 0x01020304
    .long 0x05060708
    .long 0x090a0b0c
    .long 0x10203040
    .long 0x50607080
    .long 0x90a0b0c0
    .long 0x7f102030
    .long 0x070e151c
    .long 0x191e23e8
    .long 0xfbfe0104
    .long 0x01020304
    .long 0x05060708
    .long 0x090a0b0c
    .long 0x10203040
    .long 0x50607080
    .long 0x90a0b0c0
    .long 0x80102030
    .long 0x070e151c
    .long 0x191e23e8
    .long 0xfbfe0104
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x40404040
    .long 0x40404040
    .long 0x40404040
    .long 0x00fe0402
    .long 0x01010101
    .long 0x02020202
    .long 0xffffffff
    .long 0x10101010
    .long 0x10101010
    .long 0x10101010
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x0010f008
    .long 0x18181818
    .long 0x00000000
    .long 0x20202020
    .long 0x01c08180
    .long 0xc0ff4080
    .long 0x80c0007f
    .long 0x00808000
    .long 0x00017fff
    .long 0x81018140
    .long 0x80810081
    .long 0x013f0080
    .long 0xc0ff4080
    .long 0xfebf7e40
    .long 0xc040c081
    .long 0x01ffc07f
    .long 0xc001c040
    .long 0xc0000180
    .long 0x407f7f7f
    .long 0x01c081ff
    .long 0x808000c0
    .long 0xe040c0c1
    .long 0x01ffc07f
    .long 0xbf413f41
    .long 0x7f817fff
    .long 0x40008100
    .long 0x01017f40
    .long 0xc0c04081
    .long 0xff7f8080
    .long 0x8100c001
    .long 0x0140807f
    .long 0x40807f81
    .long 0x4181017f
    .long 0xc2015f41
    .long 0x81800101
    .long 0x80808080
    .long 0x807fc0c0
    .long 0x00010080
    .long 0x7f818181
    .long 0x40c00180
    .long 0x408101ff
    .long 0x81800102
    .long 0x81808080
    .long 0x807fbf3f
    .long 0xc0c04040
    .long 0x8081c040
    .long 0x807f817f
    .long 0xc0408081
    .long 0x40ff40ff
    .long 0x01ff8100
    .long 0x8100c0ff
    .long 0xc1c04141
    .long 0x8082a041
    .long 0x807f817f
    .long 0xc0014080
    .long 0xc08140c0
    .long 0x81808101
    .long 0xff7f80c0
    .long 0x7f00c000
    .long 0x80ff8040
    .long 0x8181c000
    .long 0xc0014080
    .long 0x818160c0
    .long 0x008100c2
    .long 0x81817f80
    .long 0xffc00001
    .long 0x407f4001
    .long 0xc0c07fff
    .long 0xff7f4001
    .long 0x01810101
    .long 0x81017f7f
    .long 0x80807f80
    .long 0xfe3e4002
    .long 0x407e4001
    .long 0x007f00c0
    .long 0x0181c040
    .long 0x8181007f
    .long 0x7f7fff40
    .long 0x80018180
    .long 0x00808101
    .long 0xc0010080
    .long 0x81000180
    .long 0x0181c040
    .long 0x8180ff7f
    .long 0xff8081c0
    .long 0x817f8180
    .long 0x7f81807f
    .long 0xc0810140
    .long 0xff4001ff
    .long 0x408001c0
    .long 0x80ff8181
    .long 0x3ffe8081
    .long 0x82408081
    .long 0x7f82807f
    .long 0x00c0ff40
    .long 0xff000080
    .long 0xff0101c0
    .long 0x00c040c0
    .long 0xc0814001
    .long 0x8100807f
    .long 0xc000c07f
    .long 0x00813f01
    .long 0x1f40e080
    .long 0xff0101c0
    .long 0xe4ed9913
    .long 0x4ed0da4f
    .long 0xc3aec6ce
    .long 0x44f7c9a9
    .long 0xc6e9d78f
    .long 0x02cd055b
    .long 0x5ce21064
    .long 0x19e680cf
    .long 0x47cdd541
    .long 0xc3bac5b9
    .long 0x64949532
    .long 0x4e171718
    .long 0x9fbf8a9d
    .long 0x7c032df6
    .long 0xada02d28
    .long 0x2fe21bce
    .long 0xf16b9453
    .long 0x7f96b22c
    .long 0x7f68f1f6
    .long 0xc6a6a180
    .long 0xf7d04f8d
    .long 0xa17e0929
    .long 0x779a4304
    .long 0x21a9249b
    .long 0x5728339b
    .long 0xbe937fe2
    .long 0xe0669de8
    .long 0xf1e048a0
    .long 0x805fe277
    .long 0x42807fec
    .long 0xaa6eb0c6
    .long 0xf09d17a6
    .long 0xf3c961be
    .long 0x066fb033
    .long 0x58dbe79b
    .long 0xf5e17dba
    .long 0x6373ac1a
    .long 0xab7fa0d0
    .long 0xb6b527e8
    .long 0xe9ad7f80
    .long 0x2bad7c3a
    .long 0x5901900c
    .long 0xb0ff8f54
    .long 0xd936fb55
    .long 0x5745a8e4
    .long 0xfb6d0353
    .long 0xea0b9007
    .long 0x29b07c3f
    .long 0x0dc5dd25
    .long 0xb0088f5b
    .long 0x2bdb93f0
    .long 0x33845e0a
    .long 0xf5ec9b57
    .long 0xee7a7788
    .long 0x118f1c71
    .long 0xbfefffb6
    .long 0xa89866cd
    .long 0x32aa8020
    .long 0x41807464
    .long 0x2afa9c7f
    .long 0x6bf0b419
    .long 0x471a9ac5
    .long 0x65accb65
    .long 0xa26e167a
    .long 0x6538b46e
    .long 0x71dd9058
    .long 0x5d13b2b1
    .long 0x7faca6ce
    .long 0x09f8c882
    .long 0x76a7ba72
    .long 0xd9aa5325
    .long 0x9ecd453e
    .long 0x7b61f844
    .long 0x554fbaf3
    .long 0xdff6259e
    .long 0x9ed53c62
    .long 0x3e0ddec9
    .long 0xb488712b
    .long 0xa7d03b58
    .long 0x715dfe4e
    .long 0xdec66a96
    .long 0x6e2363ef
    .long 0x8111044d
    .long 0x4c1b3685
    .long 0x1e4b3830
    .long 0x0877829c
    .long 0x5c3c0b93
    .long 0x9daf3cff
    .long 0x712968f3
    .long 0x8549c91e
    .long 0xa09f6614
    .long 0x406f91c4
    .long 0xe8493097
    .long 0x08902ae0
    .long 0x489364ce
    .long 0xaf4aab20
    .long 0x86cdcf7f
    .long 0xa8807ff4
    .long 0x247f80d7
    .long 0x082c528a
    .long 0x3fa71c30
    .long 0xcac2a8cc
    .long 0x35fd8dc3
    .long 0x4d3282e4
    .long 0x71c6f9f8
    .long 0x09afbf76
    .long 0x5e512439
    .long 0x61bde424
    .long 0xeab2a6ca
    .long 0x3bca800e
    .long 0x2a0037d3
    .long 0x193c55bc
    .long 0xaea561af
    .long 0x8defe82b
    .long 0xa0f95f25
    .long 0x89361861
    .long 0xcf563f8c
    .long 0x7f0f4dac
    .long 0xea397fce
    .long 0x80c971f0
    .long 0x4c397fc0
    .long 0x361a4d95
    .long 0xc05f05ae
    .long 0xcc8d494b
    .long 0xfbfdb2f1
    .long 0x3d8d0f9d
    .long 0xbb25b799
    .long 0x767f4484
    .long 0x391c799e
    .long 0xd23e0991
    .long 0x9ad3eb62
    .long 0x6d7a2ec7
    .long 0x7235c756
    .long 0xeb8566af
    .long 0x5ccbd680
    .long 0x0fbd6750
    .long 0x09d7bcfa
    .long 0x9bd9e666
    .long 0x3c7f440b
    .long 0x6d4aa63c
    .long 0xbdb80b70
    .long 0xd44c09dd
    .long 0x08a7bc27
    .long 0xa933dda3
    .long 0x83eb5ea1
    .long 0xc1f10a8b
    .long 0x10e57041
    .long 0x91d2f941
    .long 0x803a5b8a
    .long 0x15aaba40
    .long 0x4bbe26be
    .long 0x417d1a2a
    .long 0x2557c4e8
    .long 0x88437cb7
    .long 0x66bf9e10
    .long 0x961325d5
    .long 0x21ec602e
    .long 0x20d653a4
    .long 0x7f4cd136
    .long 0x3654beef
entries_end:

.section .text
main:
    li 0x80020000 sp

    # The table, over flags that are all clear and over flags that are all
    # set in turn.
    li 0x1 r7
    li entries r5
    li 0x0 r6
_entry:
    lr 0(r5) r0
    mov r0 w0
    lr 4(r5) r0
    mov r0 w1
    lr 8(r5) r0
    mov r0 w2
    lr 12(r5) r0
    mov r0 x0
    lr 16(r5) r0
    mov r0 x1
    lr 20(r5) r0
    mov r0 x2
    lr 24(r5) r3
    mov r6 flags
    learn r3
    mov flags r4
    cmp r6 r4
    jne fail
    lr 24(r5) r0
    cmp r0 r3
    jne fail
    mov w0 r0
    lr 28(r5) r1
    cmp r1 r0
    jne fail
    mov w1 r0
    lr 32(r5) r1
    cmp r1 r0
    jne fail
    mov w2 r0
    lr 36(r5) r1
    cmp r1 r0
    jne fail
    mov x0 r0
    lr 12(r5) r1
    cmp r1 r0
    jne fail
    mov x1 r0
    lr 16(r5) r1
    cmp r1 r0
    jne fail
    mov x2 r0
    lr 20(r5) r1
    cmp r1 r0
    jne fail
    xor 0xf r6
    add 0x28 r5
    li entries_end r0
    cmp r0 r5
    jne _entry

    # Step after step. Every input is 64 and every error 64, a half: a weight
    # grows by 32 with each `learn`, to 96 after three, and stays at 127 from
    # the fourth on.
    li 0x2 r7
    li 0x0 r0
    mov r0 w0
    mov r0 w1
    mov r0 w2
    li 0x40404040 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x00404040 r3
    learn r3
    learn r3
    learn r3
    mov w0 r0
    li 0x60606060 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    cmp r1 r0
    jne fail
    learn r3
    learn r3
    mov w0 r0
    li 0x7f7f7f7f r1
    cmp r1 r0
    jne fail
    mov w1 r0
    cmp r1 r0
    jne fail
    mov w2 r0
    cmp r1 r0
    jne fail

    # A neuron learns. The twelve inputs are 8 each, the weights start at
    # zero, and the neuron must give 480. Its error is the fourth part of
    # what is missing, held in a signed byte, the same for all three rows.
    # The weights go 8, 4, 6, 5 and stay there: the sums are 768, 384, 576,
    # and 480.
    li 0x3 r7
    li 0x0 r0
    mov r0 w0
    mov r0 w1
    mov r0 w2
    li 0x08080808 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x8 r5
_neuron:
    li 0x0 r1
    dotv r1
    li 0x1e0 r3
    sub r1 r3
    sar 0x2 r3
    ssat r3
    and 0xff r3
    mov r3 r2
    shl 0x8 r2
    or r3 r2
    shl 0x8 r2
    or r3 r2
    learn r2
    sub 0x1 r5
    jne _neuron
    li 0x0 r1
    dotv r1
    cmp 0x1e0 r1
    jne fail
    mov w0 r0
    li 0x05050505 r1
    cmp r1 r0
    jne fail
    mov w1 r0
    cmp r1 r0
    jne fail
    mov w2 r0
    cmp r1 r0
    jne fail

    # `learn` after `mul`, `dot4`, and a shift, and `dotv` after `learn`:
    # they share the cells and the counter inside the chip. `sp` holds the
    # errors.
    li 0x4 r7
    li 0x04030201 r0
    mov r0 w0
    li 0x08070605 r0
    mov r0 w1
    li 0x0c0b0a09 r0
    mov r0 w2
    li 0x40404040 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x7 r3
    mul 0x6 r3
    mov sp r6
    li 0x00040402 sp
    learn sp
    mov r6 sp
    cmp 0x2a r3
    jne fail
    # A row with the error 2 gains 1 in every weight, one with 4 gains 2.
    mov w0 r0
    li 0x05040302 r1
    cmp r1 r0
    jne fail
    mov w1 r0
    li 0x0a090807 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    li 0x0e0d0c0b r1
    cmp r1 r0
    jne fail
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 r3
    li 0x00010101 r4
    learn r4
    cmp 0x180 r3
    jne fail
    # Every weight has gained one half, which is rounded up to 1.
    mov w0 r0
    li 0x06050403 r1
    cmp r1 r0
    jne fail
    li 0x1 r3
    shl 0x1f r3
    li 0x00ffffff r4
    learn r4
    # And lost one half, which is rounded up to nothing.
    mov w0 r0
    li 0x06050403 r1
    cmp r1 r0
    jne fail
    li 0x00fdfdfd r4
    learn r4
    # One and a half are lost as one.
    mov w0 r0
    li 0x05040302 r1
    cmp r1 r0
    jne fail
    mov w1 r0
    li 0x0a090807 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    li 0x0e0d0c0b r1
    cmp r1 r0
    jne fail
    # `dotv` works with the new weights: 2 to 5, 7 to 10, and 11 to 14 are
    # 98 together, times 64.
    li 0x0 r1
    dotv r1
    li 0x1880 r0
    cmp r0 r1
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

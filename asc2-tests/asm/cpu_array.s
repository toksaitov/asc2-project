# The array of the dot products: its registers, `dotv`, `dotm`, and `xsh`.
#
# The array is three rows of four cells. A cell multiplies two signed bytes,
# a weight and an input. The weights of the rows are the registers `w0`,
# `w1`, and `w2`, the inputs `x0`, `x1`, and `x2`, and the low byte of a
# register belongs to the first cell of its row. `mov` copies a word between
# a register of the array and a register of the CPU.
#
# * `dotv rd` adds the twelve products to `rd` and sets the flags as `test`
#   does for the new value of `rd`: Z if it is zero, S if it is negative, C
#   and O clear.
# * `dotm rd` adds the four products of the first row to `rd`, those of the
#   second row to the register after `rd`, and those of the third row to the
#   register after that. The flags do not change.
# * `xsh ra` moves every input on by one cell: the first cell of `x0` takes
#   the low byte of `ra`, the first cell of `x1` the byte of the last cell of
#   `x0`, and the byte of the last cell of `x2` is dropped.
#
# None of the three changes a weight, and only `xsh` changes an input.
#
# The test goes through the cases where a chip is most likely to go wrong:
#
# * the six registers of the array, each with its own word;
# * every bit of a weight alone against inputs that differ from cell to cell,
#   and every bit of an input alone against such weights: the chip takes the
#   bits of a weight one per clock, the sign bit of each byte first;
# * a table for `dotv` and one for `dotm` with the largest and smallest
#   sums, one cell at a time, all cells but one, and random words;
# * the flags of `dotv` over flags that were all set, and the flags after
#   `dotm`;
# * `sp` and `flags` as the sum of `dotv`;
# * `dotv` and `dotm` right after `dot4`, `mul`, and a shift, which use the
#   same cells and the same counter inside the chip, and `dot4` after them.
#
# An entry of the table of `dotv` is nine words: `w0`, `w1`, `w2`, `x0`,
# `x1`, `x2`, `rd`, `rd` after the instruction, and the flags C, Z, S, and O
# after it. An entry of the table of `dotm` is twelve words: the six
# registers, the three sums, and the three sums after the instruction.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
dotv_entries:
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x00000000
    .long 0x00030000
    .long 0x00000000
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x00000000
    .long 0xfffd0600
    .long 0x00000004
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x00000000
    .long 0x0002f40c
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffffff
    .long 0xffffffff
    .long 0xffffffff
    .long 0x00000000
    .long 0x00000000
    .long 0x00000002
    .long 0xffffffff
    .long 0xffffffff
    .long 0xffffffff
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x12345678
    .long 0x12345678
    .long 0x00000000
    .long 0x00000080
    .long 0x00000000
    .long 0x00000000
    .long 0x0000007f
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffc080
    .long 0x00000004
    .long 0x00008300
    .long 0x00000000
    .long 0x00000000
    .long 0x00007a00
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffc46e
    .long 0x00000004
    .long 0x00860000
    .long 0x00000000
    .long 0x00000000
    .long 0x00750000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffc83e
    .long 0x00000004
    .long 0x89000000
    .long 0x00000000
    .long 0x00000000
    .long 0x70000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffcbf0
    .long 0x00000004
    .long 0x00000000
    .long 0x0000008c
    .long 0x00000000
    .long 0x00000000
    .long 0x0000006b
    .long 0x00000000
    .long 0x00000000
    .long 0xffffcf84
    .long 0x00000004
    .long 0x00000000
    .long 0x00008f00
    .long 0x00000000
    .long 0x00000000
    .long 0x00006600
    .long 0x00000000
    .long 0x00000000
    .long 0xffffd2fa
    .long 0x00000004
    .long 0x00000000
    .long 0x00920000
    .long 0x00000000
    .long 0x00000000
    .long 0x00610000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffd652
    .long 0x00000004
    .long 0x00000000
    .long 0x95000000
    .long 0x00000000
    .long 0x00000000
    .long 0x5c000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffffd98c
    .long 0x00000004
    .long 0x00000000
    .long 0x00000000
    .long 0x00000098
    .long 0x00000000
    .long 0x00000000
    .long 0x00000057
    .long 0x00000000
    .long 0xffffdca8
    .long 0x00000004
    .long 0x00000000
    .long 0x00000000
    .long 0x00009b00
    .long 0x00000000
    .long 0x00000000
    .long 0x00005200
    .long 0x00000000
    .long 0xffffdfa6
    .long 0x00000004
    .long 0x00000000
    .long 0x00000000
    .long 0x009e0000
    .long 0x00000000
    .long 0x00000000
    .long 0x004d0000
    .long 0x00000000
    .long 0xffffe286
    .long 0x00000004
    .long 0x00000000
    .long 0x00000000
    .long 0xa1000000
    .long 0x00000000
    .long 0x00000000
    .long 0x48000000
    .long 0x00000000
    .long 0xffffe548
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f00
    .long 0xff7f8002
    .long 0x7f7f8080
    .long 0xd837115f
    .long 0xd836d2de
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80810001
    .long 0xff7f8002
    .long 0x7f7f8080
    .long 0xea8ff5f8
    .long 0xea8f77f6
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80007f01
    .long 0xff7f8002
    .long 0x7f7f8080
    .long 0x026f4c84
    .long 0x026ece03
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x00817f01
    .long 0xff7f8002
    .long 0x7f7f8080
    .long 0x50a74cff
    .long 0x50a74d7e
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f8000
    .long 0x7f7f8080
    .long 0x73e1d036
    .long 0x73e19037
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f0002
    .long 0x7f7f8080
    .long 0x8f8de0e9
    .long 0x8f8da268
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff008002
    .long 0x7f7f8080
    .long 0x3eb459c8
    .long 0x3eb41b46
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0x007f8002
    .long 0x7f7f8080
    .long 0xf11fc8de
    .long 0xf11f895d
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f8002
    .long 0x7f7f8000
    .long 0x0977e471
    .long 0x0977e4f0
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f8002
    .long 0x7f7f0080
    .long 0x856980bb
    .long 0x8569413a
    .long 0x00000004
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f8002
    .long 0x7f008080
    .long 0x658ef30d
    .long 0x658ef38c
    .long 0x00000000
    .long 0x7f807f80
    .long 0x80ff017f
    .long 0x0180ff7f
    .long 0x80817f01
    .long 0xff7f8002
    .long 0x007f8080
    .long 0x2e7abed5
    .long 0x2e7a7f55
    .long 0x00000000
    .long 0x00000201
    .long 0x00000000
    .long 0x00000000
    .long 0x0000ff02
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00000002
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0xfffffff4
    .long 0x00000000
    .long 0x00000002
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0xfffffff3
    .long 0xffffffff
    .long 0x00000004
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7fffffff
    .long 0x8002f40b
    .long 0x00000004
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0x80000000
    .long 0x7ffd0600
    .long 0x00000000
    .long 0xc0007fc0
    .long 0x80800080
    .long 0x017fff00
    .long 0x01814000
    .long 0x81008181
    .long 0x40c08180
    .long 0x76790f49
    .long 0x76798ec8
    .long 0x00000000
    .long 0x01ffc07f
    .long 0xc001c040
    .long 0xc0000180
    .long 0x407f7f7f
    .long 0x01c081ff
    .long 0x808000c0
    .long 0x2dbf91d3
    .long 0x2dc00fd5
    .long 0x00000000
    .long 0x007f817f
    .long 0x40400081
    .long 0x8101017f
    .long 0x80c0c040
    .long 0x01ff7f80
    .long 0x7f8100c0
    .long 0x0143ee17
    .long 0x0143ee17
    .long 0x00000000
    .long 0x01010140
    .long 0x80808180
    .long 0xc0c08080
    .long 0x0080807f
    .long 0x81810001
    .long 0x01807f81
    .long 0x774d3d0c
    .long 0x774dfa0c
    .long 0x00000000
    .long 0x8101ff40
    .long 0xc0404040
    .long 0x81c040c0
    .long 0x7f817f80
    .long 0x40808180
    .long 0xff40ffc0
    .long 0x8d4a8160
    .long 0x8d49b1e0
    .long 0x00000004
    .long 0xff810040
    .long 0x00c0ff01
    .long 0x01408081
    .long 0x8140c0c0
    .long 0x808101c0
    .long 0x7f80c081
    .long 0xe065988c
    .long 0xe065c84a
    .long 0x00000004
    .long 0x00c000ff
    .long 0xff80407f
    .long 0x81c00080
    .long 0x817f8081
    .long 0xc0000181
    .long 0x7f4001ff
    .long 0x8d96247e
    .long 0x8d95783b
    .long 0x00000004
    .long 0xc07fff40
    .long 0x7f4001c0
    .long 0x810101ff
    .long 0x017f7f01
    .long 0x7f00c081
    .long 0x81c04000
    .long 0x806f5025
    .long 0x80702c29
    .long 0x00000004
    .long 0x3051772f
    .long 0xe58ccc4d
    .long 0x160fa5dd
    .long 0x46135a82
    .long 0x92107540
    .long 0x5794bd10
    .long 0x84f00636
    .long 0x84f042aa
    .long 0x00000004
    .long 0x5c6b1c78
    .long 0x6f9a4cf5
    .long 0x26989e42
    .long 0xdee11860
    .long 0xa33aebff
    .long 0x88b6d373
    .long 0x1b08e7e4
    .long 0x1b08f3d7
    .long 0x00000000
    .long 0x90cf3dee
    .long 0x16e818b6
    .long 0x0c9c903f
    .long 0x55fd7a0c
    .long 0x8087d912
    .long 0x87bb3f9a
    .long 0x37b1216c
    .long 0x37b0f123
    .long 0x00000000
    .long 0x0295106e
    .long 0xce13c5aa
    .long 0x3bc448b3
    .long 0x5db01342
    .long 0x0e2f3d2e
    .long 0xdcc98e3f
    .long 0xf3e0e043
    .long 0xf3e0d4cc
    .long 0x00000004
    .long 0xa69e1828
    .long 0x4147e0fb
    .long 0x03afde7e
    .long 0x98bbf7a0
    .long 0x4f42488a
    .long 0x363be289
    .long 0xe6fd60d6
    .long 0xe6fd6704
    .long 0x00000004
    .long 0x716a7ae4
    .long 0xdea3f9ef
    .long 0x7ec49e94
    .long 0x5cac41e2
    .long 0x038517a4
    .long 0x9674fcda
    .long 0xfc5c1584
    .long 0xfc5c3192
    .long 0x00000004
    .long 0xeca2bc4a
    .long 0x28ad6296
    .long 0xce670dcd
    .long 0x0f9a03b5
    .long 0x5abdc29a
    .long 0x1825ffbe
    .long 0x919a46ca
    .long 0x919aa22d
    .long 0x00000004
    .long 0xa7079e88
    .long 0x556a3389
    .long 0xc36f0b94
    .long 0xa3914f68
    .long 0x141635db
    .long 0x87698d47
    .long 0x0abfb99a
    .long 0x0abfdae1
    .long 0x00000000
    .long 0xebbfda06
    .long 0x8692b5b1
    .long 0xf96318cc
    .long 0x59787334
    .long 0x1954facc
    .long 0x8248157e
    .long 0xd5fd1d79
    .long 0xd5fcd150
    .long 0x00000004
    .long 0x6df6187d
    .long 0x378af8d5
    .long 0xa57da91c
    .long 0x5797eb31
    .long 0x762fbcc3
    .long 0x286637a8
    .long 0xa6715bc4
    .long 0xa671b232
    .long 0x00000004
    .long 0x3d907d65
    .long 0x6337e6d3
    .long 0x24a5a844
    .long 0x79531e09
    .long 0x3895cf48
    .long 0x0dfa2b2a
    .long 0x9ec44882
    .long 0x9ec44a8d
    .long 0x00000004
    .long 0xeaea10be
    .long 0x62476842
    .long 0xa82ed9e4
    .long 0xbd094e6d
    .long 0xa5375006
    .long 0xf5f5ae61
    .long 0x5a6925c4
    .long 0x5a6925ad
    .long 0x00000000
dotv_entries_end:

dotm_entries:
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x80808080
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0x00010000
    .long 0x00010000
    .long 0x00010000
    .long 0x80808080
    .long 0x7f7f7f7f
    .long 0x01010101
    .long 0x7f7f7f7f
    .long 0x7f7f7f7f
    .long 0xffffffff
    .long 0x00000000
    .long 0x00000000
    .long 0x00000000
    .long 0xffff0200
    .long 0x0000fc04
    .long 0xfffffffc
    .long 0x04030201
    .long 0x08070605
    .long 0x0c0b0a09
    .long 0x01010101
    .long 0x01010101
    .long 0x01010101
    .long 0x00000100
    .long 0x00000200
    .long 0x00000300
    .long 0x0000010a
    .long 0x0000021a
    .long 0x0000032a
    .long 0x7f80ff01
    .long 0x00000000
    .long 0x00000000
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0xd837115f
    .long 0xea8ff5f8
    .long 0x026f4c84
    .long 0xd83712da
    .long 0xea8ff5f8
    .long 0x026f4c84
    .long 0x00000000
    .long 0x7f80ff01
    .long 0x00000000
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0x50a74cff
    .long 0x73e1d036
    .long 0x8f8de0e9
    .long 0x50a74cff
    .long 0x73e1d1b1
    .long 0x8f8de0e9
    .long 0x00000000
    .long 0x00000000
    .long 0x7f80ff01
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0x02fe0380
    .long 0x3eb459c8
    .long 0xf11fc8de
    .long 0x0977e471
    .long 0x3eb459c8
    .long 0xf11fc8de
    .long 0x0977e5ec
    .long 0x7fc0ff40
    .long 0x0080c000
    .long 0xff008080
    .long 0x4000017f
    .long 0x81810181
    .long 0x81808100
    .long 0xa3d1165a
    .long 0x9dfba360
    .long 0xbabeab11
    .long 0xa3d155d9
    .long 0x9dfbe2a0
    .long 0xbabeeb10
    .long 0x7fc040c0
    .long 0x4001ffc0
    .long 0x80c001c0
    .long 0x7fc00001
    .long 0xff407f7f
    .long 0xc001c081
    .long 0x97067cc0
    .long 0xeb7e7766
    .long 0x48ddf357
    .long 0x9706cb81
    .long 0xeb7e5727
    .long 0x48de3297
    .long 0x7fff8080
    .long 0x81007f81
    .long 0x7f404000
    .long 0x40810101
    .long 0x8080c0c0
    .long 0xc001ff7f
    .long 0x46dc5ccc
    .long 0x95fb57cc
    .long 0x53b4e1e5
    .long 0x46dc7c0b
    .long 0x95fb974c
    .long 0x53b4c225
    .long 0x0140807f
    .long 0x81800101
    .long 0x80808080
    .long 0x807fc0c0
    .long 0x00010080
    .long 0x7f818181
    .long 0xc96e5ff3
    .long 0x097d8b09
    .long 0xf85b1d84
    .long 0xc96e7f73
    .long 0x097d8a09
    .long 0xf85b9c84
    .long 0xff40c001
    .long 0x40408101
    .long 0x40c0c040
    .long 0x7f8081c0
    .long 0x81807f81
    .long 0xffc04080
    .long 0x6bff3521
    .long 0x281dc5a4
    .long 0x8d4a8160
    .long 0x6bff3422
    .long 0x281d4664
    .long 0x8d4a6120
    .long 0xff810040
    .long 0x00c0ff01
    .long 0x01408081
    .long 0x8140c0c0
    .long 0x808101c0
    .long 0x7f80c081
    .long 0xe065988c
    .long 0x8be77ad4
    .long 0xb64e34c5
    .long 0xe065694b
    .long 0x8be79a53
    .long 0xb64e7445
    .long 0x27ccf42d
    .long 0x9d2ca313
    .long 0x42e89007
    .long 0x7a003700
    .long 0xbd2b9816
    .long 0xa94dab08
    .long 0x4ab6636f
    .long 0xd1fca899
    .long 0x15ac7d40
    .long 0x4ab67371
    .long 0xd1fcf150
    .long 0x15ac8502
    .long 0x6473acd4
    .long 0x805ef099
    .long 0x0f590f6a
    .long 0x27595e1c
    .long 0x02be9eaf
    .long 0x9e16ba60
    .long 0x4edd2e00
    .long 0x79cd0030
    .long 0xb948e754
    .long 0x4edd418f
    .long 0x79cd0dab
    .long 0xb9490ce2
    .long 0xef323a6f
    .long 0x5fff67c6
    .long 0x57d53cde
    .long 0xa206e36e
    .long 0x0f01387b
    .long 0x97237824
    .long 0x9b0ac9d2
    .long 0x16f6c34d
    .long 0x5ca107c3
    .long 0x9b0afa5c
    .long 0x16f6c387
    .long 0x5ca0f58b
    .long 0x9a85e6e5
    .long 0xd7fe6400
    .long 0xcbc895a6
    .long 0x56525f6d
    .long 0xfb29010e
    .long 0xb0865e47
    .long 0xa3f1d54f
    .long 0x305764ae
    .long 0x43de4b4f
    .long 0xa3f17680
    .long 0x3057658d
    .long 0x43de364f
    .long 0x7f2e1a57
    .long 0x22be936d
    .long 0xa999fffe
    .long 0x3307da69
    .long 0x6a164de3
    .long 0xe53cc62e
    .long 0x10325b71
    .long 0x8d96247e
    .long 0x632ef09a
    .long 0x103295d3
    .long 0x8d95ffc4
    .long 0x632ee181
    .long 0xa8efa18f
    .long 0xf592482c
    .long 0xd3fdd500
    .long 0x2099ee8a
    .long 0x1b869ed8
    .long 0x7cb6c732
    .long 0xb24b74f2
    .long 0x9fe5f0f3
    .long 0xc01ed64a
    .long 0xb24bab8d
    .long 0x9fe601c6
    .long 0xc01ecaef
    .long 0x7ade9c78
    .long 0x35d238c0
    .long 0x65ccac77
    .long 0xf57e0d2b
    .long 0xd7205991
    .long 0xf9e2a218
    .long 0x12bd7602
    .long 0xa3b0d486
    .long 0xf4748e73
    .long 0x12bd6f1c
    .long 0xa3b0f581
    .long 0xf474bbc8
    .long 0x267a329a
    .long 0x385b9bd4
    .long 0x394d3b12
    .long 0x54150f95
    .long 0x313b8db7
    .long 0xe0cbead1
    .long 0x1926eba7
    .long 0x173c6c11
    .long 0x3509784d
    .long 0x19272fb1
    .long 0x173cc5ad
    .long 0x350958dc
dotm_entries_end:

.section .text
main:
    li 0x80020000 sp

    # The six registers hold six words, and a read changes nothing.
    li 0x1 r7
    li 0x11121314 r0
    mov r0 w0
    li 0x21222324 r0
    mov r0 w1
    li 0x31323334 r0
    mov r0 w2
    li 0x41424344 r0
    mov r0 x0
    li 0x51525354 r0
    mov r0 x1
    li 0x61626364 r0
    mov r0 x2
    li 0x2 r6
_regs:
    mov w0 r1
    li 0x11121314 r0
    cmp r0 r1
    jne fail
    mov w1 r1
    li 0x21222324 r0
    cmp r0 r1
    jne fail
    mov w2 r1
    li 0x31323334 r0
    cmp r0 r1
    jne fail
    mov x0 r1
    li 0x41424344 r0
    cmp r0 r1
    jne fail
    mov x1 r1
    li 0x51525354 r0
    cmp r0 r1
    jne fail
    mov x2 r1
    li 0x61626364 r0
    cmp r0 r1
    jne fail
    sub 0x1 r6
    jne _regs
    # A move does not change the flags, and `sp` and `flags` can take part.
    li 0xf r0
    mov r0 flags
    li 0x7f0504ff r1
    mov r1 w1
    mov w1 r2
    mov flags r0
    cmp 0xf r0
    jne fail
    cmp r1 r2
    jne fail
    mov sp r6
    li 0x80010004 sp
    mov sp x2
    mov r6 sp
    mov x2 r0
    li 0x80010004 r1
    cmp r1 r0
    jne fail
    li 0x5 r0
    mov r0 flags
    li 0xfffffffa r1
    mov r1 x0
    mov x0 flags
    mov flags r0
    cmp 0xa r0
    jne fail

    # One bit of a weight at a time against the inputs 1, 3, 5, ..., 23. A
    # bit below the sign bit of its byte counts as itself times the input of
    # its cell; the sign bit counts as -128 times that input. `r5` is the
    # number of the bit among the 96 of the three registers.
    li 0x2 r7
    li 0x07050301 r0
    mov r0 x0
    li 0x0f0d0b09 r0
    mov r0 x1
    li 0x17151311 r0
    mov r0 x2
    li 0x0 r5
_wbit:
    li 0x0 r0
    mov r0 w0
    mov r0 w1
    mov r0 w2
    # The word with the bit, in the register of its row
    mov r5 r0
    and 0x1f r0
    li 0x1 r4
    shl r0 r4
    mov r5 r0
    shr 0x5 r0
    cmp 0x0 r0
    jne _wbit_row_1
    mov r4 w0
    jmp _wbit_set
_wbit_row_1:
    cmp 0x1 r0
    jne _wbit_row_2
    mov r4 w1
    jmp _wbit_set
_wbit_row_2:
    mov r4 w2
_wbit_set:
    # The input of the cell: twice the number of the cell, plus one
    mov r5 r2
    shr 0x3 r2
    shl 0x1 r2
    add 0x1 r2
    # The worth of the bit
    mov r5 r0
    and 0x7 r0
    li 0x1 r3
    shl r0 r3
    cmp 0x80 r3
    jne _wbit_worth
    li -128 r3
_wbit_worth:
    mul r2 r3
    li 0x0 r6
    dotv r6
    cmp r3 r6
    jne fail
    add 0x1 r5
    cmp 0x60 r5
    jne _wbit

    # The same with one bit of an input at a time against the weights 1, 3,
    # 5, ..., 23.
    li 0x3 r7
    li 0x07050301 r0
    mov r0 w0
    li 0x0f0d0b09 r0
    mov r0 w1
    li 0x17151311 r0
    mov r0 w2
    li 0x0 r5
_xbit:
    li 0x0 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    mov r5 r0
    and 0x1f r0
    li 0x1 r4
    shl r0 r4
    mov r5 r0
    shr 0x5 r0
    cmp 0x0 r0
    jne _xbit_row_1
    mov r4 x0
    jmp _xbit_set
_xbit_row_1:
    cmp 0x1 r0
    jne _xbit_row_2
    mov r4 x1
    jmp _xbit_set
_xbit_row_2:
    mov r4 x2
_xbit_set:
    mov r5 r2
    shr 0x3 r2
    shl 0x1 r2
    add 0x1 r2
    mov r5 r0
    and 0x7 r0
    li 0x1 r3
    shl r0 r3
    cmp 0x80 r3
    jne _xbit_worth
    li -128 r3
_xbit_worth:
    mul r2 r3
    li 0x0 r6
    dotv r6
    cmp r3 r6
    jne fail
    add 0x1 r5
    cmp 0x60 r5
    jne _xbit

    # The table of `dotv`. All four flags are set before the instruction. The
    # registers of the array are after it as they were before.
    li 0x4 r7
    li dotv_entries r5
_dotv_entry:
    call load_array
    li 0xf r0
    mov r0 flags
    lr 24(r5) r3
    dotv r3
    mov flags r1
    lr 28(r5) r0
    cmp r0 r3
    jne fail
    lr 32(r5) r0
    cmp r0 r1
    jne fail
    call check_array
    cmp 0x0 r0
    jne fail
    add 0x24 r5
    li dotv_entries_end r0
    cmp r0 r5
    jne _dotv_entry

    # The table of `dotm`, once over flags that are all clear and once over
    # flags that are all set.
    li 0x5 r7
    li dotm_entries r5
    li 0x0 r6
_dotm_entry:
    call load_array
    lr 24(r5) r1
    lr 28(r5) r2
    lr 32(r5) r3
    mov r6 flags
    dotm r1
    mov flags r4
    cmp r6 r4
    jne fail
    lr 36(r5) r0
    cmp r0 r1
    jne fail
    lr 40(r5) r0
    cmp r0 r2
    jne fail
    lr 44(r5) r0
    cmp r0 r3
    jne fail
    call check_array
    cmp 0x0 r0
    jne fail
    xor 0xf r6
    add 0x30 r5
    li dotm_entries_end r0
    cmp r0 r5
    jne _dotm_entry

    # The other registers that `dotm` can start at. The bytes 1, 2, 3, 4
    # times 1, 1, 1, 1 are 10; 5, 6, 7, 8 give 26, and 9, 10, 11, 12 give 42.
    li 0x6 r7
    li 0x04030201 r0
    mov r0 w0
    li 0x08070605 r0
    mov r0 w1
    li 0x0c0b0a09 r0
    mov r0 w2
    li 0x01010101 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x100 r0
    li 0x200 r1
    li 0x300 r2
    dotm r0
    cmp 0x10a r0
    jne fail
    cmp 0x21a r1
    jne fail
    cmp 0x32a r2
    jne fail
    li 0x1000 r5
    li 0x2000 r6
    mov r7 r4
    li 0x3000 r7
    dotm r5
    mov r7 r3
    mov r4 r7
    cmp 0x100a r5
    jne fail
    cmp 0x201a r6
    jne fail
    cmp 0x302a r3
    jne fail

    # `sp` as the sum of `dotv`: 78 is added to it.
    li 0x7 r7
    mov sp r6
    li 0x80010000 sp
    dotv sp
    mov sp r0
    mov r6 sp
    li 0x8001004e r1
    cmp r1 r0
    jne fail
    # `flags` as the sum: a write of `flags` takes the four low bits, and it
    # goes before the flags that `dotv` sets by the sum. 78 added to 5 is 83,
    # and the four low bits of 83 are 3.
    li 0x5 r0
    mov r0 flags
    dotv flags
    mov flags r0
    cmp 0x3 r0
    jne fail

    # `xsh`. The line is empty, and the bytes 1, 2, 3, ... enter it; the
    # upper bytes of the register do not count. After five bytes the first
    # row holds 5, 4, 3, 2 and the second row 1.
    li 0x8 r7
    li 0x0 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0xabcdef00 r4
    li 0x5 r5
_xsh_five:
    add 0x1 r4
    xsh r4
    sub 0x1 r5
    jne _xsh_five
    mov x0 r0
    li 0x02030405 r1
    cmp r1 r0
    jne fail
    mov x1 r0
    cmp 0x1 r0
    jne fail
    mov x2 r0
    cmp 0x0 r0
    jne fail
    # After fifteen bytes the line holds 15 down to 4, and 3, 2, and 1 are
    # gone.
    li 0xa r5
_xsh_ten:
    add 0x1 r4
    xsh r4
    sub 0x1 r5
    jne _xsh_ten
    mov x0 r0
    li 0x0c0d0e0f r1
    cmp r1 r0
    jne fail
    mov x1 r0
    li 0x08090a0b r1
    cmp r1 r0
    jne fail
    mov x2 r0
    li 0x04050607 r1
    cmp r1 r0
    jne fail
    # `xsh` has changed neither its register, nor the flags, nor a weight.
    li 0xabcdef0f r1
    cmp r1 r4
    jne fail
    li 0xf r0
    mov r0 flags
    xsh r4
    mov flags r0
    cmp 0xf r0
    jne fail
    mov w0 r0
    li 0x04030201 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    li 0x0c0b0a09 r1
    cmp r1 r0
    jne fail
    # A byte with its top bit set stays a byte on its way through the rows.
    li 0x80 r4
    li 0xc r5
_xsh_top:
    xsh r4
    sub 0x1 r5
    jne _xsh_top
    mov x0 r0
    li 0x80808080 r1
    cmp r1 r0
    jne fail
    mov x1 r0
    cmp r1 r0
    jne fail
    mov x2 r0
    cmp r1 r0
    jne fail
    li 0x0 r4
    xsh r4
    mov x0 r0
    li 0x80808000 r1
    cmp r1 r0
    jne fail
    mov x2 r0
    li 0x80808080 r1
    cmp r1 r0
    jne fail

    # `dotv` and `dotm` after `dot4`, `mul`, and a shift, and `dot4` after
    # them. `dot4` uses the cells of the first row and leaves the registers of
    # the array alone. The weights 1 to 12 times the inputs 1, 1, ... are 78.
    li 0x9 r7
    li 0x01010101 r0
    mov r0 x0
    mov r0 x1
    mov r0 x2
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 r3
    li 0x0 r4
    dotv r4
    cmp 0x180 r3
    jne fail
    cmp 0x4e r4
    jne fail
    li 0x0 r3
    dot4 r1 r2 r3
    cmp 0x180 r3
    jne fail
    li 0x7 r3
    mul 0x6 r3
    li 0x0 r4
    dotv r4
    cmp 0x2a r3
    jne fail
    cmp 0x4e r4
    jne fail
    li 0x1 r3
    shl 0x1f r3
    li 0x0 r4
    li 0x0 r5
    li 0x0 r6
    dotm r4
    li 0x80000000 r0
    cmp r0 r3
    jne fail
    cmp 0xa r4
    jne fail
    cmp 0x1a r5
    jne fail
    cmp 0x2a r6
    jne fail
    li 0x0 r4
    dotv r4
    li 0x0 r3
    dot4 r1 r2 r3
    li 0x0 r6
    dotv r6
    cmp 0x180 r3
    jne fail
    cmp 0x4e r4
    jne fail
    cmp 0x4e r6
    jne fail
    mov w0 r0
    li 0x04030201 r1
    cmp r1 r0
    jne fail
    mov x0 r0
    li 0x01010101 r1
    cmp r1 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Loads the six registers of the array from the entry at the address in `r5`.
load_array:
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
    ret

# Compares the six registers of the array with the entry at the address in
# `r5`. `r0` is zero if they are equal.
check_array:
    mov w0 r0
    lr 0(r5) r4
    cmp r4 r0
    jne _check_array_differs
    mov w1 r0
    lr 4(r5) r4
    cmp r4 r0
    jne _check_array_differs
    mov w2 r0
    lr 8(r5) r4
    cmp r4 r0
    jne _check_array_differs
    mov x0 r0
    lr 12(r5) r4
    cmp r4 r0
    jne _check_array_differs
    mov x1 r0
    lr 16(r5) r4
    cmp r4 r0
    jne _check_array_differs
    mov x2 r0
    lr 20(r5) r4
    cmp r4 r0
    jne _check_array_differs
    li 0x0 r0
    ret
_check_array_differs:
    li 0x1 r0
    ret

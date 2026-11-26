# The dot-product instruction: the order of the bits, the registers, and the
# instructions around it.
#
# `dot4 ra rb rd` treats `ra` and `rb` as four signed bytes each, multiplies
# the bytes in the same positions, and adds the four products to `rd`.
# `cpu_mul_dot4.s` checks the rule itself. This test adds the cases where a
# chip is most likely to go wrong:
#
# * every bit of a word, alone, against bytes that differ from position to
#   position: the chip takes the bits of `rb` one per clock, the sign bit of
#   each byte first;
# * the sign bit of each byte against small, large, and negative bytes, and a
#   table of hard bytes in all four positions at once;
# * `rd` the same register as `ra`, as `rb`, or as both;
# * `sp`, `fp`, `lr`, `flags`, and `ip` as operands, and the stack pointer of
#   a user program;
# * `dot4` right after `dot4`, `mul`, and shifts, which use the same counter
#   and the same registers inside the chip.
#
# An entry of the table is four words: `ra`, `rb`, `rd`, and `rd` after the
# instruction.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
entries:
    .long 0x00000080
    .long 0x00000001
    .long 0x00000000
    .long 0xffffff80
    .long 0x00000080
    .long 0x0000007f
    .long 0x00000000
    .long 0xffffc080
    .long 0x00000080
    .long 0x00000080
    .long 0x00000000
    .long 0x00004000
    .long 0x00000080
    .long 0x000000ff
    .long 0x00000000
    .long 0x00000080
    .long 0x00008000
    .long 0x00000100
    .long 0x00000000
    .long 0xffffff80
    .long 0x00008000
    .long 0x00007f00
    .long 0x00000000
    .long 0xffffc080
    .long 0x00008000
    .long 0x00008000
    .long 0x00000000
    .long 0x00004000
    .long 0x00008000
    .long 0x0000ff00
    .long 0x00000000
    .long 0x00000080
    .long 0x00800000
    .long 0x00010000
    .long 0x00000000
    .long 0xffffff80
    .long 0x00800000
    .long 0x007f0000
    .long 0x00000000
    .long 0xffffc080
    .long 0x00800000
    .long 0x00800000
    .long 0x00000000
    .long 0x00004000
    .long 0x00800000
    .long 0x00ff0000
    .long 0x00000000
    .long 0x00000080
    .long 0x80000000
    .long 0x01000000
    .long 0x00000000
    .long 0xffffff80
    .long 0x80000000
    .long 0x7f000000
    .long 0x00000000
    .long 0xffffc080
    .long 0x80000000
    .long 0x80000000
    .long 0x00000000
    .long 0x00004000
    .long 0x80000000
    .long 0xff000000
    .long 0x00000000
    .long 0x00000080
    .long 0xfe7f0200
    .long 0x00fe4000
    .long 0xffffffff
    .long 0xffffff81
    .long 0xfe7f0200
    .long 0x7f01c07f
    .long 0xfffffffb
    .long 0xfffffefc
    .long 0xfe7f0200
    .long 0xc07f01c0
    .long 0xfffffff8
    .long 0x00003f7b
    .long 0xc0ff4001
    .long 0x01c07f01
    .long 0x00001000
    .long 0x00002fc1
    .long 0xc0ff4001
    .long 0xff0280ff
    .long 0x00001004
    .long 0xfffff041
    .long 0xc0ff4001
    .long 0x80ff0280
    .long 0x00001007
    .long 0x00003008
    .long 0x80fe7f02
    .long 0x0280ff02
    .long 0xfffff999
    .long 0xfffff91e
    .long 0x80fe7f02
    .long 0xfe4000fe
    .long 0xfffff995
    .long 0xfffffa11
    .long 0x80fe7f02
    .long 0x00fe4000
    .long 0xfffff992
    .long 0x00001956
    .long 0x00c0ff40
    .long 0x4000fe40
    .long 0x00003000
    .long 0x00004002
    .long 0x00c0ff40
    .long 0xc07f01c0
    .long 0x00003004
    .long 0x00000043
    .long 0x00c0ff40
    .long 0x01c07f01
    .long 0x00003007
    .long 0x00003fc8
    .long 0x0180fe7f
    .long 0x7f01c07f
    .long 0xfffff333
    .long 0x000032b3
    .long 0x0180fe7f
    .long 0x80ff0280
    .long 0xfffff32f
    .long 0xffffb3ab
    .long 0x0180fe7f
    .long 0x0280ff02
    .long 0xfffff32c
    .long 0x0000342e
    .long 0x0200c0ff
    .long 0xff0280ff
    .long 0x00005000
    .long 0x00006fff
    .long 0x0200c0ff
    .long 0x00fe4000
    .long 0x00005004
    .long 0x00004004
    .long 0x0200c0ff
    .long 0x4000fe40
    .long 0x00005007
    .long 0x000050c7
    .long 0x400180fe
    .long 0xfe4000fe
    .long 0xffffeccd
    .long 0xffffec91
    .long 0x400180fe
    .long 0x01c07f01
    .long 0xffffecc9
    .long 0xffffad47
    .long 0x400180fe
    .long 0x7f01c07f
    .long 0xffffecc6
    .long 0x00002b89
    .long 0x7f0200c0
    .long 0xc07f01c0
    .long 0x00007000
    .long 0x0000613e
    .long 0x7f0200c0
    .long 0x0280ff02
    .long 0x00007004
    .long 0x00006f82
    .long 0x7f0200c0
    .long 0xff0280ff
    .long 0x00007007
    .long 0x00006fcc
    .long 0xff400180
    .long 0x80ff0280
    .long 0xffffe667
    .long 0x000026a9
    .long 0xff400180
    .long 0x4000fe40
    .long 0xffffe663
    .long 0xffffc621
    .long 0xff400180
    .long 0xfe4000fe
    .long 0xffffe660
    .long 0xfffff762
    .long 0x01010101
    .long 0x00800080
    .long 0x00000000
    .long 0xffffff00
    .long 0x7f7f7f7f
    .long 0x80008000
    .long 0x00000005
    .long 0xffff8105
    .long 0x01010101
    .long 0xff00ff00
    .long 0x00000000
    .long 0xfffffffe
    .long 0x01010101
    .long 0x00ff00ff
    .long 0x00000000
    .long 0xfffffffe
entries_end:

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_int r1
    sr r1 24(r0)

    # One bit of `rb` at a time against the bytes 1, 3, 5, and 7 of `ra`. A
    # bit below the sign bit of its byte counts as itself times the byte of
    # `ra`; the sign bit counts as -128 times that byte. `r5` is the number
    # of the bit and `r4` the word with that bit.
    li 0x1 r7
    li 0x07050301 r1
    li 0x0 r5
    li 0x1 r4
_bit:
    # The byte of `ra`: twice the number of the position, plus one
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
    jne _bit_worth
    li -128 r3
_bit_worth:
    mul r2 r3
    li 0x0 r6
    dot4 r1 r4 r6
    cmp r3 r6
    jne fail
    # The same with the two words the other way round
    li 0x0 r6
    dot4 r4 r1 r6
    cmp r3 r6
    jne fail
    shl 0x1 r4
    add 0x1 r5
    cmp 0x20 r5
    jne _bit

    # The table.
    li 0x2 r7
    li entries r5
_entry:
    lr 0(r5) r1
    lr 4(r5) r2
    lr 8(r5) r3
    dot4 r1 r2 r3
    lr 12(r5) r0
    cmp r0 r3
    jne fail
    # The two words the other way round give the same sum.
    lr 8(r5) r3
    dot4 r2 r1 r3
    cmp r0 r3
    jne fail
    add 0x10 r5
    li entries_end r0
    cmp r0 r5
    jne _entry

    # `rd` is `ra`: the bytes 0x80, 0x03, 0xfe, 0x02 times 0xff, 0x04, 0x05,
    # 0x7f is 128 + 12 - 10 + 254 = 384.
    li 0x3 r7
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    dot4 r1 r2 r1
    li 0x02fe0500 r0
    cmp r0 r1
    jne fail
    li 0x7f0504ff r0
    cmp r0 r2
    jne fail

    # `rd` is `rb`. The chip uses up its copy of `rb` as it multiplies, and
    # must add to the register itself.
    li 0x4 r7
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    dot4 r1 r2 r2
    li 0x7f05067f r0
    cmp r0 r2
    jne fail
    li 0x02fe0380 r0
    cmp r0 r1
    jne fail

    # `ra` is `rb`, and `rd` is another register: 16384 + 9 + 4 + 4 = 16401.
    li 0x5 r7
    li 0x02fe0380 r1
    li 0x1000 r3
    dot4 r1 r1 r3
    li 0x00005011 r0
    cmp r0 r3
    jne fail

    # All three are one register: 1 + 16 + 25 + 16129 = 16171.
    li 0x6 r7
    li 0x7f0504ff r2
    dot4 r2 r2 r2
    li 0x7f05442a r0
    cmp r0 r2
    jne fail

    # `fp` and `lr` as the two words, `sp` as the sum, and back.
    li 0x7 r7
    mov sp r6
    mov lr r5
    mov fp r4
    li 0x02fe0380 fp
    li 0x7f0504ff lr
    li 0x80010000 sp
    dot4 fp lr sp
    mov sp r0
    mov r6 sp
    mov r5 lr
    mov r4 fp
    li 0x80010180 r1
    cmp r1 r0
    jne fail
    # `sp` as a word: its bytes 0x04, 0x03, 0x02, 0x01 times themselves.
    li 0x01020304 sp
    li 0x0 r3
    dot4 sp sp r3
    mov r6 sp
    cmp 0x1e r3
    jne fail

    # `flags` as the sum: a write of `flags` takes the four low bits, and
    # 3 * 3 added to 0 is 9.
    li 0x8 r7
    li 0x0 r0
    mov r0 flags
    li 0x00000003 r1
    dot4 r1 r1 flags
    mov flags r0
    cmp 0x9 r0
    jne fail
    # The sum is added to the flags that are there: 3 * 3 added to 5 is 14.
    li 0x5 r0
    mov r0 flags
    li 0x00000003 r1
    dot4 r1 r1 flags
    mov flags r0
    cmp 0xe r0
    jne fail
    # `flags` as a word: its low byte times 2.
    li 0x5 r0
    mov r0 flags
    li 0x00000002 r1
    li 0x0 r3
    dot4 flags r1 r3
    cmp 0xa r3
    jne fail
    # The same with `flags` as the second word.
    li 0x5 r0
    mov r0 flags
    li 0x0 r3
    dot4 r1 flags r3
    cmp 0xa r3
    jne fail
    # A sum with bits above the four flags: 7 * 7 is 49, 0x31. `flags` takes
    # the four low bits, 1, and the two bits above them, the mode and the
    # switch of the interrupts, stay as they are.
    li 0x0 r0
    mov r0 flags
    li 0x00000007 r1
    dot4 r1 r1 flags
    mov flags r0
    cmp 0x1 r0
    jne fail

    # `ip` as a word: the bytes of the address of the instruction itself.
    # The code of this test is below 0x80800000 + 64 KB, so the bytes are
    # 0x80, 0x80, and two bytes of the place: with 0x01010101 the sum is
    # -128 - 128 + the two low bytes as signed numbers.
    li 0x9 r7
    li 0x01010101 r1
    li 0x0 r3
_ip_word:
    dot4 ip r1 r3
    li _ip_word r0
    mov r0 r2
    shl 0x18 r2
    sar 0x18 r2
    shl 0x10 r0
    sar 0x18 r0
    add r2 r0
    sub 0x100 r0
    cmp r0 r3
    jne fail
    # The same with `ip` as the second word.
    li 0x0 r3
_ip_word_b:
    dot4 r1 ip r3
    li _ip_word_b r0
    mov r0 r2
    shl 0x18 r2
    sar 0x18 r2
    shl 0x10 r0
    sar 0x18 r0
    add r2 r0
    sub 0x100 r0
    cmp r0 r3
    jne fail

    # `ip` as the sum: the address of the instruction plus the sum is where
    # the program goes on. 4 * 2 is 8: the instruction after the `dot4` does
    # not run. 4 * 3 is 12: the two instructions after it do not run.
    li 0xa r7
    li 0x00000004 r1
    li 0x00000002 r2
    dot4 r1 r2 ip
    jmp fail
    li 0x00000003 r2
    dot4 r1 r2 ip
    jmp fail
    jmp fail

    # A sum below zero goes back: 4 * -3 is -12, three instructions before
    # the `dot4`, where a jump leads on. This part runs with the F bit of
    # `cr0` (0x4) off, as a kernel does: the CPU then asks the RP2040 for
    # "the next word" where it can, and after a write to `ip` it must ask
    # with the address again.
    mov cr0 r0
    and -0x5 r0
    mov r0 cr0
    li 0x000000fd r2
    jmp _back_start
_back_target:
    jmp _back_done
    jmp fail
_back_start:
    li 0x00000004 r1
    dot4 r1 r2 ip
    jmp fail
_back_done:
    mov cr0 r0
    or 0x4 r0
    mov r0 cr0

    # Two in a row add twice.
    li 0xb r7
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 r3
    dot4 r1 r2 r3
    cmp 768 r3
    jne fail

    # After `mul`, and `mul` after it: both count their steps in the same
    # counter.
    li 0xc r7
    li 0x7 r0
    mul 0x6 r0
    li 0x0 r3
    dot4 r1 r2 r3
    li 0x7 r4
    mul 0x6 r4
    cmp 384 r3
    jne fail
    cmp 0x2a r0
    jne fail
    cmp 0x2a r4
    jne fail

    # After shifts by 0, by 1, and by 31, and shifts after it.
    li 0xd r7
    li 0x1 r0
    shl 0x0 r0
    li 0x0 r3
    dot4 r1 r2 r3
    cmp 384 r3
    jne fail
    shl 0x1 r0
    li 0x0 r3
    dot4 r1 r2 r3
    cmp 384 r3
    jne fail
    shl 0x1f r0
    li 0x0 r3
    dot4 r1 r2 r3
    shr 0x1f r0
    cmp 384 r3
    jne fail
    cmp 0x0 r0
    jne fail
    li -1 r0
    sar 0x1f r0
    dot4 r1 r2 r3
    shr 0x1f r0
    cmp 768 r3
    jne fail
    cmp 0x1 r0
    jne fail

    # A user program: `sp` is the stack pointer of user mode there, and the
    # stack pointer of the kernel stays as it is.
    li 0xe r7
    mov sp r6
    li 0x1000 r0
    mov r0 cr6
    li _back_14 r5
    li user_14 r0
    jmp enter_user
user_14:
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 sp
    dot4 r1 r2 r3
    dot4 sp sp r3
    int 0x80
_back_14:
    cmp r6 sp
    jne fail
    mov cr6 r0
    cmp 4480 r0
    jne fail
    # 0x1000 + 384 is 0x1180: the bytes 0x80 and 0x11 times themselves
    cmp 17057 r3
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Continues in user mode at the code that the kernel sees at the address in
# `r0`. `iret` loads `ip` from `cr1` and `flags` from `cr4`.
enter_user:
    li 0x80000000 r1
    sub r1 r0
    mov r0 cr1
    li 0x20 r0
    mov r0 cr4
    iret

# Continues the test in kernel mode at the address in `r5`. The handler
# changes no register of the program.
on_int:
    mov r5 cr1
    push r0
    li 0x0 r0
    mov r0 cr4
    pop r0
    iret

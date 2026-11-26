# The registers of the array in user mode, and the A bit of `cr0`.
#
# The registers of the array (`w0` to `w2`, `x0` to `x2`) belong to a process
# as the registers of the CPU do, and a kernel that switches between
# processes has to save them. The A bit of `cr0` (0x20) lets it do so only
# for the processes that use the array. A reset clears the bit. While it is
# clear, `mov` with a register of the array, `dotv`, `dotm`, `learn`, and
# `xsh` raise the privileged instruction exception (vector 1) in user mode
# and change nothing. The kernel then gives the process its registers of the
# array, sets A, and returns to the instruction. In kernel mode the bit
# changes nothing.
#
# `dot4`, `popcnt`, `ssat`, and `usat` use no register of the array and are
# never privileged. A register of a row that the array does not have is an
# invalid instruction (vector 0) in both modes and both states of A.
#
# Paging is off in this test, so the code at 0x808xxxxx is the same code at
# 0x008xxxxx for a user program (see `cpu_privilege.s`).
#
# The handlers use these registers:
#
# * `r5`: the address where the test continues in kernel mode after `int`;
# * `r6`: the number of exceptions so far;
# * `r4`: the cause (`cr5`) of the last exception;
# * `fp`: the bits of all causes since the test has cleared it.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_exception r1
    sr r1 0(r0)
    sr r1 4(r0)
    li on_int r1
    sr r1 24(r0)

    li 0x0 r6

    # The kernel fills the array. A user program then tries every instruction
    # that uses a register of the array, with A clear: eight exceptions, and
    # nothing has changed.
    li 0x1 r7
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
    li 0x0 fp
    li _back_1 r5
    li user_1 r0
    jmp enter_user
user_1:
    li 0x55 r1
    li 0x66 r2
    li 0x77 r3
    mov r1 w0
    mov r1 x2
    mov w1 r2
    mov x0 r2
    dotv r3
    dotm r1
    learn r1
    xsh r1
    int 0x80
_back_1:
    cmp 0x8 r6
    jne fail
    cmp 0x1 fp
    jne fail
    cmp 0x55 r1
    jne fail
    cmp 0x66 r2
    jne fail
    cmp 0x77 r3
    jne fail
    mov w0 r0
    li 0x04030201 r1
    cmp r1 r0
    jne fail
    mov w1 r0
    li 0x08070605 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    li 0x0c0b0a09 r1
    cmp r1 r0
    jne fail
    mov x0 r0
    li 0x01010101 r1
    cmp r1 r0
    jne fail
    mov x2 r0
    cmp r1 r0
    jne fail

    # What a user program may do with A clear: `dot4`, `popcnt`, `ssat`, and
    # `usat`. The bytes 0x80, 0x03, 0xfe, 0x02 times 0xff, 0x04, 0x05, 0x7f
    # are 384, which has two bits and is above 127.
    li 0x2 r7
    li _back_2 r5
    li user_2 r0
    jmp enter_user
user_2:
    li 0x02fe0380 r1
    li 0x7f0504ff r2
    li 0x0 r3
    dot4 r1 r2 r3
    popcnt r3 r0
    mov r3 r1
    ssat r1
    li -5 r2
    usat r2
    int 0x80
_back_2:
    cmp 0x8 r6
    jne fail
    cmp 0x180 r3
    jne fail
    cmp 0x2 r0
    jne fail
    cmp 0x7f r1
    jne fail
    cmp 0x0 r2
    jne fail

    # A row that the array does not have, and a `dotm` that would add to a
    # register after `r7`, are invalid instructions for a user program with A
    # clear, not privileged ones: `mov r0 w3`, `mov w3 r0`, `mov r0 x3`,
    # `mov x3 r0`, and `dotm r6`.
    li 0x3 r7
    li 0x0 fp
    li _back_3 r5
    li user_3 r0
    jmp enter_user
user_3:
    .long 0x00000361
    .long 0x00003060
    .long 0x00000761
    .long 0x00007060
    .long 0x00000663
    int 0x80
_back_3:
    cmp 0xd r6
    jne fail
    cmp 0x0 fp
    jne fail

    # The kernel sets A, and the user program has the array. The weights 17,
    # 18, 19, 20 of the first row times the inputs 1, 1, 1, 1 are 74; with
    # the 26 and the 42 of the other rows, 142.
    li 0x4 r7
    li 0x24 r0
    mov r0 cr0
    mov cr0 r0
    cmp 0x24 r0
    jne fail
    li _back_4 r5
    li user_4 r0
    jmp enter_user
user_4:
    li 0x11121314 r1
    mov r1 w0
    mov w0 r2
    li 0x01010101 r1
    mov r1 x0
    li 0x0 r3
    dotv r3
    int 0x80
_back_4:
    cmp 0xd r6
    jne fail
    li 0x11121314 r0
    cmp r0 r2
    jne fail
    cmp 0x8e r3
    jne fail
    # `dotm`, `learn` with an error of one half for every row, which adds 1
    # to every weight, and `xsh`.
    li _back_4_more r5
    li user_4_more r0
    jmp enter_user
user_4_more:
    li 0x0 r0
    li 0x0 r1
    li 0x0 r2
    dotm r0
    li 0x00404040 r3
    learn r3
    li 0x7f r3
    xsh r3
    int 0x80
_back_4_more:
    cmp 0xd r6
    jne fail
    cmp 0x4a r0
    jne fail
    cmp 0x1a r1
    jne fail
    cmp 0x2a r2
    jne fail
    mov w0 r0
    li 0x12131415 r1
    cmp r1 r0
    jne fail
    mov w1 r0
    li 0x09080706 r1
    cmp r1 r0
    jne fail
    mov w2 r0
    li 0x0d0c0b0a r1
    cmp r1 r0
    jne fail
    mov x0 r0
    li 0x0101017f r1
    cmp r1 r0
    jne fail
    mov x1 r0
    li 0x01010101 r1
    cmp r1 r0
    jne fail
    mov x2 r0
    cmp r1 r0
    jne fail

    # The kernel clears A, and the instructions are privileged again. The
    # kernel itself goes on using the array.
    li 0x5 r7
    li 0x4 r0
    mov r0 cr0
    li 0x0 fp
    li _back_5 r5
    li user_5 r0
    jmp enter_user
user_5:
    li 0x0 r3
    dotv r3
    mov w0 r3
    int 0x80
_back_5:
    cmp 0xf r6
    jne fail
    cmp 0x1 fp
    jne fail
    cmp 0x0 r3
    jne fail
    # The inputs 127, 1, 1, 1 times the weights 21, 20, 19, 18 are 2724;
    # with the 30 and the 46 of the other rows, 2800.
    li 0x0 r3
    dotv r3
    li 0xaf0 r0
    cmp r0 r3
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Enters user mode at the kernel address in `r0`.
enter_user:
    li 0x80000000 r1
    sub r1 r0
    mov r0 cr1
    li 0x20 r0
    mov r0 cr4
    iret

# Continues the test in kernel mode at the address in `r5`.
on_int:
    mov r5 cr1
    push r0
    li 0x0 r0
    mov r0 cr4
    pop r0
    iret

# Counts the exception and returns to the instruction after the refused one.
on_exception:
    add 0x1 r6
    mov cr5 r4
    or r4 fp
    push r0
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret

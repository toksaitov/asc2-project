# Kernel mode and user mode.
#
# The U flag (0x20 in `flags`) is set in user mode. An ordinary write to
# `flags` cannot change it: the kernel enters user mode with `iret`, and the
# CPU returns to kernel mode on every interrupt and exception. In user mode
#
# * `hlt`, `cli`, `sti`, `iret`, `inb`, `outb`, and the moves to and from the
#   control registers raise the privileged instruction exception (vector 1),
#   if the instruction word is a valid one;
# * `sp` is a separate register, which the kernel reaches as `cr6`;
# * the addresses from 0x80000000 up and the device registers of the RP2040
#   raise the bus error exception (vector 5).
#
# Paging is off in this test. The user code then sees physical memory at the
# addresses 0x00000000-0x00ffffff: the code that the kernel runs at
# 0x808xxxxx is the same code at 0x008xxxxx for a user program.
#
# The handlers use these registers:
#
# * `r5`: the address where the test continues in kernel mode after `int`;
# * `r6`: the number of exceptions so far;
# * `r4`, `r3`: the cause (`cr5`) and the address (`cr2`) of the last exception;
# * `r1`, `r2`, `r3`: the saved address, the saved flags, and the cause of
#   the last `int`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp
    mov sp fp

    li 0x80000000 r0
    li on_invalid r1
    sr r1 0(r0)
    li on_privileged r1
    sr r1 4(r0)
    li on_bus_error r1
    sr r1 20(r0)
    li on_int r1
    sr r1 24(r0)

    li 0x0 r6

    # `int` in kernel mode: the handler gets the address of the next
    # instruction, the flags, and the number of the `int` above the vector.
    li 0x1 r7
    li _back_1 r5
    li 0xf r0
    mov r0 flags
    int 0x7f
_back_1:
    cmp r5 r1
    jne fail
    cmp 0xf r2
    jne fail
    cmp 0x7f06 r3
    jne fail

    # Enter user mode and come back. The saved address is a user address, and
    # the saved flags have U.
    li 0x2 r7
    li _back_2 r5
    li user_2 r0
    jmp enter_user
user_2:
    mov flags r0
    int 0x80
_user_2_end:
_back_2:
    cmp 0x20 r0
    jne fail
    li _user_2_end r0
    li 0x80000000 r4
    sub r4 r0
    cmp r0 r1
    jne fail
    cmp 0x20 r2
    jne fail
    li 0x8006 r0
    cmp r0 r3
    jne fail
    # The handler ran in kernel mode, and so does the test again.
    mov flags r0
    test 0x20 r0
    jne fail

    # The privileged instructions raise an exception in user mode and change
    # nothing.
    li 0x3 r7
    li _back_3 r5
    li user_3 r0
    jmp enter_user
user_3:
    li 0x55 r0
    hlt
    cli
    sti
    iret
    inb r0 r0
    outb r0 r0
    mov cr0 r0
    mov r0 cr0
    int 0x80
_back_3:
    cmp 0x8 r6
    jne fail
    cmp 0x1 r4
    jne fail
    cmp 0x55 r0
    jne fail
    # `sti` has not set the interrupt flag, and `cr0` is as the reset has
    # left it, with the F bit (0x4) alone.
    cmp 0x20 r2
    jne fail
    mov cr0 r0
    cmp 0x4 r0
    jne fail

    # A user program can set C, Z, S, and O in `flags`, but not I, and it
    # cannot clear U.
    li 0x4 r7
    li _back_4 r5
    li user_4 r0
    jmp enter_user
user_4:
    li -1 r0
    mov r0 flags
    mov flags r0
    int 0x80
_back_4:
    cmp 0x2f r0
    jne fail
    cmp 0x2f r2
    jne fail
    li _back_4_clear r5
    li user_4_clear r0
    jmp enter_user
user_4_clear:
    li 0x0 r0
    mov r0 flags
    mov flags r0
    int 0x80
_back_4_clear:
    cmp 0x20 r0
    jne fail
    cmp 0x20 r2
    jne fail

    # Each mode has its own stack pointer. The kernel sets the user one
    # through `cr6`.
    li 0x5 r7
    li _back_5 r5
    li 0x28000 r0
    mov r0 cr6
    li user_5 r0
    jmp enter_user
user_5:
    push 0x55
    mov sp r0
    int 0x80
_back_5:
    cmp fp sp
    jne fail
    li 0x27ffc r1
    cmp r1 r0
    jne fail
    mov cr6 r0
    cmp r1 r0
    jne fail
    li 0x80027ffc r1
    lr 0(r1) r0
    cmp 0x55 r0
    jne fail

    # The user stack pointer keeps its value between two visits to user mode.
    li 0x6 r7
    li _back_6 r5
    li user_6 r0
    jmp enter_user
user_6:
    pop r0
    int 0x80
_back_6:
    cmp 0x55 r0
    jne fail
    mov cr6 r0
    li 0x28000 r1
    cmp r1 r0
    jne fail
    cmp fp sp
    jne fail

    # A user program cannot read or write the addresses from 0x80000000 up.
    li 0x7 r7
    li 0x0 r6
    li _back_7 r5
    li user_7 r0
    jmp enter_user
user_7:
    li 0x80001000 r1
    li 0x55 r0
    lr 0(r1) r0
    sr r0 0(r1)
    int 0x80
_back_7:
    cmp 0x2 r6
    jne fail
    cmp 0x55 r0
    jne fail
    # The last exception was the store: bus error (5), a write (0x20000), in
    # user mode (0x40000).
    li 0x60005 r0
    cmp r0 r4
    jne fail
    li 0x80001000 r0
    mov cr2 r1
    cmp r0 r1
    jne fail

    # Without paging a user program reaches physical memory, and the RP2040
    # has nothing above the first 16 MB: it refuses with the status 1.
    li 0x8 r7
    li 0x0 r6
    li _back_8 r5
    li user_8 r0
    jmp enter_user
user_8:
    li 0x01000000 r0
    lrb 0(r0) r1
    li 0x7ffffffc r0
    lr 0(r0) r1
    int 0x80
_back_8:
    cmp 0x2 r6
    jne fail
    li 0x01040005 r0
    cmp r0 r4
    jne fail

    # The device registers of the RP2040 answer the kernel only. The RP2040
    # refuses a user program with the status 2, which the CPU puts in bits
    # 26-24 of the cause.
    li 0x9 r7
    li 0x0 r6
    li _back_9 r5
    li user_9 r0
    jmp enter_user
user_9:
    li 0x00ff0000 r1
    li 0x55 r0
    lr 0(r1) r0
    int 0x80
_back_9:
    cmp 0x1 r6
    jne fail
    cmp 0x55 r0
    jne fail
    li 0x02040005 r0
    cmp r0 r4
    jne fail
    li 0x80ff0000 r0
    lr 0(r0) r1
    # The identification register holds the bytes "ALM2".
    li 0x324d4c41 r0
    cmp r0 r1
    jne fail

    # A user program that jumps to a kernel address stops at the first fetch:
    # bus error (5), a fetch (0x80000), in user mode (0x40000). Both the saved
    # address and the fault address are the target of the jump.
    li 0xa r7
    li 0x0 r6
    li _back_10 r5
    li user_10 r0
    jmp enter_user
user_10:
    li _back_10 r0
    jmp r0
_back_10:
    cmp 0x1 r6
    jne fail
    li 0xc0005 r0
    cmp r0 r4
    jne fail
    cmp r5 r1
    jne fail
    cmp r5 r3
    jne fail
    mov flags r0
    test 0x20 r0
    jne fail

    # The same when `iret` itself names the address: `iret` succeeds, and
    # the fetch after it fails in user mode.
    li 0xb r7
    li 0x0 r6
    li _back_11 r5
    li 0x80800000 r0
    mov r0 cr1
    li 0x20 r0
    mov r0 cr4
    iret
_back_11:
    cmp 0x1 r6
    jne fail
    li 0xc0005 r0
    cmp r0 r4
    jne fail
    li 0x80800000 r0
    cmp r0 r1
    jne fail
    cmp r0 r3
    jne fail

    # A number that is no instruction, although its opcode is the one of
    # `hlt`: a register stands in a field that `hlt` does not use. In user
    # mode it raises the invalid instruction exception (vector 0) and not
    # the privileged one, as the CPU checks the whole word before the mode.
    li 0xc r7
    li 0x0 r6
    li _back_12 r5
    li user_12 r0
    jmp enter_user
user_12:
    .long 0x00000100
    int 0x80
_back_12:
    cmp 0x1 r6
    jne fail
    cmp 0x0 r4
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

# Copies the trap record to `r1`-`r3` and continues the test in kernel mode at
# the address in `r5`.
on_int:
    mov cr1 r1
    mov cr4 r2
    mov cr5 r3
    mov r5 cr1
    push r0
    li 0x0 r0
    mov r0 cr4
    pop r0
    iret

# Counts the exception and returns to the instruction after the bad number.
on_invalid:
    add 0x1 r6
    mov cr5 r4
    push r0
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret

# Counts the exception and returns to the instruction after the refused one.
on_privileged:
    add 0x1 r6
    mov cr5 r4
    push r0
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret

# Counts the exception. After a refused read or write the handler returns to
# the next instruction. A refused fetch has no next instruction to return to,
# so the test continues in kernel mode at the address in `r5`.
on_bus_error:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    push r0
    mov r4 r0
    shr 0x13 r0
    test 0x1 r0
    jne _on_bus_error_fetch
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    pop r0
    iret
_on_bus_error_fetch:
    mov cr1 r1
    mov r5 cr1
    li 0x0 r0
    mov r0 cr4
    pop r0
    iret

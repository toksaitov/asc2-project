# Paging: a user program gets its pages when it first touches them.
#
# The kernel of this test starts a user program with an empty second table.
# Every first access to a page raises a page fault (vector 3). The handler
# puts a page into the table and returns with `iret` to the instruction that
# has failed, which the CPU runs again. The user program notices nothing.
#
# This works because an instruction that raises an exception changes nothing:
# a `push` that cannot store has not moved the stack pointer, and a `pop`
# that cannot load has changed neither the stack pointer nor its register.
#
# The handler gives
#
# * the pages 0x00400000-0x00403fff the boot ROM, read-only: the code of the
#   user program is there (the kernel runs the same ROM at 0x80800000);
# * every other page of 0x00400000-0x007fffff the next unused page of RAM,
#   starting from the physical address 0x20000.
#
# It gives a page only when the cause in `cr5` says that the entry in the
# second table was not present. After any other page fault it continues the
# test in kernel mode, as a kernel that ends the program would.
#
# The handlers use these registers:
#
# * `r5`: the address where the test continues in kernel mode;
# * `r6`: the number of page faults so far;
# * `r4`: the cause (`cr5`) of the last page fault;
# * `r3`: the user stack pointer (`cr6`) at the last page fault;
# * `r1`, `r2`, `r3`: the saved address, the saved flags, and the cause of
#   the last `int`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
# The physical address of the next unused page of RAM.
next_page:
    .long 0x00020000

.section .text
main:
    li 0x80030000 sp

    li 0x80000000 r0
    li on_page_fault r1
    sr r1 12(r0)
    li on_int r1
    sr r1 24(r0)

    # The top table at 0x10000 with one second table at 0x11000, which is
    # empty.
    li 0x80010000 r0
    call clear_page
    li 0x80011000 r0
    call clear_page
    li 0x80010000 r0
    li 0x00011007 r1
    sr r1 4(r0)

    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0
    li 0x0 r6

    # The first instruction of the user program is in a page that is not in
    # the table: the fetch of an instruction (0x80000), from a user program
    # (0x40000), the second table (0x200000). The handler gives the page, and
    # the program runs.
    li 0x1 r7
    li _back_1 r5
    li user_1 r0
    jmp enter_user
user_1:
    li 0x55 r0
    int 0x80
_back_1:
    cmp 0x55 r0
    jne fail
    cmp 0x1 r6
    jne fail
    li 0x002c0003 r0
    cmp r0 r4
    jne fail
    # The fault address is the address of the first instruction.
    li user_1 r0
    li 0x80400000 r1
    sub r1 r0
    mov cr2 r1
    cmp r0 r1
    jne fail

    # A store to a page that is not in the table: a write (0x20000), from a
    # user program (0x40000), the second table (0x200000). The handler gives
    # the first unused page of RAM, the store runs again, and the program
    # reads its value back.
    li 0x2 r7
    li 0x0 r6
    li _back_2 r5
    li user_2 r0
    jmp enter_user
user_2:
    li 0x00413000 r1
    li 0x9abc r0
    sr r0 8(r1)
    lr 8(r1) r0
    add 0x1 r0
    int 0x80
_back_2:
    li 0x9abd r1
    cmp r1 r0
    jne fail
    cmp 0x1 r6
    jne fail
    li 0x00260003 r0
    cmp r0 r4
    jne fail
    mov cr2 r1
    li 0x00413008 r0
    cmp r0 r1
    jne fail
    li 0x80020000 r1
    lr 8(r1) r0
    li 0x9abc r1
    cmp r1 r0
    jne fail

    # A `push` to a page that is not in the table. At the page fault the
    # stack pointer still has its old value; the handler gives the page, and
    # the `push` runs again.
    li 0x3 r7
    li 0x0 r6
    li 0x00800000 r0
    mov r0 cr6
    li _back_3 r5
    li user_3 r0
    jmp enter_user
user_3:
    push 0x77
    int 0x80
_back_3:
    cmp 0x1 r6
    jne fail
    li 0x00260003 r0
    cmp r0 r4
    jne fail
    mov cr2 r1
    li 0x007ffffc r0
    cmp r0 r1
    jne fail
    mov cr6 r1
    cmp r0 r1
    jne fail
    li 0x80021000 r1
    lr 0xffc(r1) r0
    cmp 0x77 r0
    jne fail

    # The same for a `pop`. The kernel puts a value where the handler's next
    # page of RAM will be, and the user program pops it.
    li 0x4 r7
    li 0x0 r6
    li 0x80022000 r1
    li 0x1357 r0
    sr r0 0(r1)
    li 0x007fe000 r0
    mov r0 cr6
    li _back_4 r5
    li user_4 r0
    jmp enter_user
user_4:
    li 0x55 r0
    pop r0
    int 0x80
_back_4:
    cmp 0x1357 r0
    jne fail
    cmp 0x1 r6
    jne fail
    li 0x00240003 r0
    cmp r0 r4
    jne fail
    mov cr6 r1
    li 0x007fe004 r0
    cmp r0 r1
    jne fail

    # The pages are in the table now: the same accesses raise nothing.
    li 0x5 r7
    li 0x0 r6
    li 0x00800000 r0
    mov r0 cr6
    li _back_5 r5
    li user_5 r0
    jmp enter_user
user_5:
    push 0x78
    pop r0
    li 0x00413000 r1
    sr r0 8(r1)
    int 0x80
_back_5:
    cmp 0x78 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # A write to the code of the program is not a missing page: the entry is
    # present (0x10000). The handler gives nothing, and the test continues in
    # kernel mode.
    li 0x6 r7
    li 0x0 r6
    li _back_6 r5
    li user_6 r0
    jmp enter_user
user_6:
    li 0x00400000 r1
    sr r0 0(r1)
    int 0x81
_back_6:
    cmp 0x1 r6
    jne fail
    li 0x00070003 r0
    cmp r0 r4
    jne fail
    mov flags r0
    test 0x20 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Fills the page at the address in `r0` with zeros.
clear_page:
    mov r0 r1
    add 0x1000 r1
    li 0x0 r2
_clear_page_next:
    sr r2 0(r0)
    add 0x4 r0
    cmp r1 r0
    jne _clear_page_next
    ret

# Continues in user mode at the code that the kernel sees at the address in
# `r0`: the ROM is at 0x80800000 for the kernel and at 0x00400000 for a user
# program.
enter_user:
    li 0x80400000 r1
    sub r1 r0
    mov r0 cr1
    li 0x20 r0
    mov r0 cr4
    iret

# Copies the trap record to `r1`-`r3` and continues the test in kernel mode at
# the address in `r5`. `int 0x81` is the user program's sign that something
# has gone wrong.
on_int:
    mov cr1 r1
    mov cr4 r2
    mov cr5 r3
    mov r3 fp
    shr 0x8 fp
    cmp 0x80 fp
    jne fail
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

on_page_fault:
    # The handler runs on the kernel stack and saves the registers it uses,
    # so the user program finds its registers unchanged.
    push r0
    push r1
    push r2

    add 0x1 r6
    mov cr5 r4
    mov cr6 r3

    # Bits 21-16 of the cause, with the bits for a write, a user program, and
    # a fetch masked out: the page is missing if "present" (0x01) is clear and
    # the second table (0x20) stopped the access.
    mov r4 r0
    shr 0x10 r0
    and 0x31 r0
    cmp 0x20 r0
    jne _on_page_fault_refused

    # The address of the page's entry: the second table at 0x11000 has 4
    # bytes for each page.
    mov cr2 r0
    shr 0xc r0
    and 0x3ff r0
    mov r0 r2
    shl 0x2 r0
    li 0x80011000 r1
    add r1 r0

    # The first four pages are the boot ROM (0x800000), for user programs,
    # read-only.
    cmp 0x4 r2
    jae _on_page_fault_ram
    shl 0xc r2
    li 0x00800005 r1
    add r1 r2
    sr r2 0(r0)
    jmp _on_page_fault_end

_on_page_fault_ram:
    # Any other page is the next unused page of RAM, for user programs, with
    # write access.
    li next_page r1
    lr 0(r1) r2
    or 0x7 r2
    sr r2 0(r0)
    lr 0(r1) r2
    add 0x1000 r2
    sr r2 0(r1)

_on_page_fault_end:
    pop r2
    pop r1
    pop r0
    # Back to the instruction that has failed.
    iret

_on_page_fault_refused:
    pop r2
    pop r1
    pop r0
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

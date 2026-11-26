# Paging: the translation of addresses through two levels of page tables.
#
# With paging on (bit 0 of `cr0`) the CPU translates every address below
# 0x80000000. It splits the address into three parts:
#
#     bits 31-22: the index of an entry in the top table
#     bits 21-12: the index of an entry in a second table
#     bits 11-0:  the offset inside a page of 4096 bytes
#
# `cr3` holds the physical address of the top table. An entry of the top
# table gives the physical address of a second table, and an entry of the
# second table gives the physical address of the page. An entry is 4 bytes:
#
#     bit 0 (P): the entry is present
#     bit 1 (W): the page can be written
#     bit 2 (U): a user program can use the page
#     bits 31-12: bits 31-12 of the physical address
#
# The CPU reads the two entries from memory by itself and remembers the last
# 8 translations, so it does not read the entries again on the next access to
# the same page.
#
# An access that the entries do not allow raises the page fault exception
# (vector 3). The address is in `cr2` and the cause is in `cr5`:
#
#     bits 7-0:   the vector (3)
#     0x010000:   the entry was present, and the access was refused
#     0x020000:   the access was a write
#     0x040000:   the access came from a user program
#     0x080000:   the access was the fetch of an instruction
#     bits 21-20: which entry stopped the access: 1 for the top table, 2 for
#                 the second table, 0 for a remembered translation
#
# The addresses from 0x80000000 up are not translated: the kernel keeps
# reaching physical memory there.
#
# This test builds these tables (physical addresses on the right):
#
#     0x00400000-0x00403fff  the boot ROM, for user programs, read-only
#     0x00410000             0x20000, for user programs
#     0x00411000             0x21000, for user programs, read-only
#     0x00412000             0x22000, for the kernel only
#
# The user code of this test is in the boot ROM. The kernel runs the ROM at
# 0x80800000, and a user program sees the same code at 0x00400000.
#
# The handlers use these registers:
#
# * `r5`: the address where the test continues in kernel mode after `int`;
# * `r6`: the number of exceptions so far;
# * `r4`, `r3`: the cause (`cr5`) and the address (`cr2`) of the last exception;
# * `r1`, `r2`, `r3`: the saved address, the saved flags, and the cause of
#   the last `int`;
# * `fp`: values of the handlers themselves.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80030000 sp

    li 0x80000000 r0
    li on_fault r1
    sr r1 12(r0)
    sr r1 20(r0)
    li on_int r1
    sr r1 24(r0)

    # The top table at 0x10000: the entry 1 covers 0x00400000-0x007fffff and
    # points to the second table at 0x11000.
    li 0x80010000 r0
    call clear_page
    li 0x80010000 r0
    li 0x00011007 r1
    sr r1 4(r0)

    # The second table at 0x11000.
    li 0x80011000 r0
    call clear_page
    li 0x80011000 r0
    li 0x00800005 r1
    sr r1 0x0(r0)
    li 0x00801005 r1
    sr r1 0x4(r0)
    li 0x00802005 r1
    sr r1 0x8(r0)
    li 0x00803005 r1
    sr r1 0xc(r0)
    li 0x00020007 r1
    sr r1 0x40(r0)
    li 0x00021005 r1
    sr r1 0x44(r0)
    li 0x00022003 r1
    sr r1 0x48(r0)

    # `cr3` keeps bits 31-12 only.
    li 0x1 r7
    li -1 r0
    mov r0 cr3
    mov cr3 r1
    li 0xfffff000 r0
    cmp r0 r1
    jne fail

    # Turn paging on. The code, the stack, and the tables are all above
    # 0x80000000, so the kernel goes on as before.
    li 0x2 r7
    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0
    mov cr0 r1
    cmp 0x1 r1
    jne fail
    li 0x0 r6

    # A write through the tables lands in the physical page of the entry.
    li 0x3 r7
    li 0x00410000 r1
    li 0x1234 r0
    sr r0 8(r1)
    li 0x80020000 r1
    lr 8(r1) r2
    cmp r0 r2
    jne fail
    # And a read through the tables finds what is in the physical page.
    li 0x5678 r0
    sr r0 12(r1)
    li 0x00410000 r1
    lr 12(r1) r2
    cmp r0 r2
    jne fail
    cmp 0x0 r6
    jne fail

    # The boot ROM at its user address.
    li 0x4 r7
    li 0x00400000 r1
    lr 0(r1) r0
    li 0x80800000 r1
    lr 0(r1) r2
    cmp r2 r0
    jne fail
    li 0x00403ffc r1
    lr 0(r1) r0
    cmp 0x0 r6
    jne fail

    # The kernel uses a page without U.
    li 0x5 r7
    li 0x00412000 r1
    li 0x4321 r0
    sr r0 0(r1)
    li 0x80022000 r1
    lr 0(r1) r2
    cmp r0 r2
    jne fail
    cmp 0x0 r6
    jne fail

    # A page without W cannot be written even by the kernel. The second
    # table's entry stops the write: present (0x10000), a write (0x20000),
    # the second table (0x200000).
    li 0x6 r7
    li 0x0 r6
    call flush
    li 0x00411000 r1
    li 0x1 r0
    sr r0 0(r1)
    cmp 0x1 r6
    jne fail
    li 0x00230003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail
    # A read is allowed, and the CPU remembers the translation.
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    # The next write is stopped by the remembered translation.
    sr r0 0(r1)
    cmp 0x2 r6
    jne fail
    li 0x00030003 r0
    cmp r0 r4
    jne fail

    # An address whose entry in the top table is not present.
    li 0x7 r7
    li 0x0 r6
    li 0x00000000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00100003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail
    li 0x7ffffffc r1
    sr r0 0(r1)
    cmp 0x2 r6
    jne fail
    li 0x00120003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail

    # An address whose entry in the second table is not present.
    li 0x8 r7
    li 0x0 r6
    li 0x00414ffc r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00200003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail

    # The addresses from 0x81000000 up have nothing behind them, as without
    # paging: the RP2040 refuses with the status 1.
    li 0x9 r7
    li 0x0 r6
    li 0x81000000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x01000005 r0
    cmp r0 r4
    jne fail

    # A user program reads and writes its page.
    li 0xa r7
    li 0x0 r6
    call flush
    li _back_10 r5
    li user_10 r0
    jmp enter_user
user_10:
    li 0x00410000 r1
    li 0x5678 r0
    sr r0 0x10(r1)
    lr 0x10(r1) r0
    add 0x1 r0
    int 0x80
_back_10:
    cmp 0x0 r6
    jne fail
    li 0x5679 r1
    cmp r1 r0
    jne fail
    li 0x80020000 r1
    lr 0x10(r1) r0
    li 0x5678 r1
    cmp r1 r0
    jne fail
    # The user program ran in user mode.
    cmp 0x20 r2
    jne fail

    # A user program writes a page without W: present (0x10000), a write
    # (0x20000), from a user program (0x40000), the second table (0x200000).
    li 0xb r7
    li 0x0 r6
    call flush
    li _back_11 r5
    li user_11 r0
    jmp enter_user
user_11:
    li 0x00411000 r1
    sr r0 0(r1)
    int 0x80
_back_11:
    cmp 0x1 r6
    jne fail
    li 0x00270003 r0
    cmp r0 r4
    jne fail

    # The user program reads the page and writes it again: this time the
    # remembered translation stops the write.
    li 0xc r7
    li 0x0 r6
    li _back_12 r5
    li user_12 r0
    jmp enter_user
user_12:
    li 0x00411000 r1
    lr 0(r1) r0
    sr r0 0(r1)
    int 0x80
_back_12:
    cmp 0x1 r6
    jne fail
    li 0x00070003 r0
    cmp r0 r4
    jne fail

    # A user program reads a page without U.
    li 0xd r7
    li 0x0 r6
    call flush
    li _back_13 r5
    li user_13 r0
    jmp enter_user
user_13:
    li 0x00412000 r1
    li 0x55 r0
    lr 0(r1) r0
    int 0x80
_back_13:
    cmp 0x1 r6
    jne fail
    cmp 0x55 r0
    jne fail
    li 0x00250003 r0
    cmp r0 r4
    jne fail
    mov cr2 r1
    li 0x00412000 r0
    cmp r0 r1
    jne fail

    # The kernel reads the page, so the CPU remembers its translation. The
    # user program is stopped all the same.
    li 0xe r7
    li 0x0 r6
    li 0x00412000 r1
    lr 0(r1) r0
    li _back_14 r5
    li user_14 r0
    jmp enter_user
user_14:
    li 0x00412000 r1
    lr 0(r1) r0
    int 0x80
_back_14:
    cmp 0x1 r6
    jne fail
    li 0x00050003 r0
    cmp r0 r4
    jne fail

    # A user program cannot use the addresses from 0x80000000 up.
    li 0xf r7
    li 0x0 r6
    li _back_15 r5
    li user_15 r0
    jmp enter_user
user_15:
    li 0x80010000 r1
    lr 0(r1) r0
    int 0x80
_back_15:
    cmp 0x1 r6
    jne fail
    li 0x00040005 r0
    cmp r0 r4
    jne fail

    # A user program jumps to a page that is not in the tables: the fetch of
    # an instruction (0x80000), from a user program (0x40000), the second
    # table (0x200000). Both the saved address and the fault address are the
    # target of the jump.
    li 0x10 r7
    li 0x0 r6
    li _back_16 r5
    li user_16 r0
    jmp enter_user
user_16:
    li 0x00500000 r0
    jmp r0
_back_16:
    cmp 0x1 r6
    jne fail
    li 0x002c0003 r0
    cmp r0 r4
    jne fail
    li 0x00500000 r0
    cmp r0 r3
    jne fail
    cmp r0 r1
    jne fail

    # Turn paging off: the low addresses are physical addresses again.
    li 0x11 r7
    li 0x0 r6
    li 0x0 r0
    mov r0 cr0
    li 0x00020000 r1
    lr 0x10(r1) r0
    li 0x5678 r1
    cmp r1 r0
    jne fail
    cmp 0x0 r6
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

# Makes the CPU forget the translations it remembers: every write to `cr3`
# does that.
flush:
    mov cr3 r0
    mov r0 cr3
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
# the address in `r5`.
on_int:
    mov cr1 r1
    mov cr4 r2
    mov cr5 r3
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

# Counts the exception. After a refused read or write the handler returns to
# the next instruction. A refused fetch has no next instruction to return to,
# so the test continues in kernel mode at the address in `r5`.
on_fault:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    mov r4 fp
    shr 0x13 fp
    test 0x1 fp
    jne _on_fault_fetch
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret
_on_fault_fetch:
    mov cr1 r1
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

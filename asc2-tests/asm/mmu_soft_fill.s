# Paging: the kernel reads the page tables instead of the CPU.
#
# With bit 1 of `cr0` set next to the paging bit, the CPU does not read the
# page tables. An access to a page whose translation the CPU does not
# remember raises the translation miss exception (vector 4) with the address
# in `cr2`. The handler finds the translation in any way it likes and hands
# it to the CPU: it writes a page table entry to `cr7`, and the CPU remembers
# it as the translation of the page of the address in `cr2`. Then `iret`
# runs the instruction again.
#
# Processors of the MIPS family work this way. This CPU has the mode for
# teaching, and to keep the chip useful should its own reading of the tables
# turn out to be broken.
#
# The handler here reads the same two tables that the CPU reads by itself:
#
#     0x00400000-0x00403fff  the boot ROM, for user programs, read-only
#     0x00410000             0x20000, for user programs
#     0x00411000             0x21000, for user programs, read-only
#     0x00412000             0x22000, for the kernel only
#     0x00810000             0x20000 again, through an entry of the top
#                            table that is for the kernel only
#
# An access needs the permission in both entries, the one of the top table
# and the one of the second table. The CPU checks both when it reads the
# tables by itself, and the handler must do the same.
#
# The handlers use these registers:
#
# * `r5`: the address where the test continues in kernel mode after `int`;
# * `r6`: the number of translation misses so far;
# * `r4`: the cause (`cr5`) of the last translation miss;
# * `r2`: the number of accesses that were refused and skipped so far;
# * `r3`: the cause of the last refused access;
# * `fp`: values of the handlers themselves.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80030000 sp

    li 0x80000000 r0
    li on_refused r1
    sr r1 12(r0)
    li on_miss r1
    sr r1 16(r0)
    li on_int r1
    sr r1 24(r0)

    li 0x80010000 r0
    call clear_page
    li 0x80010000 r0
    li 0x00011007 r1
    sr r1 4(r0)
    li 0x00011003 r1
    sr r1 8(r0)

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

    li 0x80020000 r1
    li 0x1234 r0
    sr r0 0(r1)

    # Paging with the kernel reading the tables.
    li 0x1 r7
    li 0x00010000 r0
    mov r0 cr3
    li 0x3 r0
    mov r0 cr0
    mov cr0 r1
    cmp 0x3 r1
    jne fail
    li 0x0 r6
    li 0x0 r2

    # The first read of a page raises one translation miss; the handler hands
    # the translation to the CPU, and the read runs again.
    li 0x2 r7
    li 0x00410000 r1
    lr 0(r1) r0
    cmp 0x1234 r0
    jne fail
    cmp 0x1 r6
    jne fail
    cmp 0x4 r4
    jne fail
    mov cr2 r0
    cmp r1 r0
    jne fail

    # The CPU remembers the translation: more reads and writes raise nothing.
    li 0x3 r7
    lr 4(r1) r0
    sr r0 8(r1)
    cmp 0x1 r6
    jne fail

    # A write that misses has the write bit (0x20000) in its cause.
    li 0x4 r7
    li 0x0 r6
    call flush
    sr r0 8(r1)
    cmp 0x1 r6
    jne fail
    li 0x00020004 r0
    cmp r0 r4
    jne fail

    # A write to a page without W: first the miss, then, with the translation
    # in the CPU, a page fault (vector 3): present (0x10000), a write
    # (0x20000), a remembered translation (no table).
    li 0x5 r7
    li 0x0 r6
    li 0x00411000 r1
    sr r0 0(r1)
    cmp 0x1 r6
    jne fail
    cmp 0x1 r2
    jne fail
    li 0x00030003 r0
    cmp r0 r3
    jne fail

    # A page that is not in the tables: the handler finds no entry and has
    # nothing to hand to the CPU. A kernel would treat this as a page fault;
    # the handler here skips the access.
    li 0x6 r7
    li 0x0 r6
    li 0x0 r2
    li 0x00414000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    cmp 0x1 r2
    jne fail
    cmp 0x4 r3
    jne fail

    # A user program: the fetch of its first instruction misses (0x80000 for
    # a fetch, 0x40000 for a user program).
    li 0x7 r7
    li 0x0 r6
    li 0x0 r2
    call flush
    li _back_7 r5
    li user_7 r0
    jmp enter_user
user_7:
    int 0x80
_back_7:
    cmp 0x1 r6
    jne fail
    li 0x000c0004 r0
    cmp r0 r4
    jne fail

    # The user program reads its page: one more miss, for the data.
    li 0x8 r7
    li 0x0 r6
    li _back_8 r5
    li user_8 r0
    jmp enter_user
user_8:
    li 0x00410000 r1
    lr 0(r1) r0
    int 0x80
_back_8:
    cmp 0x1234 r0
    jne fail
    cmp 0x1 r6
    jne fail
    li 0x00040004 r0
    cmp r0 r4
    jne fail
    cmp 0x0 r2
    jne fail

    # The user program writes a page without U: the miss, then a page fault.
    li 0x9 r7
    li 0x0 r6
    li _back_9 r5
    li user_9 r0
    jmp enter_user
user_9:
    li 0x00412000 r1
    sr r0 0(r1)
    int 0x80
_back_9:
    cmp 0x1 r6
    jne fail
    li 0x00060004 r0
    cmp r0 r4
    jne fail
    cmp 0x1 r2
    jne fail
    li 0x00070003 r0
    cmp r0 r3
    jne fail

    # The user program reads a page that the second table gives to user
    # programs and the top table does not: the miss, then a page fault
    # (present, a user program). The kernel reads the same page.
    li 0xa r7
    li 0x0 r6
    li 0x0 r2
    li _back_10 r5
    li user_10 r0
    jmp enter_user
user_10:
    li 0x00810000 r1
    lr 0(r1) r0
    int 0x80
_back_10:
    cmp 0x1 r6
    jne fail
    li 0x00040004 r0
    cmp r0 r4
    jne fail
    cmp 0x1 r2
    jne fail
    li 0x00050003 r0
    cmp r0 r3
    jne fail
    li 0x00810000 r1
    lr 0(r1) r0
    cmp 0x1234 r0
    jne fail

    # With bit 1 of `cr0` clear the CPU reads the tables by itself again. The
    # change of the bit has made it forget the translations, and still no
    # miss is raised.
    li 0xb r7
    li 0x0 r6
    li 0x1 r0
    mov r0 cr0
    li 0x00410000 r1
    lr 0(r1) r0
    cmp 0x1234 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # And back: the CPU has forgotten the translation that it made by itself.
    li 0xc r7
    li 0x3 r0
    mov r0 cr0
    lr 0(r1) r0
    cmp 0x1234 r0
    jne fail
    cmp 0x1 r6
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

# Makes the CPU forget the translations it remembers.
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

# Continues the test in kernel mode at the address in `r5`.
on_int:
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

# Counts a refused access and returns to the instruction after it.
on_refused:
    add 0x1 r2
    mov cr5 r3
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret

# Finds the translation of the address in `cr2` in the page tables and hands
# it to the CPU, without the permissions that the entry of the top table does
# not give.
on_miss:
    push r0
    push r1

    add 0x1 r6
    mov cr5 r4

    # The entry of the top table: the table is at the physical address in
    # `cr3` and has 4 bytes for each value of bits 31-22 of the address. The
    # kernel reaches physical memory at 0x80000000.
    mov cr2 r0
    shr 0x16 r0
    shl 0x2 r0
    mov cr3 r1
    add r1 r0
    li 0x80000000 r1
    add r1 r0
    lr 0(r0) r0
    test 0x1 r0
    je _on_miss_no_page
    mov r0 fp

    # The entry of the second table: the table is at the physical address in
    # bits 31-12 of the top entry and has 4 bytes for each value of bits 21-12
    # of the address. A table lies in the first 8 MB of physical memory; the
    # chip refuses an entry that says otherwise, and so does this code.
    mov r0 r1
    shr 0x17 r1
    jne _on_miss_no_page
    li 0xfffff000 r1
    and r1 r0
    li 0x80000000 r1
    add r1 r0
    mov cr2 r1
    shr 0xc r1
    and 0x3ff r1
    shl 0x2 r1
    add r1 r0
    lr 0(r0) r0
    test 0x1 r0
    je _on_miss_no_page

    # The entry loses the permissions that the entry of the top table, kept
    # in `fp`, does not have: 0x2 for writes and 0x4 for user programs.
    or -0x7 fp
    and fp r0

    # The CPU takes the entry as the translation of the page of the address
    # in `cr2`. `iret` returns to the instruction that has missed.
    mov r0 cr7
    pop r1
    pop r0
    iret

_on_miss_no_page:
    pop r1
    pop r0
    add 0x1 r2
    mov cr5 r3
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret

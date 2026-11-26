# Paging: page table entries that the CPU does not accept.
#
# See `mmu_lookup.s` for the tables and for the cause of a page fault. This
# test adds the rest of the rules:
#
# * An entry that is not present may hold anything in its other bits.
# * Bits 31-12 of an entry are a physical page, anywhere in the 4 GB of
#   physical addresses. The RP2040 serves one page above the first 16 MB: the
#   high page at 0xfffff000.
# * A table is in RAM, in the first 8 MB of the physical addresses (below
#   0x800000). The CPU reads no entry from the boot ROM, from the device
#   registers, or from above: reading a device register changes the device.
#   The cause of the page fault then has 0x400000 (a malformed entry).
# * If the RP2040 refuses the read of an entry, the cause has the status of
#   the RP2040 in bits 26-24.
# * Bits 11-3 of an entry are ignored.
# * An access needs the permission of both entries, the one in the top table
#   and the one in the second table.
# * A lookup that fails leaves nothing behind: once the kernel has repaired
#   the entry, the next access works.
#
# The top table of this test (at the physical address 0x10000):
#
#     entry 1  0x00400000  a second table at 0x11000
#     entry 2  0x00800000  a second table at 0x12000, without W
#     entry 3  0x00c00000  a second table at 0x13000, without U
#     entry 4  0x01000000  a second table above the first 16 MB
#     entry 5  0x01400000  a second table in the device registers
#     entry 6  0x01800000  a second table where no memory is
#     entry 7  0x01c00000  not present, all other bits set
#     entry 8  0x02000000  a second table in the boot ROM
#
# The second table at 0x11000:
#
#     entries 0-3  0x00400000  the boot ROM, for user programs, read-only
#     entry 0x10   0x00410000  0x20000, for user programs
#     entry 0x11   0x00411000  the high page at 0xfffff000, for user programs
#     entry 0x12   0x00412000  not present, all other bits set
#     entry 0x13   0x00413000  0x20000, for the kernel, bits 11-3 set
#     entry 0x14   0x00414000  the device registers, for user programs
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

    li 0x80010000 r0
    call clear_page
    li 0x80011000 r0
    call clear_page
    li 0x80012000 r0
    call clear_page
    li 0x80013000 r0
    call clear_page

    li 0x80010000 r0
    li 0x00011007 r1
    sr r1 0x4(r0)
    li 0x00012005 r1
    sr r1 0x8(r0)
    li 0x00013003 r1
    sr r1 0xc(r0)
    li 0xff011007 r1
    sr r1 0x10(r0)
    li 0x00ff0007 r1
    sr r1 0x14(r0)
    li 0x00100007 r1
    sr r1 0x18(r0)
    li 0xfffffffe r1
    sr r1 0x1c(r0)
    li 0x00800007 r1
    sr r1 0x20(r0)

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
    li 0xfffff007 r1
    sr r1 0x44(r0)
    li 0xfffffffe r1
    sr r1 0x48(r0)
    li 0x00020ffb r1
    sr r1 0x4c(r0)
    li 0x00ff0007 r1
    sr r1 0x50(r0)

    # Both of these second tables give a page for user programs that can be
    # written; their entries in the top table take a permission away.
    li 0x80012000 r0
    li 0x00021007 r1
    sr r1 0x0(r0)
    li 0x80013000 r0
    li 0x00022007 r1
    sr r1 0x0(r0)

    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0

    # A word to recognize the page at 0x20000 by.
    li 0x80020000 r1
    li 0x1234 r0
    sr r0 0(r1)

    # An entry of the top table that names a second table above the first
    # 16 MB: malformed (0x400000), stopped at the second table (0x200000),
    # which the CPU does not read.
    li 0x1 r7
    li 0x0 r6
    li 0x01000000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00600003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail

    # An entry of the second table with a page above the first 16 MB: the
    # high page. A word written there comes back, and it is not the word of
    # the page at 0x20000, whose address has the same bits 23-12 as
    # 0xff020000 would have.
    li 0x2 r7
    li 0x0 r6
    li 0x00411000 r1
    li 0x4321 r0
    sr r0 0(r1)
    srb r0 0x3ff(r1)
    lr 0(r1) r0
    cmp 0x4321 r0
    jne fail
    lrbu 0x3ff(r1) r0
    cmp 0x21 r0
    jne fail
    li 0x80020000 r2
    lr 0(r2) r0
    cmp 0x1234 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # The kernel changes the entry to the page at 0x20000 and writes `cr3`,
    # and the same address reads the other page.
    li 0x3 r7
    li 0x0 r6
    li 0x80011000 r0
    li 0x00020007 r2
    sr r2 0x44(r0)
    mov cr3 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0x0 r6
    jne fail
    cmp 0x1234 r0
    jne fail

    # Two pages where no memory is, 0x5a5a5000 and 0xa5a5a000: every bit of
    # the page number is 1 in one of them and 0 in the other. The RP2040
    # answers each read with the status 1. The list of the requests on the
    # bus, which the chip must send as the emulator does, shows that every
    # bit of the page number has arrived in its place.
    li 0x80011000 r0
    li 0x5a5a5007 r2
    sr r2 0x44(r0)
    mov cr3 r2
    mov r2 cr3
    lr 0x5a4(r1) r2
    cmp 0x1 r6
    jne fail
    li 0x01000005 r2
    cmp r2 r4
    jne fail
    li 0xa5a5a007 r2
    sr r2 0x44(r0)
    mov cr3 r2
    mov r2 cr3
    lr 0xa58(r1) r2
    cmp 0x2 r6
    jne fail
    li 0x01000005 r2
    cmp r2 r4
    jne fail

    # A second table in the device registers: the CPU does not read it.
    li 0x4 r7
    li 0x0 r6
    li 0x01400000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00600003 r0
    cmp r0 r4
    jne fail

    # A second table where no memory is: the RP2040 answers the read of the
    # entry with the status 1.
    li 0x5 r7
    li 0x0 r6
    li 0x01800000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x01200003 r0
    cmp r0 r4
    jne fail

    # Entries that are not present with all other bits set: plain "not
    # present" faults, in the top table and in the second table.
    li 0x6 r7
    li 0x0 r6
    li 0x01c00000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00100003 r0
    cmp r0 r4
    jne fail
    li 0x00412000 r1
    lr 0(r1) r0
    cmp 0x2 r6
    jne fail
    li 0x00200003 r0
    cmp r0 r4
    jne fail

    # Bits 11-3 of an entry mean nothing.
    li 0x7 r7
    li 0x0 r6
    li 0x00413000 r1
    lr 0(r1) r0
    cmp 0x0 r6
    jne fail
    cmp 0x1234 r0
    jne fail

    # The top entry has no W: a write is stopped at the top table (0x100000),
    # present (0x10000), a write (0x20000).
    li 0x8 r7
    li 0x0 r6
    li 0x00800000 r1
    sr r0 0(r1)
    cmp 0x1 r6
    jne fail
    li 0x00130003 r0
    cmp r0 r4
    jne fail
    # A read is allowed. The remembered translation has no W either, though
    # the entry in the second table has it.
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    sr r0 0(r1)
    cmp 0x2 r6
    jne fail
    li 0x00030003 r0
    cmp r0 r4
    jne fail

    # The top entry has no U: a user program is stopped at the top table.
    li 0x9 r7
    li 0x0 r6
    li _back_9 r5
    li user_9 r0
    jmp enter_user
user_9:
    li 0x00c00000 r1
    lr 0(r1) r0
    int 0x80
_back_9:
    cmp 0x1 r6
    jne fail
    li 0x00150003 r0
    cmp r0 r4
    jne fail
    # The kernel may read the page. The remembered translation has no U
    # either.
    li 0x0 r6
    li 0x00c00000 r1
    lr 0(r1) r0
    cmp 0x0 r6
    jne fail
    li _back_9_again r5
    li user_9 r0
    jmp enter_user
_back_9_again:
    cmp 0x1 r6
    jne fail
    li 0x00050003 r0
    cmp r0 r4
    jne fail

    # A page can be in the device registers. The kernel reads the
    # identification register (the bytes "ALM2") through the tables.
    li 0xa r7
    li 0x0 r6
    li 0x00414000 r1
    lr 0(r1) r0
    li 0x324d4c41 r2
    cmp r2 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # The entry has U, but the RP2040 answers the kernel only: a user program
    # gets a bus error (vector 5) with the status 2.
    li 0xb r7
    li 0x0 r6
    li _back_11 r5
    li user_11 r0
    jmp enter_user
user_11:
    li 0x00414000 r1
    lr 0(r1) r0
    int 0x80
_back_11:
    cmp 0x1 r6
    jne fail
    li 0x02040005 r0
    cmp r0 r4
    jne fail

    # `cr3` in the device registers: the CPU does not read the top table.
    li 0xc r7
    li 0x0 r6
    li 0x00ff0000 r0
    mov r0 cr3
    li 0x00410000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00500003 r0
    cmp r0 r4
    jne fail

    # `cr3` where no memory is.
    li 0xd r7
    li 0x0 r6
    li 0x00100000 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x01100003 r0
    cmp r0 r4
    jne fail

    # `cr3` in the boot ROM: the CPU does not read the top table there.
    li 0xe r7
    li 0x0 r6
    li 0x00800000 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00500003 r0
    cmp r0 r4
    jne fail

    # With `cr3` back at the top table the access works.
    li 0xf r7
    li 0x0 r6
    li 0x00010000 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0x0 r6
    jne fail
    cmp 0x1234 r0
    jne fail

    # A second table in the boot ROM: the CPU does not read it.
    li 0x10 r7
    li 0x0 r6
    li 0x02000000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00600003 r0
    cmp r0 r4
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
# the address in `r5`.
on_int:
    mov cr1 r1
    mov cr4 r2
    mov cr5 r3
    mov r5 cr1
    li 0x0 fp
    mov fp cr4
    iret

# Counts the exception and returns to the instruction after the refused one.
on_fault:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret

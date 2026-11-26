# An answer that never comes: the bus status 5.
#
# The chip sends a request again when the answer of the RP2040 does not
# arrive whole, and the RP2040 answers a repeated request from a copy, so
# that no access is done twice. After four requests without an answer the
# chip gives up, and the access ends with the bus status 5. For an
# instruction or its data that is the bus error exception with the status in
# the bits 26:24 of the cause. For an entry of a page table it is a page
# fault with the same bits.
#
# The chip does not know then whether the RP2040 has done the access. This
# test shows the case in which it has: a byte of the console is taken, a
# write is done, and the CPU has seen nothing of it. As after every
# exception, the instruction has changed no register, so the kernel is free
# to run it again; it has to decide whether that is right.
#
# A register of the RP2040 orders the loss for a test:
#
#     0x80ff0030  a write of n makes the RP2040 do the n-th access after
#                 the write and never answer it; a write of 0 calls the
#                 order off; reads as the number of accesses left
#
# Every request of the CPU counts: a fetch, a read, a write, and a read of
# a page table entry.
#
# The console of this test gets the bytes "AB" from `rp_lost_answer.in`.
#
# The handlers count in `r6` and leave the cause (`cr5`) in `r4`, the address
# that failed (`cr2`) in `r3`, and the saved address (`cr1`) in `r5`. After a
# fetch that has failed, the handler returns to the same instruction, which
# runs then. Every other instruction is skipped.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_page_fault r1
    sr r1 12(r0)
    li on_bus_error r1
    sr r1 20(r0)

    # Sequential fetch on: a lost fetch must not confuse it.
    li 0x0 r0
    mov r0 cr0

    li 0x0 r6
    li 0x80ff0000 fp

    # The register reads as zero, counts down with every access, and a write
    # of zero calls the order off.
    li 0x1 r7
    lr 0x30(fp) r0
    cmp 0x0 r0
    jne fail
    li 0x64 r0
    sr r0 0x30(fp)
    lr 0x30(fp) r0
    cmp 0x62 r0
    jne fail
    li 0x0 r0
    sr r0 0x30(fp)
    lr 0x30(fp) r0
    cmp 0x0 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # A read of the console. The first access after the order is the fetch
    # of the `lr`, and the second is its read.
    li 0x2 r7
    li 0x5555 r2
    li 0x2 r0
    sr r0 0x30(fp)
_lost_2:
    lr 0x14(fp) r2
    cmp 0x1 r6
    jne fail
    li 0x05000005 r0
    cmp r0 r4
    jne fail
    li 0x80ff0014 r0
    cmp r0 r3
    jne fail
    li _lost_2 r0
    cmp r0 r5
    jne fail
    # The register of the `lr` keeps its value.
    cmp 0x5555 r2
    jne fail
    # The RP2040 has given the byte "A" away. The next byte is "B".
    lr 0x14(fp) r2
    cmp 0x42 r2
    jne fail

    # A write: the scratch register has the new value.
    li 0x3 r7
    li 0x1111 r0
    sr r0 4(fp)
    li 0x2222 r1
    li 0x2 r0
    sr r0 0x30(fp)
    sr r1 4(fp)
    cmp 0x2 r6
    jne fail
    li 0x05020005 r0
    cmp r0 r4
    jne fail
    lr 4(fp) r0
    cmp 0x2222 r0
    jne fail

    # A fetch. The handler returns to the instruction, and it runs once.
    li 0x4 r7
    li 0x0 r2
    li 0x1 r0
    sr r0 0x30(fp)
_lost_4:
    add 0x1 r2
    cmp 0x3 r6
    jne fail
    li 0x05080005 r0
    cmp r0 r4
    jne fail
    li _lost_4 r0
    cmp r0 r3
    jne fail
    cmp r0 r5
    jne fail
    cmp 0x1 r2
    jne fail

    # A `push`: the stack pointer keeps its value, and the word is in
    # memory all the same.
    li 0x5 r7
    li 0x8001fffc r2
    li 0x0 r0
    sr r0 0(r2)
    li 0x7777 r1
    li 0x2 r0
    sr r0 0x30(fp)
    push r1
    cmp 0x4 r6
    jne fail
    li 0x80020000 r0
    cmp r0 sp
    jne fail
    lr 0(r2) r0
    cmp 0x7777 r0
    jne fail

    # A `pop`: the stack pointer and the register keep their values.
    li 0x6 r7
    push r1
    li 0x5555 r1
    li 0x2 r0
    sr r0 0x30(fp)
    pop r1
    cmp 0x5 r6
    jne fail
    cmp r2 sp
    jne fail
    cmp 0x5555 r1
    jne fail
    pop r1
    cmp 0x7777 r1
    jne fail

    # The entries of the page tables. The virtual page 0x19000 is the
    # physical page 0x13000, through the top table at 0x10000 and the second
    # table at 0x11000. The second access after the order is the read of the
    # top entry, and the third is the read of the second entry: a page fault
    # with the status 5 and the number of the table in the bits 21:20.
    li 0x7 r7
    li 0x80010000 r1
    li 0x00011003 r0
    sr r0 0(r1)
    li 0x80011000 r1
    li 0x00013003 r0
    sr r0 0x64(r1)
    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0
    li 0x00019000 r1
    li 0x5555 r2
    li 0x2 r0
    sr r0 0x30(fp)
    lr 0(r1) r2
    cmp 0x6 r6
    jne fail
    li 0x05100003 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail
    li 0x3 r0
    sr r0 0x30(fp)
    lr 0(r1) r2
    cmp 0x7 r6
    jne fail
    li 0x05200003 r0
    cmp r0 r4
    jne fail
    cmp 0x5555 r2
    jne fail

    # The fourth access is the read of the data itself. Both entries were
    # read before it, and the translation that they gave stays.
    li 0x8 r7
    li 0x4 r0
    sr r0 0x30(fp)
    lr 0(r1) r2
    cmp 0x8 r6
    jne fail
    li 0x05000005 r0
    cmp r0 r4
    jne fail
    cmp r1 r3
    jne fail
    # With the translation remembered, the same read takes two accesses, and
    # the third one is the fetch of the instruction after it. Without it the
    # third access would be the read of the second table entry.
    li 0x3 r0
    sr r0 0x30(fp)
    lr 0(r1) r2
_lost_8:
    nop
    cmp 0x9 r6
    jne fail
    li 0x05080005 r0
    cmp r0 r4
    jne fail
    li _lost_8 r0
    cmp r0 r5
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

on_bus_error:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    # A fetch has failed if 0x80000 is set in the cause.
    mov r4 r5
    shr 0x13 r5
    test 0x1 r5
    mov cr1 r5
    jne _on_bus_error_end
    add 0x4 r5
    mov r5 cr1
    sub 0x4 r5
_on_bus_error_end:
    iret

on_page_fault:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    mov cr1 r5
    add 0x4 r5
    mov r5 cr1
    sub 0x4 r5
    iret

# The bus error exception (vector 5) in kernel mode without paging.
#
# The address space of the kernel without paging:
#
# * 0x00000000-0x7fffffff and 0x80000000-0xffffffff are both the lower half
#   of the physical addresses, which the RP2040 serves over the bus.
#
# The physical addresses of the lower half:
#
# * 0x00000000-0x0002ffff: RAM;
# * 0x00800000-0x0083ffff: the boot ROM, which cannot be written;
# * 0x00ff0000-0x00ff002f: device registers, which take whole 4-byte reads
#   and writes only; the register at 0x00ff0008 holds the amount of RAM in
#   bytes;
# * nothing anywhere else.
#
# An access that the RP2040 refuses raises a bus error too, with the status of
# the RP2040 in bits 26-24 of the cause (1: no such address or not allowed;
# 2: a fetch from a device register).
# The cause also has the vector in bits 7-0, 0x20000 for a write, and 0x80000
# for a fetch.
#
# The handler counts the exceptions in `r6` and leaves the cause in `r4` and
# the address in `r3`. After a read or a write it returns to the next
# instruction; after a fetch it continues the test at the address in `r5`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_bus_error r1
    sr r1 20(r0)

    # Addresses above the first 16 MB: every bit of the address reaches the
    # RP2040, which has nothing there. 0x01000000 is not RAM at 0, and
    # 0x81ff0000 is not a device register.
    li 0x1 r7
    li 0x0 r6
    li 0x55 r0
    li 0x01000000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x01000005 r2
    cmp r2 r4
    jne fail
    cmp r1 r3
    jne fail
    cmp 0x55 r0
    jne fail
    li 0x7ffffffc r1
    lr 0(r1) r0
    li 0x81000000 r1
    lrb 0(r1) r0
    li 0x81ff0000 r1
    lr 0(r1) r0
    li 0x80010000 r1
    lr 0(r1) r0
    li 0xc0010000 r1
    lr 0(r1) r0
    li 0xfffffffc r1
    lr 0(r1) r0
    cmp 0x6 r6
    jne fail
    cmp r2 r4
    jne fail
    cmp r1 r3
    jne fail

    # A store to such an address.
    li 0x2 r7
    li 0x0 r6
    li 0x40000000 r1
    sr r0 0(r1)
    cmp 0x1 r6
    jne fail
    li 0x01020005 r0
    cmp r0 r4
    jne fail

    # The same RAM at both of its addresses.
    li 0x3 r7
    li 0x0 r6
    li 0x80010000 r1
    li 0x12345678 r0
    sr r0 0(r1)
    li 0x00010000 r1
    lr 0(r1) r2
    cmp r0 r2
    jne fail
    cmp 0x0 r6
    jne fail

    # The last word of RAM answers; the word after it does not.
    li 0x4 r7
    li 0x0 r6
    li 0x8002fffc r1
    sr r0 0(r1)
    lr 0(r1) r2
    cmp r0 r2
    jne fail
    cmp 0x0 r6
    jne fail
    lr 4(r1) r2
    cmp 0x1 r6
    jne fail
    li 0x01000005 r0
    cmp r0 r4
    jne fail
    li 0x80030000 r0
    cmp r0 r3
    jne fail

    # Physical addresses with nothing behind them.
    li 0x5 r7
    li 0x0 r6
    li 0x00100000 r1
    lrb 0(r1) r2
    li 0x807ffffc r1
    lr 0(r1) r2
    li 0x80840000 r1
    lr 0(r1) r2
    li 0x80fefffc r1
    sr r2 0(r1)
    cmp 0x4 r6
    jne fail
    li 0x01020005 r0
    cmp r0 r4
    jne fail

    # The boot ROM can be read up to its last word, and it cannot be written.
    li 0x6 r7
    li 0x0 r6
    li 0x8083fffc r1
    lr 0(r1) r2
    cmp 0x0 r6
    jne fail
    li 0x80800000 r1
    lr 0(r1) r2
    li -1 r0
    sr r0 0(r1)
    srb r0 0(r1)
    cmp 0x2 r6
    jne fail
    li 0x01020005 r0
    cmp r0 r4
    jne fail
    lr 0(r1) r0
    cmp r2 r0
    jne fail

    # The device registers: the identification register holds the bytes
    # "ALM2", the RAM size register holds the amount of RAM, and the scratch
    # register keeps what is written to it.
    li 0x7 r7
    li 0x0 r6
    li 0x80ff0000 r1
    lr 0(r1) r0
    li 0x324d4c41 r2
    cmp r2 r0
    jne fail
    lr 8(r1) r0
    li 0x30000 r2
    cmp r2 r0
    jne fail
    li 0x12345678 r0
    sr r0 4(r1)
    lr 4(r1) r2
    cmp r0 r2
    jne fail
    cmp 0x0 r6
    jne fail

    # A device register refuses 1-byte and 2-byte accesses, a write to a
    # read-only register, and an address with no register.
    li 0x8 r7
    li 0x0 r6
    lrb 4(r1) r2
    lrhu 4(r1) r2
    srb r0 4(r1)
    srh r0 4(r1)
    sr r0 0(r1)
    sr r0 8(r1)
    lr 0x100(r1) r2
    cmp 0x7 r6
    jne fail
    li 0x01000005 r0
    cmp r0 r4
    jne fail
    # The refused writes have changed nothing.
    lr 4(r1) r2
    li 0x12345678 r0
    cmp r0 r2
    jne fail

    # A failed `push` and a failed `pop` keep the stack pointer.
    li 0x9 r7
    li 0x0 r6
    mov sp r2
    li 0x80030000 sp
    pop r0
    li 0x80800004 sp
    push r0
    mov sp r1
    mov r2 sp
    cmp 0x2 r6
    jne fail
    li 0x80800004 r0
    cmp r0 r1
    jne fail

    # A jump to a physical address with nothing behind it. The exception is
    # raised when the CPU fetches from the address, so the saved address and
    # the fault address are both the target of the jump.
    li 0xa r7
    li 0x0 r6
    li _back_10 r5
    li 0x80400000 r0
    jmp r0
_back_10:
    cmp 0x1 r6
    jne fail
    li 0x01080005 r0
    cmp r0 r4
    jne fail
    li 0x80400000 r0
    cmp r0 r3
    jne fail
    cmp r0 r1
    jne fail

    # A jump to a device register: instructions cannot be fetched from there,
    # and the RP2040 answers with its protection status.
    li 0xb r7
    li 0x0 r6
    li _back_11 r5
    li 0x80ff0004 r0
    jmp r0
_back_11:
    cmp 0x1 r6
    jne fail
    li 0x02080005 r0
    cmp r0 r4
    jne fail

    # A jump to an address with nothing behind it.
    li 0xc r7
    li 0x0 r6
    li _back_12 r5
    li 0x40000000 r0
    jmp r0
_back_12:
    cmp 0x1 r6
    jne fail
    li 0x01080005 r0
    cmp r0 r4
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# The handler keeps its own values in `fp` and does not touch the stack: some
# checks give it a stack pointer that cannot be used.
on_bus_error:
    add 0x1 r6
    mov cr5 r4
    mov cr2 r3
    mov r4 fp
    shr 0x13 fp
    test 0x1 fp
    jne _on_bus_error_fetch
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret
_on_bus_error_fetch:
    mov cr1 r1
    mov r5 cr1
    iret

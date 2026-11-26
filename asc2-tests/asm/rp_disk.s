# The devices of the RP2040: the disk.
#
# The disk is a row of sectors of 4096 bytes, the size of a page. The kernel
# names a sector and a buffer of 4096 bytes in RAM and gives a command; the
# RP2040 copies the whole sector between the disk and the buffer by itself.
# The registers (see `rp_console.s` for the others):
#
#     0x80ff0018  interrupt status: 0x2 = a disk command has ended; writing
#                 0x2 clears the bit
#     0x80ff001c  interrupt enable: the same bit
#     0x80ff0020  disk sector: the number of the sector
#     0x80ff0024  disk buffer: the physical address of the buffer
#     0x80ff0028  disk command: 1 = read the sector into the buffer,
#                               2 = write the buffer to the sector
#     0x80ff002c  disk status: 0x1 = a command is running,
#                              0x2 = the last command has failed
#
# A command takes time. While it runs, the buffer belongs to the RP2040, and
# writes to the disk's registers are ignored. The kernel either asks the
# status register until the command has ended, or it lets the RP2040 request
# an interrupt (line 2, the vector 10) and does something else meanwhile.
#
# The disk of this test has four sectors. It starts with the bytes
# "ALMURUT2"; all other bytes are zero.
#
# The handler of the disk counts its interrupts in `r6` and leaves the
# interrupt status it saw in `r4`. The handler of the timer does nothing.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80030000 sp

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)
    li on_rp r1
    sr r1 40(r0)

    li 0x0 r6
    li 0x80ff0000 r3

    # Nothing is running and nothing has failed.
    li 0x1 r7
    lr 0x2c(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    # Read the sector 0 into the buffer at 0x20000. The command is running
    # right after it was given.
    li 0x2 r7
    li 0x0 r0
    li 0x00020000 r1
    li 0x1 r2
    call start_disk
    lr 0x2c(r3) r0
    cmp 0x1 r0
    jne fail
    call wait_for_disk
    cmp 0x0 r0
    jne fail
    # The bytes "ALMU" and "RUT2", then zeros.
    li 0x80020000 r1
    lr 0(r1) r0
    li 0x554d4c41 r2
    cmp r2 r0
    jne fail
    lr 4(r1) r0
    li 0x32545552 r2
    cmp r2 r0
    jne fail
    lr 8(r1) r0
    cmp 0x0 r0
    jne fail

    # The end of the command shows in the interrupt status, enabled or not,
    # until the kernel clears it.
    li 0x3 r7
    lr 0x18(r3) r0
    cmp 0x2 r0
    jne fail
    li 0x2 r0
    sr r0 0x18(r3)
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    # Fill the buffer at 0x21000 with numbers and write it to the sector 3.
    li 0x4 r7
    li 0x80021000 r1
    li 0x0 r0
_fill_4:
    mov r0 r2
    mul 0x101 r2
    sr r2 0(r1)
    add 0x4 r1
    add 0x1 r0
    cmp 0x400 r0
    jne _fill_4
    li 0x3 r0
    li 0x00021000 r1
    li 0x2 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail

    # Read the sector 3 back into another buffer and compare.
    li 0x5 r7
    li 0x3 r0
    li 0x00022000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail
    li 0x80022000 r1
    li 0x0 r0
_compare_5:
    mov r0 r2
    mul 0x101 r2
    lr 0(r1) r5
    cmp r2 r5
    jne fail
    add 0x4 r1
    add 0x1 r0
    cmp 0x400 r0
    jne _compare_5

    # While a command runs, writes to the disk's registers are ignored: the
    # read of the sector 0 goes on, and the sector 3 is not written.
    li 0x6 r7
    li 0x80020000 r1
    li 0x0 r0
    sr r0 0(r1)
    li 0x0 r0
    li 0x00020000 r1
    li 0x1 r2
    call start_disk
    li 0x3 r0
    li 0x00020000 r1
    li 0x2 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail
    lr 0x20(r3) r0
    cmp 0x0 r0
    jne fail
    li 0x80020000 r1
    lr 0(r1) r0
    li 0x554d4c41 r2
    cmp r2 r0
    jne fail
    li 0x3 r0
    li 0x00022000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    li 0x80022000 r1
    lr 4(r1) r0
    cmp 0x101 r0
    jne fail

    # A sector past the end of the disk: the command fails.
    li 0x7 r7
    li 0x4 r0
    li 0x00020000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail

    # A good command clears the failure.
    li 0x8 r7
    li 0x0 r0
    li 0x00020000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail

    # A buffer that does not fit into RAM, a buffer in the boot ROM, a buffer
    # whose address is not a multiple of 4, and a command that does not exist.
    li 0x9 r7
    li 0x0 r0
    li 0x0002f800 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail
    li 0x0 r0
    li 0x00800000 r1
    li 0x2 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail
    li 0x0 r0
    li 0x00020002 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail
    li 0x0 r0
    li 0x00020000 r1
    li 0x3 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail

    # The kernel sleeps in `hlt` until the disk has ended the command. The
    # handler clears the disk's bit in the interrupt status, or the RP2040
    # would keep requesting the interrupt.
    #
    # The disk may end the command between the `sti` and the `hlt`. Its
    # handler has run by then, and the `hlt` would wait for an interrupt
    # that never comes. So the line of the timer is open too. The timer has
    # run since the reset and wakes the CPU again and again, and the loop
    # sleeps until the handler of the disk has counted.
    li 0xa r7
    li 0x2 r0
    sr r0 0x18(r3)
    sr r0 0x1c(r3)
    li 0xfa r0
    call set_irq_mask
    li 0x0 r6
    li 0x0 r0
    li 0x00020000 r1
    li 0x1 r2
    call start_disk
    li 0x2710 r1
_sleep_a:
    sti
    hlt
    cli
    cmp 0x0 r6
    jne _woken_a
    sub 0x1 r1
    jne _sleep_a
    jmp fail
_woken_a:
    cmp 0x1 r6
    jne fail
    cmp 0x2 r4
    jne fail
    lr 0x2c(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail

    li 0x600d r0
    hlt

fail:
    cli
    mov r7 r0
    hlt

# Starts the command in `r2` on the sector in `r0` with the buffer at the
# physical address in `r1`.
start_disk:
    sr r0 0x20(r3)
    sr r1 0x24(r3)
    sr r2 0x28(r3)
    ret

# Waits until no disk command is running and returns the disk status in `r0`.
wait_for_disk:
    li 0x2710 r1
_wait_for_disk_next:
    lr 0x2c(r3) r0
    test 0x1 r0
    je _wait_for_disk_end
    sub 0x1 r1
    jne _wait_for_disk_next
    jmp fail
_wait_for_disk_end:
    ret

# Sets the mask of the interrupt controller to the low byte of `r0`.
set_irq_mask:
    li 0x20 r1
    li 0x0 r2
    outb r2 r1
    li 0x21 r1
    outb r0 r1
    ret

on_timer:
    iret

on_rp:
    push r0
    add 0x1 r6
    lr 0x18(r3) r4
    li 0x2 r0
    sr r0 0x18(r3)
    pop r0
    iret

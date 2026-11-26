# The disk of the RP2040 on the board.
#
# `../asm/rp_disk.s` explains the disk and checks its rules on the emulator,
# with a disk whose first bytes the test knows. The disk of the board is a
# part of the flash of the Pico. It keeps what the last program has written,
# and a write takes the time of a flash: about a tenth of a second. So this
# program checks only what it writes itself, and it prints a line for each
# step that went well. `make board-suite` compares the lines with
# `disk.expect`.
#
# The numbers that the program writes start at the value of the clock
# register. Two runs write different numbers, so a sector that an earlier
# run has left on the disk cannot pass for the sector of this run.
#
# The registers of the disk (see `../asm/rp_disk.s`):
#
#     0x80ff000c  clock: microseconds since the reset
#     0x80ff0018  interrupt status: 0x2 = a disk command has ended
#     0x80ff001c  interrupt enable: the same bit
#     0x80ff0020  disk sector
#     0x80ff0024  disk buffer: the physical address of the buffer
#     0x80ff0028  disk command: 1 = read, 2 = write
#     0x80ff002c  disk status: 0x1 = a command is running,
#                              0x2 = the last command has failed
#
# `r6` holds the first number, and `r7` counts the interrupts of the disk.

.section .data
line_written:
    .asciz "disk: a sector is written\n"
line_read:
    .asciz "disk: the sector reads back\n"
line_interrupt:
    .asciz "disk: the end of a command interrupts\n"
line_refused:
    .asciz "disk: a sector past the end is refused\n"
line_done:
    .asciz "disk: done\n"
line_failed:
    .asciz "disk: FAILED\n"

.section .text
main:
    li 0x80030000 sp
    li 0x80ff0000 r3

    li 0x80000000 r0
    li on_timer r1
    sr r1 32(r0)
    li on_rp r1
    sr r1 40(r0)

    lr 0xc(r3) r6

    # Fill the buffer at 0x21000 with numbers and write it to the sector 3.
    li 0x80021000 r1
    li 0x0 r0
_fill:
    mov r0 r2
    mul 0x101 r2
    add r6 r2
    sr r2 0(r1)
    add 0x4 r1
    add 0x1 r0
    cmp 0x400 r0
    jne _fill
    li 0x3 r0
    li 0x00021000 r1
    li 0x2 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail
    li line_written r2
    call print

    # Read the sector 3 back into another buffer and compare.
    li 0x3 r0
    li 0x00022000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x0 r0
    jne fail
    call compare
    li line_read r2
    call print

    # Read it once more, and sleep until the disk has ended the command.
    # The line of the timer is open too: the disk may end the command
    # between the `sti` and the `hlt`, and the timer then wakes the CPU.
    li 0x80022000 r1
    li 0x0 r0
    sr r0 0(r1)
    li 0x2 r0
    sr r0 0x18(r3)
    sr r0 0x1c(r3)
    li 0xfa r0
    call set_irq_mask
    li 0x0 r7
    li 0x3 r0
    li 0x00022000 r1
    li 0x1 r2
    call start_disk
    li 0x100000 r1
_sleep:
    sti
    hlt
    cli
    cmp 0x0 r7
    jne _woken
    sub 0x1 r1
    jne _sleep
    jmp fail
_woken:
    li 0x0 r0
    sr r0 0x1c(r3)
    lr 0x2c(r3) r0
    cmp 0x0 r0
    jne fail
    call compare
    li line_interrupt r2
    call print

    # A sector that no disk has: the command fails.
    li 0x10000 r0
    li 0x00022000 r1
    li 0x1 r2
    call start_disk
    call wait_for_disk
    cmp 0x2 r0
    jne fail
    li line_refused r2
    call print

    li line_done r2
    call print
    cli
    li 0x600d r0
    hlt

fail:
    cli
    li line_failed r2
    call print
    hlt

# Compares the buffer at 0x22000 with the numbers that the program wrote.
compare:
    li 0x80022000 r1
    li 0x0 r0
_compare_next:
    mov r0 r2
    mul 0x101 r2
    add r6 r2
    lr 0(r1) r5
    cmp r2 r5
    jne fail
    add 0x4 r1
    add 0x1 r0
    cmp 0x400 r0
    jne _compare_next
    ret

# Starts the command in `r2` on the sector in `r0` with the buffer at the
# physical address in `r1`.
start_disk:
    sr r0 0x20(r3)
    sr r1 0x24(r3)
    sr r2 0x28(r3)
    ret

# Waits until no disk command is running and returns the disk status in `r0`.
wait_for_disk:
    li 0x100000 r1
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

# Prints the string at the address in `r2`. A zero byte ends the string.
print:
    push lr
_print_next:
    lrbu 0(r2) r0
    cmp 0x0 r0
    je _print_end
    call print_byte
    add 0x1 r2
    jmp _print_next
_print_end:
    pop lr
    ret

# Prints the byte in `r0`: waits until the console is ready, then writes the
# byte to the data register.
print_byte:
    lr 0x10(r3) r1
    test 0x2 r1
    je print_byte
    sr r0 0x14(r3)
    ret

on_timer:
    iret

on_rp:
    push r0
    add 0x1 r7
    li 0x2 r0
    sr r0 0x18(r3)
    pop r0
    iret

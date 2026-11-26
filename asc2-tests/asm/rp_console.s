# The devices of the RP2040: the identification, scratch, and clock registers,
# and the console.
#
# The RP2040 next to the chip serves memory, and it is also a few devices.
# Their registers are at the physical addresses from 0xff0000, which the
# kernel reaches at 0x80ff0000. Every register takes whole 4-byte reads and
# writes.
#
#     0x80ff0000  identification: the bytes "ALM2"; read-only
#     0x80ff0004  scratch: keeps what is written to it
#     0x80ff000c  clock: microseconds since the reset; read-only
#     0x80ff0010  console status: 0x1 = an input byte is waiting,
#                                 0x2 = the console can take an output byte
#     0x80ff0014  console data: a read takes the next input byte, or gives
#                 0x100 if none is waiting; a write prints a byte
#     0x80ff0018  interrupt status: 0x1 = a console input byte is waiting
#     0x80ff001c  interrupt enable: the same bits; a device with its bit set
#                 here may request an interrupt
#
# All devices of the RP2040 share line 2 of the interrupt controller (the
# vector 10). Unlike the timer, the RP2040 holds the line for as long as it
# has a reason to: the handler runs again and again until it has taken every
# waiting byte or has cleared the enable bit.
#
# The console of this test gets the bytes of the file `rp_console.in`
# ("ok", a new line, "xyz"), and what the test prints is compared with the
# file `rp_console.expect`.
#
# The handler uses these registers:
#
# * `r6`: the number of interrupts so far;
# * `r5`: the bytes it has taken, the last one in the low byte;
# * `r4`: the interrupt status it saw last.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
greeting:
    .byte 0x48
    .byte 0x69
    .byte 0x0a
    .byte 0x00

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_rp r1
    sr r1 40(r0)

    li 0x0 r6
    li 0x0 r5
    li 0x80ff0000 r3

    # The identification register.
    li 0x1 r7
    lr 0x0(r3) r0
    li 0x324d4c41 r1
    cmp r1 r0
    jne fail

    # The scratch register.
    li 0x2 r7
    li 0x12345678 r0
    sr r0 0x4(r3)
    lr 0x4(r3) r1
    cmp r0 r1
    jne fail
    li 0x0 r0
    sr r0 0x4(r3)
    lr 0x4(r3) r1
    cmp 0x0 r1
    jne fail

    # The clock moves forward.
    li 0x3 r7
    lr 0xc(r3) r0
    li 0x64 r2
_loop_3:
    sub 0x1 r2
    jne _loop_3
    lr 0xc(r3) r1
    cmp r0 r1
    jbe fail

    # Print "Hi" and a new line: wait for the console to be ready, then write
    # the byte.
    li 0x4 r7
    li greeting r2
_loop_4:
    lrbu 0(r2) r0
    cmp 0x0 r0
    je _done_4
    call print_byte
    add 0x1 r2
    jmp _loop_4
_done_4:

    # Read the first line of the input by asking the status register again
    # and again: "ok" and a new line.
    li 0x5 r7
    call read_byte
    cmp 0x6f r0
    jne fail
    call read_byte
    cmp 0x6b r0
    jne fail
    call read_byte
    cmp 0x0a r0
    jne fail

    # More input is waiting. The interrupt status shows it, but nothing is
    # enabled, so no interrupt is requested.
    li 0x6 r7
    li 0xfb r0
    call set_irq_mask
    lr 0x18(r3) r0
    cmp 0x1 r0
    jne fail
    lr 0x1c(r3) r0
    cmp 0x0 r0
    jne fail
    sti
    nop
    nop
    cli
    cmp 0x0 r6
    jne fail

    # Enable the console's interrupt. The handler takes one byte each time it
    # runs, and after the second byte it clears the enable bit: it runs twice
    # though three bytes were waiting.
    li 0x7 r7
    li 0x1 r0
    sr r0 0x1c(r3)
    sti
    nop
    nop
    cli
    cmp 0x2 r6
    jne fail
    li 0x7879 r0
    cmp r0 r5
    jne fail
    cmp 0x1 r4
    jne fail

    # The third byte is still there.
    li 0x8 r7
    lr 0x18(r3) r0
    cmp 0x1 r0
    jne fail
    call read_byte
    cmp 0x7a r0
    jne fail

    # Nothing is left: the status bits are clear, and a read of the data
    # register gives 0x100.
    li 0x9 r7
    lr 0x10(r3) r0
    test 0x1 r0
    jne fail
    lr 0x18(r3) r0
    cmp 0x0 r0
    jne fail
    lr 0x14(r3) r0
    cmp 0x100 r0
    jne fail

    # With no byte waiting the enabled interrupt stays quiet.
    li 0xa r7
    li 0x1 r0
    sr r0 0x1c(r3)
    sti
    nop
    nop
    cli
    cmp 0x2 r6
    jne fail

    # Print what the handler and the last read have taken: "xyz" and a new
    # line.
    li 0xb r7
    li 0x78 r0
    call print_byte
    li 0x79 r0
    call print_byte
    li 0x7a r0
    call print_byte
    li 0x0a r0
    call print_byte

    li 0x600d r0
    hlt

fail:
    cli
    mov r7 r0
    hlt

# Prints the byte in `r0`.
print_byte:
    lr 0x10(r3) r1
    test 0x2 r1
    je print_byte
    sr r0 0x14(r3)
    ret

# Waits for an input byte and returns it in `r0`.
read_byte:
    li 0x2710 r1
_read_byte_next:
    lr 0x10(r3) r0
    test 0x1 r0
    jne _read_byte_end
    sub 0x1 r1
    jne _read_byte_next
    jmp fail
_read_byte_end:
    lr 0x14(r3) r0
    ret

# Sets the mask of the interrupt controller to the low byte of `r0`.
set_irq_mask:
    li 0x20 r1
    li 0x0 r2
    outb r2 r1
    li 0x21 r1
    outb r0 r1
    ret

on_rp:
    push r0
    add 0x1 r6
    lr 0x18(r3) r4
    lr 0x14(r3) r0
    shl 0x8 r5
    or r0 r5
    cmp 0x2 r6
    jne _on_rp_end
    li 0x0 r0
    sr r0 0x1c(r3)
_on_rp_end:
    pop r0
    iret

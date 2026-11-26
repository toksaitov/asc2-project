# The first program: prints a greeting on the console, and then prints back
# every byte that the console receives.
#
# The console is a device of the RP2040. Its registers are in memory, at the
# addresses from 0x80ff0000:
#
#     0x80ff0010  console status: 0x1 = an input byte is waiting,
#                                 0x2 = the console can take an output byte
#     0x80ff0014  console data: a read takes the next input byte, a write
#                 prints a byte
#     0x80ff001c  interrupt enable: 0x1 = request an interrupt while an input
#                 byte is waiting
#
# The program does not ask the console for input again and again. It sleeps
# in `hlt`, and the RP2040 wakes it with an interrupt on line 2 of the
# interrupt controller (the vector 10) when a byte has arrived.

.section .data
greeting:
    .asciz "Hello from Almurut II!\n"

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3

    # The address of the handler of the vector 10 goes to the vector table,
    # which starts at the physical address 0.
    li 0x80000000 r0
    li on_console r1
    sr r1 40(r0)

    li greeting r2
    call print

    # Let the console request interrupts, open line 2 of the interrupt
    # controller (a 0 bit in the mask), and turn interrupts on.
    li 0x1 r0
    sr r0 0x1c(r3)
    li 0x20 r1
    li 0x0 r0
    outb r0 r1
    li 0x21 r1
    li 0xfb r0
    outb r0 r1
    sti

_sleep:
    hlt
    jmp _sleep

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

# Takes one byte from the console and prints it. The RP2040 keeps requesting
# the interrupt while more bytes are waiting, so the handler runs once for
# each byte.
on_console:
    push r0
    push r1
    push lr
    lr 0x14(r3) r0
    call print_byte
    pop lr
    pop r1
    pop r0
    iret

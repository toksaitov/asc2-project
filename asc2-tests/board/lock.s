# The lock of the debug port on the board.
#
# A kernel that does not want its registers shown writes the key 0x4c4f434b
# to `cr8`. From then on the debug port of the chip shows that it is locked
# and nothing else, until a reset (`../asm/cpu_dbg_lock.s` has the rules).
#
# The program prints a line, locks the port, prints what `cr8` reads, and
# sleeps. `make board-lock` reads the debug port with the bridge before the
# program starts and after its last line: open at first, locked at the end.
# The program that the suite loads next gets a reset, and the suite reads
# its registers through the port, which is open again.
#
# The program ends with 0x600d in `r0`.

.section .data
digits:
    .ascii "01"
line_open:
    .asciz "lock: the port is open\n"
line_cr8:
    .asciz "lock: cr8 reads "
line_done:
    .asciz "lock: done\n"

.section .text
main:
    li 0x80020000 sp
    li 0x80ff0000 r3

    li line_open r2
    call print

    li 0x4c4f434b r1
    mov r1 cr8

    li line_cr8 r2
    call print
    mov cr8 r4
    and 0x1 r4
    li digits r2
    add r4 r2
    lrbu 0(r2) r0
    call print_byte
    li 0xa r0
    call print_byte

    li line_done r2
    call print
    li 0x600d r0
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

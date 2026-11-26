# The physical addresses above 16 MB on the board.
#
# A physical address has 32 bits, and the chip sends all four bytes of it to
# the RP2040. The RP2040 has a little RAM at 0xfffff000, the start of the
# last page of the 4 GB, so that a program can tell an address from the same
# address without its upper bits. `../asm/mmu_fault.s` and `../asm/mmu_tlb.s`
# check the rules on the emulator and on the chip in simulation. This program
# runs on the chip in the FPGA, and `make board-suite` compares the lines
# that it prints with `high.expect`.
#
# The page tables give four pages, from 0x00400000 on:
#
#     0x00400000  the high RAM at 0xfffff000
#     0x00401000  the RAM at 0x20000
#     0x00402000  0x5a5a5000, where no memory is
#     0x00403000  0xa5a5a000, where no memory is
#
# The program writes one word to the first page and another to the second,
# and prints both, and the word that the kernel window shows at 0x20000. A
# chip that lost the upper bits of the page number would write both words to
# one place. It then reads from the last two pages. The RP2040 refuses each
# read, and the program prints the cause of each exception; every bit of the
# page number is 1 in one of the two pages and 0 in the other.
#
# `r7` holds the address of the first page, `r6` the word to print, and `r4`
# the cause of the last exception. The program ends with 0x600d in `r0`.

.section .data
digits:
    .ascii "0123456789abcdef"
line_ready:
    .asciz "high: ready\n"
line_done:
    .asciz "high: done\n"

.section .text
main:
    li 0x80030000 sp
    li 0x80ff0000 r3

    # A page fault and a bus error go to `on_fault`
    li 0x80000000 r0
    li on_fault r1
    sr r1 12(r0)
    sr r1 20(r0)

    # The top table at 0x10000 and the second table at 0x11000
    li 0x80010000 r0
    call clear_page
    li 0x80011000 r0
    call clear_page
    li 0x80010000 r0
    li 0x00011007 r1
    sr r1 0x4(r0)
    li 0x80011000 r0
    li 0xfffff007 r1
    sr r1 0x0(r0)
    li 0x00020007 r1
    sr r1 0x4(r0)
    li 0x5a5a5007 r1
    sr r1 0x8(r0)
    li 0xa5a5a007 r1
    sr r1 0xc(r0)

    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0

    li line_ready r2
    call print

    # One word to the high RAM, another to the RAM at 0x20000
    li 0x00400000 r7
    li 0x48494748 r0
    sr r0 0(r7)
    li 0x00401000 r1
    li 0x4c4f574c r0
    sr r0 0(r1)

    lr 0(r7) r6
    call print_word
    li 0x00401000 r1
    lr 0(r1) r6
    call print_word
    li 0x80020000 r1
    lr 0(r1) r6
    call print_word

    # The two pages where no memory is
    li 0x0 r4
    li 0x00402000 r1
    lr 0x5a4(r1) r0
    mov r4 r6
    call print_word
    li 0x0 r4
    li 0x00403000 r1
    lr 0xa58(r1) r0
    mov r4 r6
    call print_word

    li line_done r2
    call print
    li 0x600d r0
    hlt

# Keeps the cause of the exception in `r4` and returns to the instruction
# after the one that was refused.
on_fault:
    mov cr5 r4
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret

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

# Prints the word in `r6` as eight hex digits and ends the line. `r5` holds
# the digits that are still to print, the next one in its upper four bits,
# and `r4` counts them.
print_word:
    push lr
    mov r6 r5
    li 0x8 r4
_print_word_next:
    mov r5 r0
    shr 0x1c r0
    call print_digit
    shl 0x4 r5
    sub 0x1 r4
    cmp 0x0 r4
    jne _print_word_next
    li 0xa r0
    call print_byte
    pop lr
    ret

# Prints the hex digit of the number in `r0`, which is from 0 to 15.
print_digit:
    push lr
    li digits r1
    add r0 r1
    lrbu 0(r1) r0
    call print_byte
    pop lr
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

# Paging: the translations that the CPU remembers.
#
# Reading two page table entries for every access to memory would make the
# CPU three times slower. The CPU remembers the last 8 translations (page
# number, physical page number, W, U) and reads the tables only for a page
# that is not among them. Textbooks call this memory of translations a
# translation lookaside buffer.
#
# The rules that a kernel has to know:
#
# * A remembered translation is used even after its page table entry has
#   changed. Every write to `cr3` makes the CPU forget all translations, and
#   so does a write to `cr0` that changes the paging bits.
# * When all 8 places are taken, a new translation replaces the oldest one.
# * The kernel can hand the CPU a translation directly: it writes the address
#   to `cr2` and a page table entry to `cr7`. A translation of the same page
#   is replaced. An entry without P is not a translation and changes nothing.
#   `cr7` reads as zero.
#
# The test uses nine pages at 0x00420000-0x00428fff. `map_nine` points them at
# nine physical pages that begin with the words 0x100-0x108, and
# `map_nine_to_x` points all of them at one physical page that begins with
# 0x999. What a read returns tells if the CPU has read the table again.
#
# The handler counts the page faults in `r6`, leaves the cause in `r4`, and
# returns to the next instruction.
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

    # Two top tables, at 0x10000 and 0x12000, each with one second table.
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
    sr r1 4(r0)
    li 0x80012000 r0
    li 0x00013007 r1
    sr r1 4(r0)

    # 0x00410000 is the physical page 0x2a000 in the first set of tables and
    # 0x2b000 in the second set.
    li 0x80011000 r0
    li 0x0002a003 r1
    sr r1 0x40(r0)
    li 0x80013000 r0
    li 0x0002b003 r1
    sr r1 0x40(r0)

    # Words to recognize the physical pages by.
    li 0x80020000 r0
    li 0x100 r1
_mark_next:
    sr r1 0(r0)
    add 0x1000 r0
    add 0x1 r1
    cmp 0x109 r1
    jne _mark_next
    li 0x80029000 r0
    li 0x999 r1
    sr r1 0(r0)
    li 0x8002a000 r0
    li 0xaaa r1
    sr r1 0(r0)
    li 0x8002b000 r0
    li 0xbbb r1
    sr r1 0(r0)

    li 0x00010000 r0
    mov r0 cr3
    li 0x1 r0
    mov r0 cr0
    li 0x0 r6

    # Two sets of tables give two meanings to one address. The write to `cr3`
    # makes the CPU forget the translation of the first set.
    li 0x1 r7
    li 0x00410000 r1
    lr 0(r1) r0
    cmp 0xaaa r0
    jne fail
    li 0x00012000 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0xbbb r0
    jne fail
    li 0x00010000 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0xaaa r0
    jne fail

    # A changed page table entry has no effect while the CPU remembers the old
    # translation.
    li 0x2 r7
    li 0x80011000 r2
    li 0x0002b003 r0
    sr r0 0x40(r2)
    lr 0(r1) r0
    cmp 0xaaa r0
    jne fail
    # A write to `cr3`, even of the value it has, makes the CPU read the
    # entry again.
    mov cr3 r0
    mov r0 cr3
    lr 0(r1) r0
    cmp 0xbbb r0
    jne fail
    li 0x0002a003 r0
    sr r0 0x40(r2)
    mov cr3 r0
    mov r0 cr3

    # The CPU remembers 8 translations. After the reads of the pages 0-7 no
    # read looks at the changed table.
    li 0x3 r7
    call map_nine
    call flush
    li 0x0 r2
    li 0x7 r3
    call expect_old
    call map_nine_to_x
    li 0x0 r2
    li 0x7 r3
    call expect_old

    # The ninth page takes the place of the oldest translation, the one of the
    # page 0. The pages 1-7 are still remembered.
    li 0x4 r7
    li 0x8 r0
    call read_page
    cmp 0x999 r0
    jne fail
    li 0x1 r2
    li 0x7 r3
    call expect_old

    # The page 0 has to be looked up again, and its translation takes the
    # place of the next oldest one, the one of the page 1.
    li 0x5 r7
    li 0x0 r0
    call read_page
    cmp 0x999 r0
    jne fail
    li 0x2 r2
    li 0x7 r3
    call expect_old
    li 0x1 r0
    call read_page
    cmp 0x999 r0
    jne fail
    cmp 0x0 r6
    jne fail

    # The kernel hands the CPU a translation of a page that is not in the
    # tables. Only the page number of the address in `cr2` matters.
    li 0x6 r7
    li 0x0 r6
    li 0x00430000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00200003 r0
    cmp r0 r4
    jne fail
    li 0x0 r6
    li 0x00430abc r0
    mov r0 cr2
    li 0x0002a003 r0
    mov r0 cr7
    lr 0(r1) r0
    cmp 0xaaa r0
    jne fail
    cmp 0x0 r6
    jne fail
    # `cr7` reads as zero.
    mov cr7 r0
    cmp 0x0 r0
    jne fail

    # A second translation of the same page replaces the first one.
    li 0x7 r7
    li 0x0002b001 r0
    mov r0 cr7
    lr 0(r1) r0
    cmp 0xbbb r0
    jne fail
    # The translation has no W now: present (0x10000), a write (0x20000), a
    # remembered translation (no table).
    sr r0 0(r1)
    cmp 0x1 r6
    jne fail
    li 0x00030003 r0
    cmp r0 r4
    jne fail

    # A write to `cr0` that keeps the paging bits changes nothing: bit 2 has
    # no effect on translations.
    li 0x8 r7
    li 0x0 r6
    li 0x5 r0
    mov r0 cr0
    lr 0(r1) r0
    cmp 0xbbb r0
    jne fail
    li 0x1 r0
    mov r0 cr0
    lr 0(r1) r0
    cmp 0xbbb r0
    jne fail
    cmp 0x0 r6
    jne fail

    # Paging off and on again: the CPU has forgotten the translation.
    li 0x9 r7
    li 0x0 r0
    mov r0 cr0
    li 0x1 r0
    mov r0 cr0
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00200003 r0
    cmp r0 r4
    jne fail

    # A translation of an address from 0x80000000 up is not taken: these
    # addresses are never translated. All of the pages 0-7 are still
    # remembered after the write to `cr7`.
    li 0xa r7
    li 0x0 r6
    call map_nine
    call flush
    li 0x0 r2
    li 0x7 r3
    call expect_old
    call map_nine_to_x
    li 0x80430000 r0
    mov r0 cr2
    li 0x0002a003 r0
    mov r0 cr7
    li 0x0 r2
    li 0x7 r3
    call expect_old
    cmp 0x0 r6
    jne fail

    # An entry without P is not a translation: it takes the place of nothing,
    # whether its page is remembered or not. Only a write to `cr3` or a change
    # of the paging bits makes the CPU forget a translation.
    li 0xb r7
    li 0x00430000 r0
    mov r0 cr2
    li 0x0002a002 r0
    mov r0 cr7
    li 0x00423000 r0
    mov r0 cr2
    li 0x0002a002 r0
    mov r0 cr7
    li 0x0 r2
    li 0x7 r3
    call expect_old
    cmp 0x0 r6
    jne fail

    # The page that got the entry without P is not translated by it.
    li 0xc r7
    li 0x00430000 r1
    lr 0(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x00200003 r0
    cmp r0 r4
    jne fail
    li 0x0 r6

    # Bits 31-12 of an entry written to `cr7` are the physical page, all 20
    # of them: 0xfffff000 is the high page, which the RP2040 serves at the
    # end of the physical addresses, and not the page 0x2a000.
    li 0xd r7
    li 0x00430000 r1
    mov r1 cr2
    li 0xfffff003 r0
    mov r0 cr7
    li 0x7654 r0
    sr r0 0(r1)
    lr 0(r1) r0
    cmp 0x7654 r0
    jne fail
    li 0x0002a003 r0
    mov r0 cr7
    lr 0(r1) r0
    cmp 0xaaa r0
    jne fail
    cmp 0x0 r6
    jne fail

    # Two pages where no memory is, 0x5a5a5000 and 0xa5a5a000: every bit of
    # the page number is 1 in one of them and 0 in the other. The RP2040
    # answers each read with the status 1, and the list of the requests on
    # the bus shows that every bit has arrived in its place.
    li 0xe r7
    li 0x5a5a5003 r0
    mov r0 cr7
    lr 0x5a4(r1) r0
    cmp 0x1 r6
    jne fail
    li 0x01000005 r0
    cmp r0 r4
    jne fail
    mov r1 cr2
    li 0xa5a5a003 r0
    mov r0 cr7
    lr 0xa58(r1) r0
    cmp 0x2 r6
    jne fail
    li 0x01000005 r0
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

# Makes the CPU forget the translations it remembers.
flush:
    mov cr3 r0
    mov r0 cr3
    ret

# Points the nine pages at the physical pages 0x20000-0x28000. The entry of
# the page 0x00420000 is the entry 0x20 of the second table.
map_nine:
    li 0x80011080 r0
    li 0x00020003 r1
    li 0x9 r2
_map_nine_next:
    sr r1 0(r0)
    add 0x4 r0
    add 0x1000 r1
    sub 0x1 r2
    jne _map_nine_next
    ret

# Points all nine pages at the physical page 0x29000.
map_nine_to_x:
    li 0x80011080 r0
    li 0x00029003 r1
    li 0x9 r2
_map_nine_to_x_next:
    sr r1 0(r0)
    add 0x4 r0
    sub 0x1 r2
    jne _map_nine_to_x_next
    ret

# Returns the first word of the page whose number (0-8) is in `r0`.
read_page:
    shl 0xc r0
    li 0x00420000 r1
    add r1 r0
    lr 0(r0) r0
    ret

# Fails the test unless the pages from the number in `r2` to the number in
# `r3` still read as the physical pages of `map_nine`.
expect_old:
    push lr
_expect_old_next:
    mov r2 r0
    call read_page
    mov r2 r1
    add 0x100 r1
    cmp r1 r0
    jne fail
    add 0x1 r2
    cmp r3 r2
    jle _expect_old_next
    pop lr
    ret

on_fault:
    add 0x1 r6
    mov cr5 r4
    mov cr1 fp
    add 0x4 fp
    mov fp cr1
    iret

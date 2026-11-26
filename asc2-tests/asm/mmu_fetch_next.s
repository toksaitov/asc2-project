# Sequential fetch and paging (see `cpu_fetch_next.s` for the rule).
#
# The RP2040 knows physical addresses only. When it serves "the next word",
# it reads the word that follows the last fetch in physical memory. That is
# the right word only while the CPU stays on one page under one translation.
# So the CPU sends the address again after the last word of a page, after a
# write to a control register (`cr0`, `cr3`, and `cr7` change translations),
# and after a page table lookup that has loaded a translation.
#
# In each of the first four checks the instruction that follows in physical
# memory is a wrong one. A CPU that asked for the next word there would run
# it, and the check would fail.
#
# The pages of the test:
#
#     0x10000  the top page table; its first entry names the second table
#     0x11000  the second page table
#     0x13000  the page that backs the virtual page 0x19000 at first
#     0x15000  the page that backs it later
#     0x17000  the page that backs the virtual page 0x1a000
#     0x19000  the page that the address 0x19000 names with paging off
#
# The code that runs in these pages is copied there four instructions at a
# time. The requests that the CPU sends on the bus must equal
# `mmu_fetch_next.bus`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80010000 r1
    li 0x00011003 r0
    sr r0 0(r1)
    li 0x80011000 r3
    li 0x00013003 r0
    sr r0 0x64(r3)
    li 0x00017003 r0
    sr r0 0x68(r3)
    li 0x00010000 r0
    mov r0 cr3

    # Paging on, and F (0x4) clear.
    li 0x1 r0
    mov r0 cr0

    # Three additions over the end of the page 0x19000. The pages 0x19000
    # and 0x1a000 follow each other in virtual memory, and their physical
    # pages do not: after the page 0x13000 comes 0x14000, where a `ret`
    # waits.
    li 0x1 r7
    li across_head r1
    li 0x80013ff0 r2
    call copy
    li across_tail r1
    li 0x80017000 r2
    call copy
    li only_ret r1
    li 0x80014000 r2
    call copy
    li 0x0 r4
    li 0x00019ff8 r0
    call r0
    cmp 0x3 r4
    jne fail

    # A write to `cr3`. The CPU still remembers the page 0x13000 for the
    # virtual page 0x19000, and the page table now names 0x15000. The code
    # writes the same address to `cr3` again: the CPU forgets what it
    # remembers, and the next instruction comes from the page 0x15000.
    li 0x2 r7
    li reroot_old r1
    li 0x80013100 r2
    call copy
    li reroot_new r1
    li 0x80015100 r2
    call copy
    li 0x00015003 r0
    sr r0 0x64(r3)
    li 0x0 r4
    li 0x00010000 r0
    li 0x00019100 r2
    call r2
    cmp 0x2 r4
    jne fail

    # A write to `cr7`. The CPU remembers the page 0x15000 now. The code
    # loads the translation to the page 0x13000 by hand, and the next
    # instruction comes from there.
    li 0x3 r7
    li reload_old r1
    li 0x80015200 r2
    call copy
    li reload_new r1
    li 0x80013200 r2
    call copy
    li 0x00019000 r0
    mov r0 cr2
    li 0x0 r4
    li 0x00013003 r1
    li 0x00019200 r2
    call r2
    cmp 0x2 r4
    jne fail

    # A write to `cr0`. The code turns paging off, and the address of the
    # next instruction is a physical address from then on: 0x19304 and not
    # 0x13304.
    li 0x4 r7
    li unpage_old r1
    li 0x80013300 r2
    call copy
    li unpage_new r1
    li 0x80019300 r2
    call copy
    li 0x0 r4
    li 0x0 r0
    li 0x00019300 r2
    call r2
    cmp 0x2 r4
    jne fail

    # A page table lookup for the data of an instruction. Paging is on again,
    # and the CPU remembers nothing. The first read makes the CPU read two
    # page table entries, and the fetch after it carries its address. The
    # second read needs no lookup, and the run goes on.
    li 0x5 r7
    li 0x80015000 r2
    li 0x5a5a r0
    sr r0 0(r2)
    li 0x1 r0
    mov r0 cr0
    li 0x00019000 r1
    lr 0(r1) r0
    lr 0(r1) r2
    cmp 0x5a5a r0
    jne fail
    cmp 0x5a5a r2
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# Copies four words from the address in `r1` to the address in `r2`.
copy:
    lr 0(r1) r0
    sr r0 0(r2)
    lr 4(r1) r0
    sr r0 4(r2)
    lr 8(r1) r0
    sr r0 8(r2)
    lr 12(r1) r0
    sr r0 12(r2)
    ret

# The code that the test copies, four instructions each. The test calls the
# third instruction of `across_head`, and `across_tail` follows it on the
# next virtual page.
across_head:
    nop
    nop
    add 0x1 r4
    add 0x1 r4

across_tail:
    add 0x1 r4
    ret
    nop
    nop

only_ret:
    ret
    ret
    ret
    ret

# Each pair differs in the second instruction: the copy named `old` is in
# the page that the first instruction is fetched from, and the copy named
# `new` is in the page that the second instruction must come from.
reroot_old:
    mov r0 cr3
    li 0x1 r4
    ret
    nop

reroot_new:
    mov r0 cr3
    li 0x2 r4
    ret
    nop

reload_old:
    mov r1 cr7
    li 0x1 r4
    ret
    nop

reload_new:
    mov r1 cr7
    li 0x2 r4
    ret
    nop

unpage_old:
    mov r0 cr0
    li 0x1 r4
    ret
    nop

unpage_new:
    mov r0 cr0
    li 0x2 r4
    ret
    nop

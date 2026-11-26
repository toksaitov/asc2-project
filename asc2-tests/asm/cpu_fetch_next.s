# Sequential fetch: which instruction fetches carry an address.
#
# A fetch takes nine bytes on the bus: one that says what the CPU wants,
# three of the address, one of the answer, and four of the instruction. The
# RP2040 remembers where the last fetch was, so the CPU can ask for "the next
# word" and leave the address out. It does so while it runs through
# consecutive instructions, and it sends the address again after
#
# * a reset;
# * an interrupt, an exception, or `int`;
# * `iret` or any other write to `ip`, also a write of the address that
#   follows (a conditional jump that is not taken writes nothing);
# * a write to a control register;
# * a page table lookup that has loaded a translation
#   (`mmu_fetch_next.s`);
# * the last word of a page.
#
# The F bit of `cr0` (0x4) makes every fetch carry its address. A reset sets
# it, and the boot code clears it.
#
# The CPU runs the same instructions with and without the addresses, so the
# program cannot tell the difference. The runner of the tests can: it writes
# down every request that the CPU sends on the bus, and the list must equal
# `cpu_fetch_next.bus`. In that file `fetch` is a fetch with an address and
# `next` is one without.
#
# The handlers count in `r6`.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_invalid r1
    sr r1 0(r0)
    li on_int r1
    sr r1 24(r0)
    li 0x0 r6

    # Up to here every fetch has carried its address. The write to `cr0`
    # clears F. The fetch after the write still carries its address, and the
    # ones after that do not.
    li 0x1 r7
    mov cr0 r0
    cmp 0x4 r0
    jne fail
    li 0x0 r0
    mov r0 cr0
    nop
    nop

    # Reads and writes of data do not end a run, and neither does a jump that
    # is not taken.
    li 0x2 r7
    li 0x80010000 r1
    li 0x1234 r0
    sr r0 0(r1)
    lr 0(r1) r2
    cmp r0 r2
    jne fail
    nop

    # A jump that is taken ends the run, also a jump to the instruction that
    # follows.
    li 0x3 r7
    jmp _next_3
_next_3:
    nop
    cmp r0 r0
    je _far_3
    jmp fail
_far_3:
    nop

    # So does every other write to `ip`.
    li 0x4 r7
    li _next_4 r0
    mov r0 ip
_next_4:
    nop

    # A call and a return.
    li 0x5 r7
    call leaf
    nop

    # `int` and `iret`.
    li 0x6 r7
    int 0x1
    nop

    # An exception: a number that is not an instruction. The handler returns
    # to the instruction after it.
    li 0x7 r7
    .long 0x000000ff
    nop
    cmp 0x2 r6
    jne fail

    # The last word of a page: the next page may be anywhere in memory. The
    # four instructions of `across` go to the end of one page of RAM and to
    # the start of the next.
    li 0x8 r7
    li across r1
    li 0x80010ff8 r2
    lr 0(r1) r0
    sr r0 0(r2)
    lr 4(r1) r0
    sr r0 4(r2)
    lr 8(r1) r0
    sr r0 8(r2)
    lr 12(r1) r0
    sr r0 12(r2)
    li 0x0 r4
    call r2
    cmp 0x3 r4
    jne fail

    # A write to a control register, also to one that has nothing to do
    # with addresses. A read does not end the run.
    li 0x9 r7
    mov cr2 r0
    nop
    mov r0 cr2
    nop

    # With F set every fetch carries its address again.
    li 0xa r7
    li 0x4 r0
    mov r0 cr0
    nop
    nop

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

leaf:
    nop
    ret

across:
    add 0x1 r4
    add 0x1 r4
    add 0x1 r4
    ret

on_int:
    add 0x1 r6
    iret

on_invalid:
    add 0x1 r6
    mov cr1 r0
    add 0x4 r0
    mov r0 cr1
    iret

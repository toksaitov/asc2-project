# The registers `ip` and `flags` as ordinary operands.
#
# Reading `ip` gives the address of the instruction that reads it. Writing
# `ip` is a jump. A write to `flags` changes only C (0x1), Z (0x2), S (0x4),
# and O (0x8); the flags I (0x10) and U (0x20) keep their values.
#
# `r7` holds the number of the current check. On a failure the program parks
# the CPU with that number in `r0`; at the end it parks with 0x600d in `r0`.

.section .data
target_addr:
    .long _loaded_target

.section .text
main:
    li 0x80020000 sp

    # Reading `ip` gives the address of the reading instruction.
    li 0x1 r7
    li _read_ip r1
_read_ip:
    mov ip r0
    cmp r1 r0
    jne fail

    # A move into `ip` jumps.
    li 0x2 r7
    li _moved_target r0
    mov r0 ip
    jmp fail
_moved_target:

    # An addition to `ip` jumps forward from the adding instruction.
    li 0x3 r7
    add 0x8 ip
    jmp fail

    # A load into `ip` jumps to an address kept in memory.
    li 0x4 r7
    li target_addr r0
    lr 0(r0) ip
    jmp fail
_loaded_target:

    # `push ip` stores the address of the push itself, and `pop ip` jumps.
    li 0x5 r7
    li _push_ip r1
_push_ip:
    push ip
    pop r0
    cmp r1 r0
    jne fail
    li _popped_target r0
    push r0
    pop ip
    jmp fail
_popped_target:

    # A store of `ip` stores the address of the store itself.
    li 0x6 r7
    li _store_ip r1
    li 0x80010000 r2
_store_ip:
    sr ip 0(r2)
    lr 0(r2) r0
    cmp r1 r0
    jne fail

    # A write to `flags` sets C, Z, S, and O, and nothing else.
    li 0x7 r7
    li -1 r0
    mov r0 flags
    mov flags r1
    cmp 0xf r1
    jne fail

    # The written flags steer the conditional jumps.
    li 0x8 r7
    li 0x2 r0
    mov r0 flags
    jne fail
    li 0x0 r0
    mov r0 flags
    je fail
    mov flags r1
    cmp 0x0 r1
    jne fail

    # A write to `flags` keeps the interrupt flag. All interrupt lines are
    # masked after a reset, so no interrupt arrives while the flag is set.
    li 0x9 r7
    sti
    li 0x0 r0
    mov r0 flags
    mov flags r1
    cli
    cmp 0x10 r1
    jne fail
    sti
    li -1 r0
    mov r0 flags
    mov flags r1
    cli
    cmp 0x1f r1
    jne fail
    mov flags r1
    cmp 0x2 r1
    jne fail

    # An arithmetic instruction with `flags` as its destination: the result
    # wins over the flags that the instruction sets for itself. The sum 0 + 5
    # sets no flag by itself, and the difference 3 - 3 would set Z.
    li 0xa r7
    li 0x0 r0
    mov r0 flags
    add 0x5 flags
    mov flags r1
    cmp 0x5 r1
    jne fail
    li 0x3 r0
    mov r0 flags
    sub 0x3 flags
    mov flags r1
    cmp 0x0 r1
    jne fail

    li 0x600d r0
    hlt

fail:
    mov r7 r0
    hlt

# The double fault: an exception inside a handler that has not saved the trap
# record.
#
# The record of the first exception is all that the handler has to return
# with, and a second exception would write over it (see `cpu_nested_trap.s`).
# The CPU keeps the record and stops instead. Only a reset starts it again.
#
# This test passes if the CPU stops with a double fault and if the line
# that the runner prints then, with the trap record in it, equals
# `cpu_double_fault.stop`: the record must tell of the first exception. A
# test that reaches a `hlt` has failed.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_misaligned r1
    sr r1 8(r0)

    # Flags for the record to show: C, Z, S, and O.
    li 0xf r0
    mov r0 flags

    # A read at an address that is no multiple of 4.
    li 0x80010000 r1
    li 0x81000000 r3
    lr 1(r1) r2

    # The handler never returns.
    li 0x1 r0
    hlt

on_misaligned:
    # Another exception inside the handler, with other flags, of another
    # kind, and at another address: a read past the end of the kernel's
    # addresses. The CPU stops here.
    cmp r3 r3
    lr 0(r3) r2

    li 0x2 r0
    hlt

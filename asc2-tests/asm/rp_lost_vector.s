# A lost answer on the way into a handler (see `rp_lost_answer.s`).
#
# To enter a handler the CPU reads the address of the handler from the
# vector table. If the bus fails on that read, the CPU cannot go on. It
# stops as it does on a double fault, and the trap record stays as the
# exception on its way has written it: the kernel is gone, but a debugger
# can still see what happened.
#
# This test passes if the CPU stops and if the line that the runner prints
# then, with the trap record in it, equals `rp_lost_vector.stop`. A test
# that reaches a `hlt` has failed.

.section .text
main:
    li 0x80020000 sp

    li 0x80000000 r0
    li on_misaligned r1
    sr r1 8(r0)

    # Flags for the record to show: C, Z, S, and O.
    li 0xf r0
    mov r0 flags

    # The second access after this order gets no answer. The first one is
    # the fetch of the `lr`. The `lr` sends nothing, as the CPU refuses an
    # address that is no multiple of 4 by itself. The second one is the read
    # of the handler's address.
    li 0x80ff0000 r3
    li 0x80010000 r1
    li 0x2 r0
    sr r0 0x30(r3)
    lr 1(r1) r2

    li 0x1 r0
    hlt

on_misaligned:
    li 0x2 r0
    hlt

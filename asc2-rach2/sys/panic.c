/*
    The end of the kernel when it finds a mistake of its own. The kernel
    cannot trust its own state after that, so it says what it has found and
    stops the machine.
*/

#include "sys/panic.h"

#include "sys/cpu.h"
#include "sys/kio.h"

void k_panic(const char *message)
{
    k_printf("Kernel panic: %s\n", message);
    k_halt(HALT_PANIC);
}

/*
    Rach2 for Almurut II: the start of the kernel.

    `loader.s` calls `kernel_main` after a reset. It prepares the parts of the
    kernel, makes the first process from the program `ish` of the disk, and
    gives the CPU to the scheduler.
*/

#include <stddef.h>

#include "sys/cpu.h"
#include "sys/disk.h"
#include "sys/isr.h"
#include "sys/kio.h"
#include "sys/panic.h"
#include "sys/pic.h"
#include "sys/pit.h"
#include "sys/rp.h"
#include "sys/syscall.h"
#include "sys/fs/krffs.h"
#include "sys/lib/kint.h"
#include "sys/mm/pages.h"
#include "sys/mm/paging.h"
#include "sys/proc/array.h"
#include "sys/proc/process.h"
#include "sys/proc/sched.h"
#include "sys/proc/slice.h"

#define INIT_PATH "ish" // the program of the first process

// Its one argument, its own name, and its environment: two strings that `ish` looks for
static const char INIT_STRS[] = "ish\0HOME=/\0PATH=/";

void kernel_main(void)
{
    init_pages();

    // Two lines that are short enough for the screen of the machine
    k_printf("Rach2 for Almurut II\n%u KB RAM, %u free\n", get_ram_size() / 1024, get_free_pages() * (PAGE_SIZE / 1024));

    init_isr();
    init_pic();
    init_syscall();
    init_paging();
    init_proc();
    init_array();
    init_rp();
    init_disk();

    if (get_disk_size() == 0) {
        k_panic("no disk");
    }
    if (!init_krffs()) {
        k_panic("no file system on the disk");
    }

    uint32_t free_pages = get_free_pages();

    exec_args_t args;
    args.strs = INIT_STRS;
    args.size = sizeof(INIT_STRS);
    args.argc = 1;
    args.envc = 2;
    if (create_process(INIT_PATH, &args) == NULL) {
        k_panic("failed to start " INIT_PATH);
    }

    init_pit(PIT_PERIOD);

    scheduler();

    // A process that has exited has given back every page that it had
    if (get_free_pages() != free_pages) {
        k_panic("pages were lost");
    }

    int32_t status = get_init_status();
    if (status != STATUS_STOPPED) {
        k_printf("The first process has ended with the exit code %u\n", EXIT_CODE_OF(status));
    }
    print_slices();
    k_printf("No processes are left: %u page faults repaired, %u pages free at the least\n",
             get_repaired_faults(), get_least_free_pages());

    // The last words for a person; the lines are short enough for the screen of the machine
    k_printf("\nGoodbye! The shell\nhas ended and the\nmachine has stopped.\nReset it to restart.\n");
    k_halt(status == 0 ? HALT_OK : HALT_FAILED);
}

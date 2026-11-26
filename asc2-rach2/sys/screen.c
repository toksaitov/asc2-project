/*
    The screen and the push button of the RP2040 for user programs.

    The device page (`RP_USER_BASE`) holds the frame memory of the screen
    and a copy of the registers that draw, show a frame, and tell the state
    of the button and the time. It is the only device page that the RP2040
    lets a user program use, and nothing in it can reach the memory of
    another program or of the kernel.

    A picture is drawn by the RP2040 out of RAM, and a program knows only
    its virtual addresses. So the program names a picture to the kernel
    first; the kernel finds where the picture is in RAM and tells the
    RP2040, which then draws it whenever the program writes its number to
    the register for pictures. A picture is at most a little over 1 KB, so
    it lies in one page or in two, and the two pages need not be next to
    each other in RAM: the RP2040 gets the address of the second one too.
*/

#include "sys/screen.h"

#include <stddef.h>

#include "sys/cpu.h"
#include "sys/machine.h"
#include "sys/rp.h"
#include "sys/syscall.h"
#include "sys/mm/pages.h"
#include "sys/mm/paging.h"

#define PAGE_OFF_MASK (PAGE_SIZE - 1)

static process_t *owner; // the process that has the screen, or NULL while the console has it

static int32_t open_screen(void)
{
    if (owner == current_proc) {
        return USER_DEVICE_ADDR;
    }
    if (owner != NULL) {
        return -EBUSY;
    }
    if (!map_device_page(current_proc->page_dir, USER_DEVICE_ADDR, RP_USER_BASE)) {
        return -ENOMEM;
    }
    owner = current_proc;

    // The console stops drawing at once, and the program starts with a dark frame memory
    write_rp(RP_SCR_CMD_ADDR, RP_SCR_CMD_CLEAR);

    return USER_DEVICE_ADDR;
}

static int32_t set_picture(uint32_t number, uint32_t addr)
{
    uint32_t *page_dir = current_proc->page_dir;

    if (number >= RP_SCR_PICS || (addr & 3) != 0) {
        return -EINVAL;
    }

    // The first word of a picture has its width and its height
    uint32_t first_word;
    if (!copy_from_user(page_dir, &first_word, addr, sizeof(first_word))) {
        return -EFAULT;
    }
    uint32_t width = first_word & 0xffff;
    uint32_t height = first_word >> 16;
    if (width == 0 || width > RP_SCR_WIDTH || height == 0 || height > RP_SCR_HEIGHT) {
        return -EINVAL;
    }
    uint32_t size = 4 + (width + 31) / 32 * 4 * height;

    uint32_t count = size;
    void *bytes = get_user_bytes(page_dir, addr, false, &count);
    if (bytes == NULL) {
        return -EFAULT;
    }
    uint32_t start = KERN_TO_PHYS(bytes);

    /*
        The RP2040 reads the size anew whenever it draws, and the program
        may have changed it by then. A picture that then leaves its first
        page goes on in the page named here, so for a picture of one page
        it is that same page: the RP2040 never reads past the pages of the
        program.
    */
    uint32_t next = start & ~PAGE_OFF_MASK;
    if (count < size) {
        uint32_t rest = size - count;
        bytes = get_user_bytes(page_dir, addr + count, false, &rest);
        if (bytes == NULL) {
            return -EFAULT;
        }
        next = KERN_TO_PHYS(bytes);
    }

    write_rp(RP_SCR_PIC_ADDR + number * 8, start);
    write_rp(RP_SCR_PIC_ADDR + number * 8 + 4, next);

    return 0;
}

int32_t screen_call(uint32_t request, uint32_t a, uint32_t b)
{
    if (request == SCREEN_OPEN) {
        return open_screen();
    }
    if (owner != current_proc) {
        return -EINVAL;
    }

    if (request == SCREEN_CLOSE) {
        release_screen(current_proc);
        return 0;
    }
    if (request == SCREEN_PICTURE) {
        return set_picture(a, b);
    }

    return -EINVAL;
}

void release_screen(process_t *proc)
{
    if (owner != proc) {
        return;
    }
    owner = NULL;

    // The pages of the pictures may soon belong to somebody else
    for (uint32_t i = 0; i < RP_SCR_PICS * 2; ++i) {
        write_rp(RP_SCR_PIC_ADDR + i * 4, 0);
    }

    if (proc->page_dir != NULL) {
        unmap_device_page(proc->page_dir, USER_DEVICE_ADDR);
        if (proc == current_proc) {
            write_cr3(KERN_TO_PHYS(proc->page_dir)); // the CPU forgets the translation
        }
    }

    write_rp(RP_SCR_CMD_ADDR, RP_SCR_CMD_CONSOLE);
}

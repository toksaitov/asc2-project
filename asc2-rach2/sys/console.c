/*
    The console of the RP2040.

    Its two registers are in memory: the status register tells if the console
    can take a byte and if a typed byte waits, a write to the data register
    prints a byte, and a read takes the next typed byte.

    Input is given to programs a line at a time. The kernel gathers the line:
    it prints every byte back, so that the person sees what was typed,
    Backspace takes the last byte away, and Enter ends the line. `ctrl+d`
    ends a line without a new line; on an empty line that is the end of the
    input.

    The kernel takes typed bytes only while a process waits for a line. The
    console may interrupt the CPU only then, and what is typed at other times
    waits in the RP2040.
*/

#include "sys/console.h"

#include <stdbool.h>

#include "sys/machine.h"
#include "sys/rp.h"
#include "sys/syscall.h"
#include "sys/mm/paging.h"
#include "sys/proc/sched.h"

#define LINE_SIZE 256 // bytes of the longest line, with its new line

#define KEY_END_OF_INPUT 0x04 // `ctrl+d`
#define KEY_BACKSPACE    0x08
#define KEY_DELETE       0x7f // what the Backspace key of most terminals sends

static uint8_t line[LINE_SIZE];
static uint32_t line_len;  // bytes of the line that have been typed
static uint32_t line_pos;  // bytes of the complete line that programs have taken
static bool is_line_ready; // the line is complete, and programs may take it

void console_putc(char ch)
{
    while (!(read_rp(RP_CON_STAT_ADDR) & RP_CON_FLAG_OUTPUT)) {
    }

    write_rp(RP_CON_DATA_ADDR, (uint8_t) ch);
}

int32_t read_console(uint32_t *page_dir, uint32_t buf, uint32_t count)
{
    if (count == 0) {
        return 0;
    }

    while (!is_line_ready) {
        enable_rp_irq(RP_IRQ_FLAG_CON);
        sleep(line);
    }

    uint32_t rest = line_len - line_pos;
    if (count > rest) {
        count = rest;
    }
    if (!copy_to_user(page_dir, buf, &line[line_pos], count)) {
        return -EFAULT;
    }

    line_pos += count;
    if (line_pos == line_len) {
        line_len = 0;
        line_pos = 0;
        is_line_ready = false;
    }

    return (int32_t) count;
}

static void end_line(void)
{
    is_line_ready = true;

    // The bytes of the next line wait in the RP2040 until a process asks for them
    disable_rp_irq(RP_IRQ_FLAG_CON);
    wakeup(line);
}

void handle_console_irq(void)
{
    while (!is_line_ready && (read_rp(RP_CON_STAT_ADDR) & RP_CON_FLAG_INPUT)) {
        uint8_t key = (uint8_t) read_rp(RP_CON_DATA_ADDR);

        // The Enter key of a terminal sends a carriage return
        if (key == '\r') {
            key = '\n';
        }

        if (key == '\n') {
            line[line_len++] = key;
            console_putc('\n');
            end_line();
        } else if (key == KEY_END_OF_INPUT) {
            end_line();
        } else if (key == KEY_BACKSPACE || key == KEY_DELETE) {
            if (line_len > 0) {
                --line_len;

                // Back one place, a space over the last byte, and back again
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
        } else if ((key >= ' ' || key == '\t') && line_len < LINE_SIZE - 1) {
            // The last place of the line is kept for the new line
            line[line_len++] = key;
            console_putc((char) key);
        }
    }
}

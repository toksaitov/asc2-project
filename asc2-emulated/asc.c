#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asc.h"

#define PROGRAM_NAME "asc"
#define HELP_MSG "Zoom out in the terminal to see all panels. "                                       \
                 "Press 'ctrl+c' to exit, 'ctrl+z' to pause/continue. "                               \
                 "When paused: 's' steps one instruction, 'j' and 'k' scroll the memory panel, "      \
                 "'g' and 'G' jump to the start of RAM and of the boot ROM."

#define KEY_CTRL_L 12

#define TOSTR_BUFF_SIZE 16384
#define DEVS_PAD_WIDTH  45
#define DEVS_PAD_HEIGHT 72
#define CON_PAD_WIDTH   80
#define CON_PAD_HEIGHT  30
#define MEM_PAD_WIDTH   56
#define MEM_PAD_HEIGHT  72
#define FOOT_PAD_HEIGHT 1

#define MEM_ROW_SIZE   8                    // bytes in one row of the memory panel (see `mem_to_string`)
#define MEM_SCRL_ROWS  16                   // rows that one key press scrolls the memory panel by
#define STEPS_PER_FRAME 5000                // instructions between two updates of the screen
#define FRAME_TIME_MS   16                  // time to wait for a key between two updates of the screen

static volatile sig_atomic_t should_exit = false;
static volatile sig_atomic_t paused = false;
static volatile sig_atomic_t should_step = true;

static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGQUIT) {
        should_exit = true;
    } else if (sig == SIGTSTP) {
        paused = !paused;
        should_step = true;
    }

    signal(SIGINT,  signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTSTP, signal_handler);
}

/*
    Reads a whole file into memory. An empty file gives a buffer of one byte so
    that the result is never NULL on success.
*/
static uint8_t *read_file(const char *path, size_t *size)
{
    assert(path != NULL);
    assert(size != NULL);

    uint8_t *bytes = NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror(path);
        return NULL;
    }

    long file_size = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        file_size = ftell(file);
    }
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        perror(path);
        goto end;
    }

    bytes = (uint8_t *) malloc(file_size > 0 ? (size_t) file_size : 1);
    if (bytes == NULL) {
        perror("Not enough memory to read a file");
        goto end;
    }

    if (file_size > 0 && fread(bytes, (size_t) file_size, 1, file) != 1) {
        perror(path);
        free(bytes);
        bytes = NULL;
        goto end;
    }

    *size = (size_t) file_size;

end:
    fclose(file);

    return bytes;
}

static bool write_file(const char *path, uint8_t *bytes, size_t size)
{
    assert(path != NULL);
    assert(bytes != NULL);

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return false;
    }

    bool status = size == 0 || fwrite(bytes, size, 1, file) == 1;
    if (fclose(file) != 0) {
        status = false;
    }
    if (!status) {
        perror(path);
    }

    return status;
}

int main(int argc, char *argv[])
{
    int status = EXIT_FAILURE;

    const char *in_file_path = NULL;
    const char *disk_file_path = NULL;
    bool keys_to_kbdc = false;
    bool is_usage_valid = true;

    firmware_t frm = {
        .image    = NULL,
        .text_sec = NULL,
        .data_sec = NULL
    };
    uint8_t *disk = NULL;
    size_t disk_size = 0;

    WINDOW *dev_pad  = NULL;
    WINDOW *con_pad  = NULL;
    WINDOW *mem_pad  = NULL;
    WINDOW *foot_pad = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0) {
            paused = true;
            should_step = false;
        } else if (strcmp(argv[i], "-k") == 0) {
            keys_to_kbdc = true;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            disk_file_path = argv[++i];
        } else if (argv[i][0] != '-' && in_file_path == NULL) {
            in_file_path = argv[i];
        } else {
            is_usage_valid = false;
        }
    }
    if (!is_usage_valid || in_file_path == NULL) {
        fprintf(
            stderr,
            "Usage:\n"
                "\t%s [-p] [-k] [-d disk_file] <path to the compiled binary>\n"
            "\n"
            "\t-p  start paused\n"
            "\t-k  send the keys to the keyboard controller instead of the console\n"
            "\t-d  use the file as the disk; the file is written back at exit\n",
            PROGRAM_NAME
        );
        goto end;
    }

    if (!firmware_load(in_file_path, &frm)) {
        goto end;
    }

    if (disk_file_path != NULL && (disk = read_file(disk_file_path, &disk_size)) == NULL) {
        goto end;
    }

    signal(SIGINT,  signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTSTP, signal_handler);

    static char tostr_buff[TOSTR_BUFF_SIZE];

    if (!initscr()) {
        fprintf(stderr, "Failed to create the program's UI\n");
        goto end;
    }
    noecho();

    dev_pad = newpad(DEVS_PAD_HEIGHT, DEVS_PAD_WIDTH);
    if (!dev_pad) {
        fprintf(stderr, "Failed to create a UI panel for general device info.\n");
        goto end;
    }

    con_pad = newpad(CON_PAD_HEIGHT, CON_PAD_WIDTH);
    if (!con_pad) {
        fprintf(stderr, "Failed to create a UI panel for the console.\n");
        goto end;
    }
    scrollok(con_pad, true); // the text moves up when the last line is full
    wtimeout(con_pad, FRAME_TIME_MS);
    keypad(con_pad, true);

    mem_pad = newpad(MEM_PAD_HEIGHT, MEM_PAD_WIDTH);
    if (!mem_pad) {
        fprintf(stderr, "Failed to create a UI panel for memory info.\n");
        goto end;
    }
    uint32_t mem_addr = RAM_BASE; // physical address of the first row of the memory panel

    foot_pad = newpad(FOOT_PAD_HEIGHT, (int) sizeof("Almurut II (Double Fault). " HELP_MSG));
    if (!foot_pad) {
        fprintf(stderr, "Failed to create a UI panel for the footer.\n");
        goto end;
    }

    static device_t dev;
    dev_init(&dev, &frm);
    rp_attach_disk(&dev, disk, disk_size);

    bool has_scancode = false;
    uint8_t scancode = 0;

    while (!should_exit) {
        if (should_step) {
            size_t steps = paused ? 1 : STEPS_PER_FRAME;
            for (size_t i = 0; i < steps; ++i) {
                dev_step(&dev, has_scancode, scancode);
                has_scancode = false;

                uint8_t con_out_byte;
                if (rp_take_con_output(&dev, &con_out_byte)) {
                    waddch(con_pad, con_out_byte);
                }

                if (dev.cpu.double_fault || cpu_is_parked(&dev.cpu)) { // nothing will change from here on
                    break;
                }
            }
            if (paused) {
                should_step = false;
            }
        }

        werase(dev_pad);
        wattron(dev_pad, A_BOLD);
        cpu_to_string(&dev.cpu, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(dev_pad, tostr_buff); waddstr(dev_pad, "\n\n");
        pic_to_string(&dev.pic, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(dev_pad, tostr_buff); waddstr(dev_pad, "\n\n");
        pit_to_string(&dev.pit, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(dev_pad, tostr_buff); waddstr(dev_pad, "\n\n");
        kbdc_to_string(&dev.kbdc, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(dev_pad, tostr_buff); waddstr(dev_pad, "\n\n");
        rp_to_string(&dev.rp, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(dev_pad, tostr_buff);
        wattroff(dev_pad, A_BOLD);

        werase(mem_pad);
        mem_to_string(&dev.mem, mem_addr, MEM_PAD_HEIGHT - 1, tostr_buff, TOSTR_BUFF_SIZE);
        waddstr(mem_pad, tostr_buff);

        werase(foot_pad);
        if (dev.cpu.double_fault) {
            waddstr(foot_pad, "Almurut II (Double Fault). " HELP_MSG);
        } else if (cpu_is_parked(&dev.cpu)) {
            waddstr(foot_pad, "Almurut II (Halted). " HELP_MSG);
        } else if (paused) {
            waddstr(foot_pad, "Almurut II (Paused). " HELP_MSG);
        } else {
            waddstr(foot_pad, "Almurut II. " HELP_MSG);
        }

        int win_height, win_width;
        getmaxyx(stdscr, win_height, win_width);
        wnoutrefresh(stdscr);
        pnoutrefresh(
            dev_pad,
            0, 0,
            0, 0,
            win_height - FOOT_PAD_HEIGHT - 1, DEVS_PAD_WIDTH - 1
        );
        pnoutrefresh(
            mem_pad,
            0, 0,
            0, DEVS_PAD_WIDTH + CON_PAD_WIDTH + 1,
            win_height - FOOT_PAD_HEIGHT - 1, win_width - 1
        );
        pnoutrefresh(
            foot_pad,
            0, 0,
            win_height - FOOT_PAD_HEIGHT, 0,
            win_height, win_width - 1
        );
        // The console goes last, so the cursor of the terminal stays where the console's next character will appear
        pnoutrefresh(
            con_pad,
            0, 0,
            0, DEVS_PAD_WIDTH,
            win_height - FOOT_PAD_HEIGHT - 1, DEVS_PAD_WIDTH + CON_PAD_WIDTH - 1
        );
        doupdate();

        int key = wgetch(con_pad);
        if (key == KEY_RESIZE || key == KEY_CTRL_L) {
            clear();
            refresh();
            continue;
        }
        if (key == ERR || key == KEY_MOUSE) {
            continue;
        }

        if (!paused) {
            // A running machine gets the key: its console by default, its keyboard controller with `-k`
            if (key == KEY_BACKSPACE) {
                key = '\b';
            } else if (key == KEY_ENTER) {
                key = '\n';
            }
            if (key > UINT8_MAX) { // a key with no character, such as an arrow
                continue;
            }

            if (keys_to_kbdc) {
                has_scancode = true;
                scancode = (uint8_t) key;
            } else {
                rp_put_con_input(&dev, (uint8_t) key); // a key pressed while the queue is full is lost
            }
            continue;
        }

        switch (key) {
            case 's':
                should_step = true;
                break;
            case 'j':
                if (mem_addr <= UINT32_MAX - 2 * MEM_SCRL_ROWS * MEM_ROW_SIZE) {
                    mem_addr += MEM_SCRL_ROWS * MEM_ROW_SIZE;
                }
                break;
            case 'k':
                if (mem_addr >= MEM_SCRL_ROWS * MEM_ROW_SIZE) {
                    mem_addr -= MEM_SCRL_ROWS * MEM_ROW_SIZE;
                }
                break;
            case 'g':
                mem_addr = RAM_BASE;
                break;
            case 'G':
                mem_addr = ROM_BASE;
                break;
        }
    }

    status = EXIT_SUCCESS;

end:
    if (dev_pad  != NULL ||
        con_pad  != NULL ||
        mem_pad  != NULL ||
        foot_pad != NULL) {
        if (dev_pad != NULL) {
            delwin(dev_pad);
            dev_pad = NULL;
        }
        if (con_pad != NULL) {
            delwin(con_pad);
            con_pad = NULL;
        }
        if (mem_pad != NULL) {
            delwin(mem_pad);
            mem_pad = NULL;
        }
        if (foot_pad != NULL) {
            delwin(foot_pad);
            foot_pad = NULL;
        }
        endwin();
    }

    if (disk != NULL) {
        // The disk keeps what the machine has written to it
        if (status == EXIT_SUCCESS && !write_file(disk_file_path, disk, disk_size)) {
            status = EXIT_FAILURE;
        }

        free(disk);
        disk = NULL;
    }

    firmware_free(&frm);

    return status;
}

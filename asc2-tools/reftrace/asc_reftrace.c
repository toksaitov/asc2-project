#include "trace_common.h"

#define PROGRAM_NAME "asc_reftrace"

typedef struct trace_args
{
    size_t max_retired;           // number of retired instructions after which the tool gives up
    bool is_quiet;                // print no trace rows
    bool is_ram_dirty;            // fill the RAM that the image does not set with `TRACE_DIRTY_RAM_BYTE`
    const char *event_file_path;  // scancodes for the keyboard controller by retirement order
    const char *con_in_file_path; // bytes that the console receives
    const char *con_out_file_path; // file for the bytes that the program prints on the console
    const char *disk_file_path;   // contents of the disk
    const char *disk_out_file_path; // file for the contents of the disk at the end
    const char *bus_file_path;    // file for the requests that the CPU sends on the bus
    const char *screen_file_path; // file for what the screen shows at the end
    const char *film_file_path;   // file for every frame that reaches the screen
    const char *in_file_path;     // firmware image to run
} trace_args_t;

static int parse_args(int argc, char *argv[], trace_args_t *args)
{
    assert(args != NULL);

    args->max_retired = TRACE_DEFAULT_MAX_RETIRED;
    args->is_quiet = false;
    args->is_ram_dirty = false;
    args->event_file_path = NULL;
    args->con_in_file_path = NULL;
    args->con_out_file_path = NULL;
    args->disk_file_path = NULL;
    args->disk_out_file_path = NULL;
    args->bus_file_path = NULL;
    args->screen_file_path = NULL;
    args->film_file_path = NULL;
    args->in_file_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-n") == 0) {
            char *endptr = NULL;
            unsigned long parsed = 0;

            if (++i >= argc) {
                return -1;
            }

            errno = 0;
            parsed = strtoul(argv[i], &endptr, 10);
            if (errno != 0 || endptr == argv[i] || *endptr != '\0' || parsed == 0) {
                return -1;
            }

            args->max_retired = (size_t) parsed;
        } else if (strcmp(argv[i], "-q") == 0) {
            args->is_quiet = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            args->is_ram_dirty = true;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "-i") == 0 ||
                   strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-d") == 0 ||
                   strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-b") == 0 ||
                   strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-m") == 0) {
            char option = argv[i][1];

            if (++i >= argc) {
                return -1;
            }

            if (option == 'e') {
                args->event_file_path = argv[i];
            } else if (option == 'i') {
                args->con_in_file_path = argv[i];
            } else if (option == 'o') {
                args->con_out_file_path = argv[i];
            } else if (option == 'd') {
                args->disk_file_path = argv[i];
            } else if (option == 's') {
                args->disk_out_file_path = argv[i];
            } else if (option == 'f') {
                args->screen_file_path = argv[i];
            } else if (option == 'm') {
                args->film_file_path = argv[i];
            } else {
                args->bus_file_path = argv[i];
            }
        } else if (args->in_file_path == NULL) {
            args->in_file_path = argv[i];
        } else {
            return -1;
        }
    }

    return args->in_file_path != NULL ? 0 : -1;
}

int main(int argc, char *argv[])
{
    int status = TRACE_EXIT_ERROR;
    trace_args_t args;
    trace_event_t *events = NULL;
    size_t events_count = 0;
    size_t events_pos = 0;
    uint8_t *con_in = NULL;
    size_t con_in_size = 0;
    size_t con_in_pos = 0;
    FILE *con_out_file = NULL;
    FILE *bus_file = NULL;
    FILE *film_file = NULL;
    uint8_t *disk = NULL;
    size_t disk_size = 0;
    firmware_t frm = {
        .image = NULL,
        .text_sec = NULL,
        .data_sec = NULL
    };
    static device_t dev;

    if (parse_args(argc, argv, &args) != 0) {
        fprintf(
            stderr,
            "Usage:\n"
                "\t%s [-n max_retired] [-q] [-r] [-e event_file] [-i console_input_file] [-o console_output_file] [-d disk_file] [-s disk_output_file] [-b bus_log_file] [-f screen_file] [-m film_file] <path to the compiled binary>\n",
            PROGRAM_NAME
        );
        goto end;
    }

    if (!firmware_load(args.in_file_path, &frm)) {
        goto end;
    }

    if (trace_read_events(args.event_file_path, &events, &events_count) != 0) {
        goto end;
    }

    if (args.con_in_file_path != NULL && (con_in = trace_read_file(args.con_in_file_path, &con_in_size)) == NULL) {
        goto end;
    }

    if (args.con_out_file_path != NULL && (con_out_file = fopen(args.con_out_file_path, "wb")) == NULL) {
        perror("Failed to create the console output file");
        goto end;
    }

    if (args.disk_file_path != NULL && (disk = trace_read_file(args.disk_file_path, &disk_size)) == NULL) {
        goto end;
    }

    if (args.bus_file_path != NULL) {
        if ((bus_file = fopen(args.bus_file_path, "w")) == NULL) {
            perror("Failed to create the bus log file");
            goto end;
        }
        fputs(TRACE_BUS_CSV_HEADER "\n", bus_file);
    }

    if (args.film_file_path != NULL && (film_file = fopen(args.film_file_path, "w")) == NULL) {
        perror("Failed to create the film file");
        goto end;
    }

    dev_init(&dev, &frm);
    rp_attach_disk(&dev, disk, disk_size);

    if (args.is_ram_dirty) {
        trace_dirty_ram(&dev, &frm);
    }

    if (!args.is_quiet) {
        puts(TRACE_CSV_HEADER);
    }

    /*
        One row is printed for every retired instruction, with the state of
        the CPU after it. An instruction that raises an exception does not
        retire; the trap record in the control registers shows up in the row
        of the first instruction of the handler. An `int` retires, and its row
        shows the trap record. A `hlt` retires once, when it
        halts the CPU. The trace ends when the CPU is parked: halted with no
        way for an interrupt to wake it.
    */
    size_t order = 0;
    size_t wait_steps = 0;
    uint32_t film_frames = 0;

    while (order < args.max_retired) {
        bool has_scancode = false;
        uint8_t scancode = 0x00;
        uint32_t ret_ip = dev.cpu.ip;

        // The console gets its input as fast as the program takes it
        while (con_in_pos < con_in_size && rp_put_con_input(&dev, con_in[con_in_pos])) {
            ++con_in_pos;
        }

        cpu_step(&dev);

        size_t bus_rows = 0;
        if (bus_file != NULL) {
            trace_print_bus_rows(bus_file, &dev, &bus_rows);
        }

        if (dev.cpu.retired) {
            wait_steps = 0;
            if (!args.is_quiet) {
                trace_print_row(order, ret_ip, &dev.cpu);
            }

            if (events_pos < events_count && events[events_pos].order == order) {
                if (events[events_pos].is_button) {
                    rp_set_button(&dev, events[events_pos].is_down);
                } else {
                    has_scancode = true;
                    scancode = events[events_pos].scancode;
                }
                ++events_pos;
            }

            ++order;
        } else if (dev.cpu.halted && ++wait_steps > TRACE_MAX_WAIT_STEPS) {
            fputs("The CPU is halted and no interrupt wakes it\n", stderr);
            status = TRACE_EXIT_NO_END;
            goto end;
        }

        uint8_t con_out_byte;
        if (rp_take_con_output(&dev, &con_out_byte) && con_out_file != NULL) {
            fputc(con_out_byte, con_out_file);
        }

        if (dev.cpu.double_fault) {
            trace_report_double_fault(&dev.cpu);
            status = TRACE_EXIT_DOUBLE_FAULT;
            goto end;
        }

        if (cpu_is_parked(&dev.cpu)) {
            status = trace_report_parked(&dev.cpu);
            goto end;
        }

        mem_step(&dev);
        pic_step(&dev);
        pit_step(&dev);
        kbdc_step(&dev, has_scancode, scancode);
        rp_step(&dev);
        pic_deliver_irq(&dev); // pending requests are delivered at the instruction boundary

        /*
            The film: a line for every frame that has reached the screen,
            with the number of instructions finished before it, the clock of
            the RP2040 in microseconds, and the 1,024 bytes of the frame in
            hex.
        */
        if (film_file != NULL && dev.rp.scr_frames != film_frames) {
            film_frames = dev.rp.scr_frames;
            fprintf(film_file, "%zu %u ", order, (unsigned) dev.rp.time_us);
            for (size_t i = 0; i < RP_SCR_SIZE; ++i) {
                fprintf(film_file, "%02x", dev.rp.scr_shown[i]);
            }
            fputc('\n', film_file);
        }

        // The CPU reads the address of the handler when it takes an interrupt
        if (bus_file != NULL) {
            trace_print_bus_rows(bus_file, &dev, &bus_rows);
        }
    }

    fputs("The retirement limit was reached before the program parked the CPU\n", stderr);
    status = TRACE_EXIT_NO_END;

end:
    // The disk is saved however the run has ended: what a failed run has left on it helps to find the reason
    if (args.disk_out_file_path != NULL && disk != NULL &&
        trace_write_file(args.disk_out_file_path, disk, disk_size) != 0) {
        status = TRACE_EXIT_ERROR;
    }

    if (args.screen_file_path != NULL && status != TRACE_EXIT_ERROR &&
        trace_write_screen(args.screen_file_path, &dev.rp) != 0) {
        status = TRACE_EXIT_ERROR;
    }

    firmware_free(&frm);

    if (events != NULL) {
        free(events);
    }

    if (con_in != NULL) {
        free(con_in);
    }

    if (con_out_file != NULL) {
        fclose(con_out_file);
    }

    if (bus_file != NULL) {
        fclose(bus_file);
    }

    if (film_file != NULL) {
        fclose(film_file);
    }

    if (disk != NULL) {
        free(disk);
    }

    return status;
}

/*
    asc_elf2fw: turns a linked program into a firmware image.

    The C compiler links a program into an ELF file. The program headers of
    the file list the parts to load: the text, which the linker has placed at
    the start of the boot ROM, and the data, which it has placed in RAM at
    `FIRMWARE_DATA_ADDR`. A firmware image holds the same two parts after a
    short header, so the tool checks the addresses and copies the parts.

    The variables without a starting value come after the data in memory, and
    the file has no bytes for them. A program sets them to zero in its first
    instructions.
*/

#include "../../asc2-emulated/asc.h"

#define PROGRAM_NAME "asc_elf2fw"

static uint8_t *read_file(const char *path, size_t *size)
{
    assert(path != NULL);
    assert(size != NULL);

    uint8_t *bytes = NULL;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror(PROGRAM_NAME ": failed to open the program");
        return NULL;
    }

    long file_size = 0;
    if (fseek(file, 0, SEEK_END) != 0 || (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        perror(PROGRAM_NAME ": failed to find the size of the program");
        goto end;
    }

    bytes = (uint8_t *) malloc(file_size > 0 ? (size_t) file_size : 1);
    if (bytes == NULL) {
        perror(PROGRAM_NAME ": not enough memory to read the program");
        goto end;
    }

    if (file_size > 0 && fread(bytes, (size_t) file_size, 1, file) != 1) {
        fputs(PROGRAM_NAME ": failed to read the program\n", stderr);
        free(bytes);
        bytes = NULL;
        goto end;
    }

    *size = (size_t) file_size;

end:
    fclose(file);

    return bytes;
}

static bool is_asc2_program(const elf_header_t *header)
{
    assert(header != NULL);

    return header->ident[0] == 0x7f && header->ident[1] == 'E' && header->ident[2] == 'L' && header->ident[3] == 'F' &&
           header->ident[4] == ELF_CLASS_32 && header->ident[5] == ELF_DATA_LSB &&
           header->type == ELF_TYPE_EXEC && header->machine == ELF_MACHINE_ASC2;
}

int main(int argc, char *argv[])
{
    int status = EXIT_FAILURE;

    FILE *out_file = NULL;

    if (argc != 3) {
        fputs("Usage: " PROGRAM_NAME " path_to_program output_file\n", stderr);
        return EXIT_FAILURE;
    }

    size_t size = 0;
    uint8_t *bytes = read_file(argv[1], &size);
    if (bytes == NULL) {
        return EXIT_FAILURE;
    }

    elf_header_t header;
    if (size < sizeof(header)) {
        fputs(PROGRAM_NAME ": the file is too short for an ELF header\n", stderr);
        goto end;
    }
    memcpy(&header, bytes, sizeof(header));

    if (!is_asc2_program(&header)) {
        fputs(PROGRAM_NAME ": the file is not a linked program for Almurut II\n", stderr);
        goto end;
    }

    // The CPU starts at the first instruction of the boot ROM, so the entry
    // point of the program must be there. The object file with the start-up
    // code goes first on the command line of the linker.
    if (header.entry != FIRMWARE_TEXT_ADDR) {
        fprintf(stderr, PROGRAM_NAME ": the program starts at 0x%08x, and the CPU starts at 0x%08x\n",
                (unsigned) header.entry, (unsigned) FIRMWARE_TEXT_ADDR);
        goto end;
    }

    if (header.phentsize != sizeof(elf_prog_header_t) ||
        header.phoff > size || (size_t) header.phnum * sizeof(elf_prog_header_t) > size - header.phoff) {
        fputs(PROGRAM_NAME ": the program headers are not inside the file\n", stderr);
        goto end;
    }

    const uint8_t *text_sec = NULL;
    uint32_t text_sec_size = 0;

    const uint8_t *data_sec = NULL;
    uint32_t data_sec_size = 0;
    uint32_t data_mem_size = 0;

    for (size_t i = 0; i < header.phnum; ++i) {
        elf_prog_header_t part;
        memcpy(&part, bytes + header.phoff + i * sizeof(part), sizeof(part));

        if (part.type != ELF_PROG_LOAD || part.memsz == 0) {
            continue;
        }

        if (part.filesz > part.memsz || part.offset > size || part.filesz > size - part.offset) {
            fputs(PROGRAM_NAME ": a part of the program is not inside the file\n", stderr);
            goto end;
        }

        if (part.vaddr == FIRMWARE_TEXT_ADDR && text_sec == NULL) {
            text_sec = bytes + part.offset;
            text_sec_size = part.filesz;
        } else if (part.vaddr == FIRMWARE_DATA_ADDR && data_sec == NULL) {
            data_sec = bytes + part.offset;
            data_sec_size = part.filesz;
            data_mem_size = part.memsz;
        } else {
            fprintf(stderr, PROGRAM_NAME ": a part of the program is at 0x%08x; "
                            "the text must be linked at 0x%08x and the data at 0x%08x\n",
                    (unsigned) part.vaddr, (unsigned) FIRMWARE_TEXT_ADDR, (unsigned) FIRMWARE_DATA_ADDR);
            goto end;
        }
    }

    if (text_sec == NULL) {
        fputs(PROGRAM_NAME ": the program has no text\n", stderr);
        goto end;
    }

    if (text_sec_size > MAX_FIRMWARE_TEXT_SIZE) {
        fprintf(stderr, PROGRAM_NAME ": the text takes %u bytes, and the boot ROM has %u\n",
                (unsigned) text_sec_size, (unsigned) MAX_FIRMWARE_TEXT_SIZE);
        goto end;
    }

    if (data_mem_size > MAX_FIRMWARE_DATA_SIZE) {
        fprintf(stderr, PROGRAM_NAME ": the data takes %u bytes, and the RAM after 0x%08x has %u\n",
                (unsigned) data_mem_size, (unsigned) FIRMWARE_DATA_ADDR, (unsigned) MAX_FIRMWARE_DATA_SIZE);
        goto end;
    }

    out_file = fopen(argv[2], "wb");
    if (out_file == NULL) {
        perror(PROGRAM_NAME ": failed to open the output file");
        goto end;
    }

    bin_fmt_header_t bin_fmt_header = {
        .magic = BIN_FMT_MAGIC,
        .data_sec_size = data_sec_size,
        .text_sec_size = text_sec_size
    };

    if (fwrite(&bin_fmt_header, sizeof(bin_fmt_header), 1, out_file) != 1 ||
        (text_sec_size > 0 && fwrite(text_sec, text_sec_size, 1, out_file) != 1) ||
        (data_sec_size > 0 && fwrite(data_sec, data_sec_size, 1, out_file) != 1)) {
        fputs(PROGRAM_NAME ": failed to write the firmware image\n", stderr);
        goto end;
    }

    status = EXIT_SUCCESS;

end:
    if (out_file != NULL && fclose(out_file) != 0) {
        perror(PROGRAM_NAME ": failed to finish the output file");
        status = EXIT_FAILURE;
    }

    free(bytes);

    return status;
}

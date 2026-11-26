/*
    The start of a program.

    A program is an ELF file. The file starts with a header, which tells where
    the first instruction of the program is and where the table of program
    headers is. Each program header of the type "load" describes one part of
    the program: `filesz` bytes of the file, from `offset`, belong at the
    address `vaddr`, and the part takes `memsz` bytes of memory. The bytes
    that the file does not have are variables without a starting value, which
    are zeros. The linker writes two such parts: the code, and the variables.

    The file is never in memory as a whole. The two kinds of headers are read
    into the kernel, and the bytes of every part go from the disk straight
    into the pages of the program.

    A program starts with its arguments on its stack. From the stack pointer
    up there are: the number of arguments, the addresses of the argument
    strings, a zero, the addresses of the strings of the environment, a zero,
    and at the top of the stack the strings themselves. All of that must fit
    in the last page of the stack.
*/

#include "sys/proc/exec.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/machine.h"
#include "sys/syscall.h"
#include "sys/fs/file.h"
#include "sys/fs/krffs.h"
#include "sys/lib/kstring.h"
#include "sys/mm/paging.h"

#define ELF_CLASS_32   1
#define ELF_DATA_LSB   1
#define ELF_TYPE_EXEC  2
#define ELF_PROG_LOAD  1
#define ELF_PROG_EXEC  0b00000001 // flag of a part with instructions
#define ELF_PROG_WRITE 0b00000010 // flag of a part that the program may write

#define MAX_PARTS 8 // program headers of a file

typedef struct elf_header
{
    uint8_t  ident[16]; // 0x7f, 'E', 'L', 'F', the class (32 bits), the byte order (low byte first), and padding
    uint16_t type;      // kind of the file
    uint16_t machine;   // machine that the code is for
    uint32_t version;
    uint32_t entry;     // address of the first instruction
    uint32_t phoff;     // position of the program headers in the file
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize; // size of one program header
    uint16_t phnum;     // number of program headers
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_t;

typedef struct elf_prog_header
{
    uint32_t type;   // `ELF_PROG_LOAD` for a part to load
    uint32_t offset; // position of the bytes of the part in the file
    uint32_t vaddr;  // address of the part in memory
    uint32_t paddr;
    uint32_t filesz; // number of bytes of the part in the file
    uint32_t memsz;  // number of bytes of the part in memory
    uint32_t flags;
    uint32_t align;
} elf_prog_header_t;

static bool is_program(const elf_header_t *header)
{
    return header->ident[0] == 0x7f && header->ident[1] == 'E' && header->ident[2] == 'L' && header->ident[3] == 'F' &&
           header->ident[4] == ELF_CLASS_32 && header->ident[5] == ELF_DATA_LSB &&
           header->type == ELF_TYPE_EXEC && header->machine == ELF_MACHINE_ASC2;
}

// Reads exactly `size` bytes of a file.
static int32_t read_all(const krffs_node_t *file, uint32_t offset, void *dst, uint32_t size)
{
    int32_t result = krffs_read(file, offset, dst, size);
    if (result < 0) {
        return result;
    }

    return (uint32_t) result == size ? 0 : -ENOEXEC;
}

static int32_t load_part(const krffs_node_t *file, uint32_t *page_dir, const elf_prog_header_t *part)
{
    bool writable = (part->flags & ELF_PROG_WRITE) != 0;

    uint32_t start = part->vaddr;
    uint32_t file_end = start + part->filesz;
    uint32_t end = start + part->memsz;

    for (uint32_t page_addr = start & ~(PAGE_SIZE - 1); page_addr < end; page_addr += PAGE_SIZE) {
        uint8_t *page = (uint8_t *) map_user_page(page_dir, page_addr, writable);
        if (page == NULL) {
            return -ENOMEM;
        }

        // The bytes of the file that belong to this page
        uint32_t from = page_addr > start ? page_addr : start;
        uint32_t to = page_addr + PAGE_SIZE < file_end ? page_addr + PAGE_SIZE : file_end;
        if (from < to) {
            int32_t result = read_all(file, part->offset + (from - start), page + (from - page_addr), to - from);
            if (result != 0) {
                return result;
            }
        }
    }

    return 0;
}

/*
    Reads the parts of a program into the page tables of `page_dir`. `entry`
    gets the address of the first instruction. The pages that were added stay
    in the tables for the caller to free if the file turns out wrong.
*/
static int32_t load_file(const krffs_node_t *file, uint32_t *page_dir, uint32_t *entry)
{
    elf_header_t header;
    int32_t result = read_all(file, 0, &header, sizeof(header));
    if (result != 0) {
        return result;
    }

    if (!is_program(&header) || header.phentsize != sizeof(elf_prog_header_t) ||
        header.phnum == 0 || header.phnum > MAX_PARTS) {
        return -ENOEXEC;
    }

    elf_prog_header_t parts[MAX_PARTS];
    result = read_all(file, header.phoff, parts, header.phnum * sizeof(elf_prog_header_t));
    if (result != 0) {
        return result;
    }

    bool has_entry = false;
    for (uint32_t i = 0; i < header.phnum; ++i) {
        const elf_prog_header_t *part = &parts[i];

        if (part->type != ELF_PROG_LOAD || part->memsz == 0) {
            continue;
        }

        // The bytes must be inside the file, and the part between the first page of a program and its stack
        if (part->filesz > part->memsz || part->offset > file->data_size ||
            part->filesz > file->data_size - part->offset ||
            part->vaddr < USER_START || part->vaddr > USER_STACK_START || part->memsz > USER_STACK_START - part->vaddr) {
            return -ENOEXEC;
        }

        if ((part->flags & ELF_PROG_EXEC) && header.entry >= part->vaddr && header.entry - part->vaddr < part->memsz) {
            has_entry = true;
        }

        result = load_part(file, page_dir, part);
        if (result != 0) {
            return result;
        }
    }

    // The first instruction must be in a part with instructions, at the address of a word
    if (!has_entry || (header.entry & 3) != 0) {
        return -ENOEXEC;
    }
    *entry = header.entry;

    return 0;
}

/*
    Puts the arguments and the environment into the last page of the stack.
    `sp` gets the stack pointer of the program at its start.
*/
static int32_t put_args(uint32_t *page_dir, const exec_args_t *args, uint32_t *sp)
{
    // The strings end at the top of the stack; the words below them: the number of arguments, the addresses, and two zeros
    uint32_t strs_addr = (USER_STACK_TOP - args->size) & ~3u;
    uint32_t words = 1 + args->argc + 1 + args->envc + 1;
    if (args->size > PAGE_SIZE || words * sizeof(uint32_t) > strs_addr - (USER_STACK_TOP - PAGE_SIZE)) {
        return -E2BIG;
    }

    uint32_t page_addr = USER_STACK_TOP - PAGE_SIZE;
    uint8_t *page = (uint8_t *) map_user_page(page_dir, page_addr, true);
    if (page == NULL) {
        return -ENOMEM;
    }

    memcpy(page + (strs_addr - page_addr), args->strs, args->size);

    *sp = strs_addr - words * sizeof(uint32_t);
    uint32_t *word = (uint32_t *) (page + (*sp - page_addr));

    // The two lists of addresses, each with a zero at its end
    uint32_t counts[2] = { args->argc, args->envc };
    const char *str = args->strs;
    *word++ = args->argc;
    for (uint32_t list = 0; list < 2; ++list) {
        for (uint32_t i = 0; i < counts[list]; ++i) {
            *word++ = strs_addr + (uint32_t) (str - args->strs);
            str += strlen(str) + 1;
        }
        *word++ = 0;
    }

    return 0;
}

int32_t load_program(const char *path, const exec_args_t *args, program_t *program)
{
    const char *name;
    int32_t kind = parse_path(path, &name);
    if (kind < 0) {
        return kind;
    }
    if (kind == 1) {
        // The directory of the disk is not a program
        return -EACCES;
    }

    program->page_dir = create_page_dir();
    if (program->page_dir == NULL) {
        return -ENOMEM;
    }

    enter_krffs();

    krffs_node_t file;
    int32_t result = krffs_find(name, &file);
    if (result == 0) {
        result = load_file(&file, program->page_dir, &program->entry);
    }

    leave_krffs();

    if (result == 0) {
        result = put_args(program->page_dir, args, &program->sp);
    }
    if (result != 0) {
        free_page_dir(program->page_dir);
    }

    return result;
}

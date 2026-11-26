/*
    A program that checks `execve`.

    The kernel builds the memory of the new program next to the old one, so
    a call that fails returns to this program, which goes on. The program
    gives the kernel names that are no files, files that are no programs, and
    programs with one wrong number in their headers. Then it starts `args`,
    which prints the arguments that it gets.
*/

#include "user.h"

#include "check.h"

#define PROGRAM "args"      // a program of the disk
#define BAD_PROGRAM "bad"   // the file for the copies of the program with a mistake

#define MAX_PROGRAM_SIZE 4096
#define MAX_ARGS 32         // arguments that the kernel takes
#define LONG_STRING_SIZE 5000

/* Positions in an ELF file */

#define ELF_TYPE_OFF 16     // a word with two numbers: the kind of the file, and above it the machine
#define ELF_ENTRY_OFF 24
#define ELF_PHNUM_OFF 44    // a word with two numbers: the number of program headers is the low one
#define ELF_HEADER_SIZE 52
#define PART_VADDR_OFF (ELF_HEADER_SIZE + 8)   // the fields of the first program header
#define PART_FILESZ_OFF (ELF_HEADER_SIZE + 16)
#define PART_MEMSZ_OFF (ELF_HEADER_SIZE + 20)

#define KERNEL_START 0x80000000

// The bytes of `PROGRAM`, as words: the numbers of its headers are read and written in place
static unsigned int program[MAX_PROGRAM_SIZE / 4];
static int program_size;

static char long_string[LONG_STRING_SIZE];

static char *no_strings[] = { NULL };

static unsigned int *get_field(int offset)
{
    return (unsigned int *) ((char *) program + offset);
}

/*
    Writes the first `size` bytes of the program into the file `BAD_PROGRAM`
    and asks the kernel to start it. Returns what `execve` returns; it does
    return, if the kernel sees the mistake.
*/
static int start_copy(int size)
{
    int fd = creat(BAD_PROGRAM, 0755);
    if (fd < 0 || write(fd, program, size) != size || close(fd) != 0) {
        return 0;
    }

    return execve(BAD_PROGRAM, no_strings, no_strings);
}

/*
    Starts a copy of the program with another value in one of its 32-bit
    fields. Returns 0 if the kernel refuses the copy as a file that is not a
    program.
*/
static int start_with_field(int offset, unsigned int value)
{
    unsigned int *field = get_field(offset);

    unsigned int old_value = *field;
    *field = value;
    int result = start_copy(program_size);
    *field = old_value;

    return result == -ENOEXEC ? 0 : 1;
}

static int start_bad_programs(void)
{
    int fd = open(PROGRAM, O_RDONLY, 0);
    program_size = read(fd, program, MAX_PROGRAM_SIZE);
    close(fd);
    if (program_size < ELF_HEADER_SIZE || program_size == MAX_PROGRAM_SIZE) {
        return 1;
    }

    unsigned int entry = *get_field(ELF_ENTRY_OFF);
    unsigned int file_size = *get_field(PART_FILESZ_OFF);

    int failures = 0;

    unsigned int type = *get_field(ELF_TYPE_OFF);

    // Not an ELF file, a file that is not a program, a program for the i386, and a file without program headers
    failures += start_with_field(0, 0);
    failures += start_with_field(ELF_TYPE_OFF, (type & 0xffff0000) | 1);
    failures += start_with_field(ELF_TYPE_OFF, (3 << 16) | (type & 0xffff));
    failures += start_with_field(ELF_PHNUM_OFF, *get_field(ELF_PHNUM_OFF) & 0xffff0000);

    // More program headers than the file has room for
    failures += start_with_field(ELF_PHNUM_OFF, (*get_field(ELF_PHNUM_OFF) & 0xffff0000) | 1000);

    // A first instruction in the kernel, outside the program, and at an address where no instruction can be
    failures += start_with_field(ELF_ENTRY_OFF, KERNEL_START + 0x800000);
    failures += start_with_field(ELF_ENTRY_OFF, entry + 0x100000);
    failures += start_with_field(ELF_ENTRY_OFF, entry + 2);

    // A part of the program in the memory of the kernel, at the address 0, and across the end of the memory of a program
    failures += start_with_field(PART_VADDR_OFF, KERNEL_START + 0x1000);
    failures += start_with_field(PART_VADDR_OFF, 0);
    failures += start_with_field(PART_VADDR_OFF, 0x007ff000);
    failures += start_with_field(PART_VADDR_OFF, 0xfffff000);

    // A part with more bytes than the file has, and with more bytes in the file than in memory
    failures += start_with_field(PART_FILESZ_OFF, 0x100000);
    failures += start_with_field(PART_MEMSZ_OFF, file_size - 4);
    failures += start_with_field(PART_MEMSZ_OFF, 0xfffffff0);

    // A file that ends inside its headers, and inside its code
    failures += start_copy(ELF_HEADER_SIZE - 4) == -ENOEXEC ? 0 : 1;
    failures += start_copy(ELF_HEADER_SIZE + 16) == -ENOEXEC ? 0 : 1;
    failures += start_copy(program_size / 2) == -ENOEXEC ? 0 : 1;

    // The whole copy is a program
    int pid = fork();
    if (pid == 0) {
        close(STDOUT);
        exit(start_copy(program_size));
    }

    int status = -1;
    if (pid < 0 || waitpid(pid, &status, 0) != pid || status != 0) {
        ++failures;
    }

    unlink(BAD_PROGRAM);

    return failures;
}

static int start_with_bad_arguments(void)
{
    char *args[MAX_ARGS + 2];

    // No file, a name that is too long, and no name at all
    for (int i = 0; i < LONG_STRING_SIZE - 1; ++i) {
        long_string[i] = 'x';
    }
    if (execve("nothing", no_strings, no_strings) != -ENOENT ||
        execve(long_string, no_strings, no_strings) != -ENAMETOOLONG ||
        execve("", no_strings, no_strings) != -ENOENT ||
        execve("/", no_strings, no_strings) != -EACCES ||
        execve("a/b", no_strings, no_strings) != -ENOENT) {
        return 1;
    }

    // A file that is not a program
    int fd = creat(BAD_PROGRAM, 0755);
    write(fd, "echo text\n", 10);
    close(fd);
    if (execve(BAD_PROGRAM, no_strings, no_strings) != -ENOEXEC || unlink(BAD_PROGRAM) != 0) {
        return 1;
    }

    // Addresses outside the memory of the program
    args[0] = (char *) KERNEL_START;
    args[1] = NULL;
    if (execve((const char *) 0, no_strings, no_strings) != -EFAULT ||
        execve(PROGRAM, (char **) 0x00800000, no_strings) != -EFAULT ||
        execve(PROGRAM, args, no_strings) != -EFAULT ||
        execve(PROGRAM, no_strings, args) != -EFAULT) {
        return 1;
    }

    // One argument more than the kernel takes, and an argument that is longer than the page for all of them
    for (int i = 0; i <= MAX_ARGS; ++i) {
        args[i] = "x";
    }
    args[MAX_ARGS + 1] = NULL;
    if (execve(PROGRAM, args, no_strings) != -E2BIG) {
        return 1;
    }

    args[0] = long_string;
    args[1] = NULL;

    return execve(PROGRAM, args, no_strings) == -E2BIG ? 0 : 1;
}

/*
    Starts `PROGRAM` in a child with the arguments `args` and the environment
    `vars`. Returns the exit code of the child, which is the number of
    arguments that the program has got, or -1.
*/
static int start_program(char *args[], char *vars[])
{
    int pid = fork();
    if (pid == 0) {
        execve(PROGRAM, args, vars);
        exit(-1);
    }

    int status;
    if (pid < 0 || waitpid(pid, &status, 0) != pid || STOP_REASON(status) != 0) {
        return -1;
    }

    return EXIT_CODE(status);
}

int main(void)
{
    check(start_with_bad_arguments() == 0, "the kernel refuses to start what it cannot start");
    check(start_bad_programs() == 0, "the kernel refuses programs with wrong headers");

    char *args[MAX_ARGS + 1] = { PROGRAM, "one", "", "three" };
    char *vars[] = { "HOME=/", "EMPTY=", NULL };
    check(start_program(args, vars) == 4, "a program gets its arguments and its environment");

    check(start_program(no_strings, no_strings) == 0, "a program may have no arguments");

    // No output this time: the exit code tells the number of arguments
    for (int i = 0; i < MAX_ARGS; ++i) {
        args[i] = "x";
    }
    args[MAX_ARGS] = NULL;

    int pid = fork();
    if (pid == 0) {
        close(STDOUT);
        execve(PROGRAM, args, no_strings);
        exit(-1);
    }

    int status = -1;
    check(pid > 0 && waitpid(pid, &status, 0) == pid && EXIT_CODE(status) == MAX_ARGS,
          "a program may have as many arguments as the kernel takes");

    return get_result();
}

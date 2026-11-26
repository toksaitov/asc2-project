/*
    ls: prints the size and the name of every file of the disk. With
    arguments it prints the lines of the files that they name.
*/

#include "user.h"

#define BUF_WORDS 128  // words of the buffer for the entries of the directory; the longest entry has 70
#define SIZE_WIDTH 8   // characters of the column with the sizes

// Prints the line of a file. Returns 0, or the error of `stat`.
static int print_file(const char *name)
{
    stat_t info;
    int result = stat(name, &info);
    if (result != 0) {
        return result;
    }

    // The sizes stand in a column, with their last digits under each other
    int width = 1;
    for (unsigned int rest = info.size; rest >= 10; rest /= 10) {
        ++width;
    }
    for (; width < SIZE_WIDTH; ++width) {
        print(STDOUT, " ");
    }

    print_number(STDOUT, info.size);
    print(STDOUT, " ");
    print(STDOUT, name);
    print(STDOUT, "\n");

    return 0;
}

// Prints the lines of all files of a directory. Returns 0, or a negative number of an error.
static int print_dir(const char *path)
{
    /*
        The kernel fills the buffer with entries of different lengths. An
        entry starts with 8-byte numbers, and the CPU reads a number only at
        an address that is a multiple of 4. An array of words starts at such
        an address, and an array of bytes may start anywhere.
    */
    unsigned int buf[BUF_WORDS];

    int fd = open(path, O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return fd;
    }

    int result;
    while ((result = getdents64(fd, buf, sizeof(buf))) > 0) {
        for (int pos = 0; pos < result;) {
            dirent64_t *entry = (dirent64_t *) ((char *) buf + pos);

            print_file(entry->name);
            pos += entry->reclen;
        }
    }

    close(fd);

    return result;
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        return print_dir("/") == 0 ? 0 : 1;
    }

    int status = 0;
    for (int i = 1; i < argc; ++i) {
        stat_t info;

        int result = stat(argv[i], &info);
        if (result == 0) {
            result = (info.mode & S_IFMT) == S_IFDIR ? print_dir(argv[i]) : print_file(argv[i]);
        }
        if (result != 0) {
            print(STDERR, "ls: cannot find ");
            print(STDERR, argv[i]);
            print(STDERR, "\n");
            status = 1;
        }
    }

    return status;
}

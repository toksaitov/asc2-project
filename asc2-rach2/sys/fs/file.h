/*
    The open files of a process.
*/

#ifndef _FILE_H_
#define _FILE_H_

#include "sys/lib/kint.h"

#define MAX_FILES 8 // open files of a process

#define PATH_SIZE 257 // bytes of the longest path with its zero byte: a `/` and a name

/* Flags of `open`, those of Linux */

#define O_RDONLY    00
#define O_WRONLY    01
#define O_RDWR      02
#define O_ACCMODE   03
#define O_CREAT     0100
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_DIRECTORY 0200000

/* Starting points of `lseek`, those of Linux */

#define SEEK_SET 0 // the start of the file
#define SEEK_CUR 1 // the position of the descriptor
#define SEEK_END 2 // the end of the file

typedef enum file_type
{
    FILE_NONE,    // the descriptor is not open
    FILE_CONSOLE,
    FILE_DISK,    // a file of the disk
    FILE_DIR,     // the directory of the disk
    FILE_GONE     // a file of the disk that has been removed since it was opened
} file_type_t;

/*
    What a file descriptor of a process stands for. A file of the disk is
    remembered by the position of its node. `file.c` corrects the position in
    every process when a file moves.
*/
typedef struct file
{
    file_type_t type;
    uint32_t flags;  // flags of `open`: the kind of access and `O_APPEND`
    uint32_t pos;    // position of the node on the disk
    uint32_t offset; // position of the next byte to read or write; in the directory, of the next node to list
} file_t;

struct process;

// Opens the console as the descriptors 0, 1, and 2 of a process without open files.
void init_files(file_t *files);

/*
    Finds what a path names. Returns 1 for the directory, 0 for a file, whose
    name `name` then points to, and a negative number of an error for a path
    that names nothing.
*/
int32_t parse_path(const char *path, const char **name);

/*
    The functions below are the system calls of the same names for the
    running process. An address is of the memory of its program. Each returns
    what the system call returns: a negative number of an error if it fails.
*/

int32_t file_open(const char *path, uint32_t flags, uint32_t mode);
int32_t file_close(uint32_t fd);
int32_t file_read(uint32_t fd, uint32_t buf, uint32_t count);
int32_t file_write(uint32_t fd, uint32_t buf, uint32_t count);
int32_t file_lseek(uint32_t fd, int32_t offset, uint32_t whence);
int32_t file_dup2(uint32_t old_fd, uint32_t new_fd);
int32_t file_stat(const char *path, uint32_t buf);
int32_t file_getdents64(uint32_t fd, uint32_t buf, uint32_t count);
int32_t file_unlink(const char *path);
int32_t file_rename(const char *old_path, const char *new_path);
int32_t file_touch(uint32_t fd);
int32_t file_chdir(const char *path);

#endif

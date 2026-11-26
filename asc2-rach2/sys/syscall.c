/*
    System calls.

    A program asks the kernel with `int 0x80`: the number of the call is in
    `r0`, and the arguments are in `r1`, `r2`, and so on. The result comes
    back in `r0`, and every other register keeps its value. The handler finds
    the registers of the program in the saved words of `isr_entry` and writes
    the result there.

    The numbers and the meanings of the calls are those of Linux. The calls
    for files are in `fs/file.c`, and those for processes in `proc/process.c`;
    this file takes the arguments of a call out of the memory of the program
    and gives them on.
*/

#include "sys/syscall.h"

#include <stddef.h>

#include "sys/isr.h"
#include "sys/machine.h"
#include "sys/pit.h"
#include "sys/screen.h"
#include "sys/fs/file.h"
#include "sys/mm/pages.h"
#include "sys/mm/paging.h"
#include "sys/proc/process.h"
#include "sys/proc/slice.h"

#define CREAT_FLAGS (O_CREAT | O_WRONLY | O_TRUNC) // `creat` is `open` with these flags

/*
    The times that `times` reports, in timer interrupts. The time that the
    kernel works for a process is not counted, and neither are the times of
    its children.
*/
typedef struct tms
{
    uint32_t utime;  // time that the process itself has run
    uint32_t stime;  // time that the kernel has worked for the process
    uint32_t cutime; // the two times of the children that have ended
    uint32_t cstime;
} tms_t;

/*
    Copies a path from the memory of the program into the `PATH_SIZE` bytes
    at `path`. Returns 0 or a negative number of an error.
*/
static int32_t get_path(char *path, uint32_t addr)
{
    int32_t len = copy_str_from_user(current_proc->page_dir, path, addr, PATH_SIZE);
    if (len == COPY_STR_TOO_LONG) {
        return -ENAMETOOLONG;
    }

    return len < 0 ? -EFAULT : 0;
}

/*
    int open(const char *path, int flags, int mode)

    Opens a file, or the directory of the disk. With `O_CREAT` in the flags, a
    file that is not there is made first, with the permissions of `mode`.
    Returns the new file descriptor.
*/
static int32_t sys_open(uint32_t path_addr, uint32_t flags, uint32_t mode)
{
    char path[PATH_SIZE];
    int32_t result = get_path(path, path_addr);
    if (result != 0) {
        return result;
    }

    return file_open(path, flags, mode);
}

/*
    int waitpid(int pid, int *status, int options)

    Waits for the end of a child: the one with the number `pid`, or any child
    if `pid` is -1. What the child has left is stored at `status` if it is
    not NULL. Returns the number of the child.
*/
static int32_t sys_waitpid(int32_t pid, uint32_t status_addr)
{
    int32_t status;
    int32_t child_pid = wait_process(pid, &status);
    if (child_pid < 0) {
        return child_pid;
    }

    if (status_addr != 0 && !copy_to_user(current_proc->page_dir, status_addr, &status, sizeof(status))) {
        return -EFAULT;
    }

    return child_pid;
}

/*
    Copies a list of strings of the program into the kernel, behind the
    strings that are at `strs` already. The list is an array of addresses
    that ends with a zero. `count` and `size` grow by the strings copied.
*/
static int32_t get_strs(uint32_t list_addr, uint32_t max_count, char *strs, uint32_t *count, uint32_t *size)
{
    uint32_t *page_dir = current_proc->page_dir;

    // A program may give no list at all
    if (list_addr == 0) {
        return 0;
    }

    for (;;) {
        uint32_t str_addr;
        if (!copy_from_user(page_dir, &str_addr, list_addr + *count * sizeof(uint32_t), sizeof(str_addr))) {
            return -EFAULT;
        }
        if (str_addr == 0) {
            return 0;
        }
        if (*count == max_count) {
            return -E2BIG;
        }

        int32_t len = copy_str_from_user(page_dir, strs + *size, str_addr, PAGE_SIZE - *size);
        if (len < 0) {
            return len == COPY_STR_TOO_LONG ? -E2BIG : -EFAULT;
        }

        *size += (uint32_t) len + 1;
        ++*count;
    }
}

/*
    int execve(const char *path, char *const argv[], char *const envp[])

    Gives the process another program: the file `path` of the disk, with the
    arguments `argv` and the environment `envp`. Does not return if all goes
    well: the new program starts instead.
*/
static int32_t sys_execve(uint32_t path_addr, uint32_t argv_addr, uint32_t envp_addr, isr_params_t *params)
{
    char path[PATH_SIZE];
    int32_t result = get_path(path, path_addr);
    if (result != 0) {
        return result;
    }

    // The strings must leave the old memory before it is freed. They wait in a page of the kernel
    char *strs = (char *) take_page();
    if (strs == NULL) {
        return -ENOMEM;
    }

    exec_args_t args;
    args.strs = strs;
    args.size = 0;
    args.argc = 0;
    args.envc = 0;

    result = get_strs(argv_addr, MAX_ARGS, strs, &args.argc, &args.size);
    if (result == 0) {
        // The list of the environment starts with its own count
        uint32_t envc = 0;
        result = get_strs(envp_addr, MAX_ENVS, strs, &envc, &args.size);
        args.envc = envc;
    }
    if (result == 0) {
        result = exec_process(path, &args, params);
    }

    free_page(strs);

    return result;
}

/*
    int unlink(const char *path)

    Removes a file.
*/
static int32_t sys_unlink(uint32_t path_addr)
{
    char path[PATH_SIZE];
    int32_t result = get_path(path, path_addr);
    if (result != 0) {
        return result;
    }

    return file_unlink(path);
}

/*
    int chdir(const char *path)

    Makes a directory the place where the names of files are looked for. The
    disk has one directory, so the call checks the path and changes nothing.
*/
static int32_t sys_chdir(uint32_t path_addr)
{
    char path[PATH_SIZE];
    int32_t result = get_path(path, path_addr);
    if (result != 0) {
        return result;
    }

    return file_chdir(path);
}

/*
    int rename(const char *old_path, const char *new_path)

    Gives a file a new name. A file that has the new name is removed.
*/
static int32_t sys_rename(uint32_t old_addr, uint32_t new_addr)
{
    char old_path[PATH_SIZE];
    char new_path[PATH_SIZE];
    int32_t result = get_path(old_path, old_addr);
    if (result == 0) {
        result = get_path(new_path, new_addr);
    }
    if (result != 0) {
        return result;
    }

    return file_rename(old_path, new_path);
}

/*
    int times(struct tms *buf)

    Returns the number of timer interrupts since the start of the machine. If
    `buf` is not NULL, the times of the process are stored there.
*/
static int32_t sys_times(uint32_t buf)
{
    if (buf != 0) {
        tms_t tms;
        tms.utime = current_proc->ticks;
        tms.stime = 0;
        tms.cutime = 0;
        tms.cstime = 0;

        if (!copy_to_user(current_proc->page_dir, buf, &tms, sizeof(tms))) {
            return -EFAULT;
        }
    }

    return (int32_t) get_ticks();
}

/*
    int slices(int request, unsigned int *words)

    Stores what the network of the scheduler has learned and how often it
    has guessed right at `words` (`SLICES_WORDS` words, `sys/proc/slice.h`).
    With `SLICES_FORGET` the network forgets first what it has learned.
*/
static int32_t sys_slices(uint32_t request, uint32_t words_addr)
{
    if (request != SLICES_READ && request != SLICES_FORGET) {
        return -EINVAL;
    }
    if (request == SLICES_FORGET) {
        forget_slices();
    }

    uint32_t words[SLICES_WORDS];
    get_slices(words);

    return copy_to_user(current_proc->page_dir, words_addr, words, sizeof(words)) ? 0 : -EFAULT;
}

/*
    int stat(const char *path, struct stat *buf)

    Tells the kind, the size, and the times of a file.
*/
static int32_t sys_stat(uint32_t path_addr, uint32_t buf)
{
    char path[PATH_SIZE];
    int32_t result = get_path(path, path_addr);
    if (result != 0) {
        return result;
    }

    return file_stat(path, buf);
}

/*
    int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags)

    Sets the times of a file. The kernel knows the one form that `touch`
    uses: the open file `dirfd`, no path, and no times, which means now.
*/
static int32_t sys_utimensat(uint32_t fd, uint32_t path_addr, uint32_t times_addr)
{
    if (path_addr != 0 || times_addr != 0) {
        return -EINVAL;
    }

    return file_touch(fd);
}

/*
    The calls without a function of their own above:

    void exit(int status)
        Ends the process. The parent gets the low byte of `status`.
    int fork(void)
        Makes a child: a copy of the process. Returns the number of the child
        in the parent, and 0 in the child.
    int read(int fd, void *buf, unsigned int count)
        Reads at most `count` bytes of an open file, or a typed line of the
        console. Returns the number of bytes, 0 at the end.
    int write(int fd, const void *buf, unsigned int count)
        Writes `count` bytes to an open file or prints them on the console.
        Returns the number of bytes.
    int close(int fd)
        Closes a file descriptor.
    int creat(const char *path, int mode)
        Opens a file for writing, empty: a new one, or one that loses its
        bytes. Returns the new file descriptor.
    int lseek(int fd, int offset, int whence)
        Moves the position of an open file. Returns the new position.
    int getpid(void)
        Returns the number of the process.
    int dup2(int old_fd, int new_fd)
        Makes `new_fd` stand for the open file of `old_fd`. Returns `new_fd`.
    int getdents64(int fd, void *buf, unsigned int count)
        Reads entries of the open directory. Returns the number of bytes, 0
        at the end.

    Three calls are not calls of Linux:

    int screen(int request, unsigned int a, unsigned int b)
        Takes the screen and the button of the RP2040 for the program, gives
        them back, or names a picture (`sys/screen.h`).
    int slices(int request, unsigned int *words)
        Tells what the network of the scheduler has learned and how often
        it has guessed right, or makes it forget and learn anew.
    int nap(unsigned int ticks)
        Lets the process sleep until so many more timer interrupts have
        come. Returns 0.
*/
static void syscall_handler(isr_params_t *params)
{
    uint32_t int_number = (params->cause >> CPU_CAUSE_INT_SHIFT) & 0xff;
    if (int_number != SYSCALL_INT) {
        fail_isr(params);
    }

    uint32_t a = params->r1;
    uint32_t b = params->r2;
    uint32_t c = params->r3;

    ++current_proc->turn_calls;

    int32_t result;
    switch (params->r0) {
        case SYS_EXIT:
            exit_process(STATUS_OF_EXIT(a));
            return;
        case SYS_FORK:
            result = fork_process(params);
            break;
        case SYS_READ:
            result = file_read(a, b, c);
            break;
        case SYS_WRITE:
            result = file_write(a, b, c);
            break;
        case SYS_OPEN:
            result = sys_open(a, b, c);
            break;
        case SYS_CLOSE:
            result = file_close(a);
            break;
        case SYS_WAITPID:
            result = sys_waitpid((int32_t) a, b);
            break;
        case SYS_CREAT:
            result = sys_open(a, CREAT_FLAGS, b);
            break;
        case SYS_UNLINK:
            result = sys_unlink(a);
            break;
        case SYS_EXECVE:
            result = sys_execve(a, b, c, params);

            // The registers are those of the new program already
            if (result == 0) {
                return;
            }
            break;
        case SYS_CHDIR:
            result = sys_chdir(a);
            break;
        case SYS_LSEEK:
            result = file_lseek(a, (int32_t) b, c);
            break;
        case SYS_GETPID:
            result = (int32_t) current_proc->pid;
            break;
        case SYS_RENAME:
            result = sys_rename(a, b);
            break;
        case SYS_TIMES:
            result = sys_times(a);
            break;
        case SYS_DUP2:
            result = file_dup2(a, b);
            break;
        case SYS_STAT:
            result = sys_stat(a, b);
            break;
        case SYS_GETDENTS64:
            result = file_getdents64(a, b, c);
            break;
        case SYS_UTIMENSAT:
            result = sys_utimensat(a, b, c);
            break;
        case SYS_SCREEN:
            result = screen_call(a, b, c);
            break;
        case SYS_SLICES:
            result = sys_slices(a, b);
            break;
        case SYS_NAP:
            nap(a);
            result = 0;
            break;
        default:
            result = -ENOSYS;
            break;
    }

    params->r0 = (uint32_t) result;
}

void init_syscall(void)
{
    register_isr_handler(CPU_EXCPT_INT, syscall_handler);
}

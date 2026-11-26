/*
    The open files of a process.

    A program names an open file by a small number, the file descriptor: an
    index into the table of open files of its process. An entry of the table
    is the console, a file of the disk, or the directory of the disk, and it
    has the position at which the next read or write goes on. The descriptors
    0, 1, and 2 of the first process are the console, and `fork` gives a
    child a copy of the whole table.

    The disk has one directory, the root, and no other. A path may start
    with `/`; the paths `/` and `.` name the directory, and any other path is
    the name of a file.

    A file of the disk is remembered by the position of its node, and a file
    may move when it grows. The table of every process is corrected then.
    When a file is removed, the descriptors that have it open are of no use
    any more; Linux would keep the file until the last of them is closed.
*/

#include "sys/fs/file.h"

#include <stdbool.h>
#include <stddef.h>

#include "sys/console.h"
#include "sys/rp.h"
#include "sys/syscall.h"
#include "sys/fs/krffs.h"
#include "sys/lib/kstring.h"
#include "sys/mm/paging.h"
#include "sys/proc/process.h"

#define DIRENT_TYPE_FILE 8 // type of a regular file in a directory entry of Linux

/*
    What `stat` tells about a file: the structure of Linux for the i386. The
    kernel fills the mode, the size, and the times.
*/
typedef struct stat
{
    uint32_t dev;
    uint32_t ino;        // a number that no other file has: the position of the node
    uint16_t mode;       // the kind of the file and its permissions
    uint16_t nlink;      // number of names of the file
    uint16_t uid;
    uint16_t gid;
    uint32_t rdev;
    uint32_t size;       // bytes of data
    uint32_t blksize;
    uint32_t blocks;
    uint32_t atime;      // the times, in seconds, each followed by its nanoseconds
    uint32_t atime_nsec;
    uint32_t mtime;
    uint32_t mtime_nsec;
    uint32_t ctime;
    uint32_t ctime_nsec;
    uint32_t unused[2];
} stat_t;

/*
    An entry of a directory as `getdents64` of Linux gives it. Entries follow
    each other in the buffer of the program, and `reclen` leads from one to
    the next.
*/
typedef struct dirent64
{
    uint64_t ino;    // a number that no other file has: the position of the node
    int64_t off;     // where the listing goes on after this entry
    uint16_t reclen; // bytes of this entry
    uint8_t type;
    char name[KRFFS_NAME_SIZE];
} dirent64_t;

#define DIRENT_NAME_OFF 19 // position of the name in an entry

void init_files(file_t *files)
{
    memset(files, 0, MAX_FILES * sizeof(file_t));

    for (uint32_t fd = 0; fd <= 2; ++fd) {
        files[fd].type = FILE_CONSOLE;
        files[fd].flags = O_RDWR;
    }
}

int32_t parse_path(const char *path, const char **name)
{
    if (path[0] == '\0') {
        return -ENOENT;
    }

    while (*path == '/') {
        ++path;
    }
    if (path[0] == '\0' || (path[0] == '.' && path[1] == '\0')) {
        return 1;
    }

    // The root is the only directory, so a name has no `/`
    for (const char *ch = path; *ch != '\0'; ++ch) {
        if (*ch == '/') {
            return -ENOENT;
        }
    }
    if (strlen(path) >= KRFFS_NAME_SIZE) {
        return -ENAMETOOLONG;
    }

    *name = path;

    return 0;
}

// Returns the open file of a descriptor of the running process, or NULL.
static file_t *get_file(uint32_t fd)
{
    if (fd >= MAX_FILES || current_proc->files[fd].type == FILE_NONE) {
        return NULL;
    }

    return &current_proc->files[fd];
}

// Tells every process that the file at `old_pos` is at `new_pos` now.
static void move_files(uint32_t old_pos, uint32_t new_pos)
{
    for (uint32_t i = 0; i < MAX_PROCS; ++i) {
        for (uint32_t fd = 0; fd < MAX_FILES; ++fd) {
            file_t *file = &proc_table[i].files[fd];
            if (file->type == FILE_DISK && file->pos == old_pos) {
                file->pos = new_pos;
            }
        }
    }
}

// Tells every process that the file at `pos` has been removed.
static void forget_files(uint32_t pos)
{
    for (uint32_t i = 0; i < MAX_PROCS; ++i) {
        for (uint32_t fd = 0; fd < MAX_FILES; ++fd) {
            file_t *file = &proc_table[i].files[fd];
            if (file->type == FILE_DISK && file->pos == pos) {
                file->type = FILE_GONE;
            }
        }
    }
}

/*
    Ends a visit of the file system. Returns `result`, or `-EIO` if the
    changes have not reached the disk.
*/
static int32_t leave(int32_t result)
{
    if (!leave_krffs() && result >= 0) {
        return -EIO;
    }

    return result;
}

int32_t file_open(const char *path, uint32_t flags, uint32_t mode)
{
    const char *name;
    int32_t kind = parse_path(path, &name);
    if (kind < 0) {
        return kind;
    }

    uint32_t access = flags & O_ACCMODE;
    if (access == O_ACCMODE) {
        return -EINVAL;
    }

    // The new descriptor is the lowest free one, as programs expect
    uint32_t fd = 0;
    while (fd < MAX_FILES && current_proc->files[fd].type != FILE_NONE) {
        ++fd;
    }
    if (fd == MAX_FILES) {
        return -EMFILE;
    }
    file_t *file = &current_proc->files[fd];

    if (kind == 1) {
        if (access != O_RDONLY) {
            return -EISDIR;
        }

        file->type = FILE_DIR;
        file->flags = flags;
        file->pos = 0;
        file->offset = 0;

        return (int32_t) fd;
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = krffs_find(name, &node);
    if (result == -ENOENT && (flags & O_CREAT)) {
        result = krffs_create(name, mode, &node);
    } else if (result == 0 && (flags & O_DIRECTORY)) {
        result = -ENOTDIR;
    } else if (result == 0 && (flags & O_TRUNC) && access != O_RDONLY) {
        result = krffs_resize(&node, 0);
    }

    result = leave(result);
    if (result != 0) {
        return result;
    }

    file->type = FILE_DISK;
    file->flags = flags;
    file->pos = node.pos;
    file->offset = 0;

    return (int32_t) fd;
}

int32_t file_close(uint32_t fd)
{
    file_t *file = get_file(fd);
    if (file == NULL) {
        return -EBADF;
    }

    file->type = FILE_NONE;

    return 0;
}

int32_t file_read(uint32_t fd, uint32_t buf, uint32_t count)
{
    file_t *file = get_file(fd);
    if (file == NULL || (file->flags & O_ACCMODE) == O_WRONLY) {
        return -EBADF;
    }
    if (file->type == FILE_CONSOLE) {
        return read_console(current_proc->page_dir, buf, count);
    }
    if (file->type == FILE_DIR) {
        return -EISDIR;
    }

    enter_krffs();

    // The file may have moved or gone while the process waited for its turn
    krffs_node_t node;
    int32_t result = file->type == FILE_DISK ? krffs_get_node(file->pos, &node) : -EBADF;

    // The bytes go from the disk straight into the pages of the program, one page at a time
    uint32_t done = 0;
    while (result == 0 && done < count) {
        uint32_t size = count - done;
        void *bytes = get_user_bytes(current_proc->page_dir, buf + done, true, &size);
        if (bytes == NULL) {
            result = -EFAULT;
            break;
        }

        int32_t part = krffs_read(&node, file->offset, bytes, size);
        if (part <= 0) {
            result = part;
            break;
        }

        done += (uint32_t) part;
        file->offset += (uint32_t) part;
    }

    result = leave(result);

    // As Linux does, the call reports the bytes that it has read before a failure, if there are any
    return done > 0 ? (int32_t) done : result;
}

static int32_t write_console(uint32_t buf, uint32_t count)
{
    uint32_t done = 0;
    while (done < count) {
        uint32_t size = count - done;
        const char *bytes = (const char *) get_user_bytes(current_proc->page_dir, buf + done, false, &size);
        if (bytes == NULL) {
            return done > 0 ? (int32_t) done : -EFAULT;
        }

        for (uint32_t i = 0; i < size; ++i) {
            console_putc(bytes[i]);
        }

        done += size;
    }

    return (int32_t) done;
}

int32_t file_write(uint32_t fd, uint32_t buf, uint32_t count)
{
    file_t *file = get_file(fd);
    if (file == NULL || (file->flags & O_ACCMODE) == O_RDONLY) {
        return -EBADF;
    }
    if (file->type == FILE_CONSOLE) {
        return write_console(buf, count);
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = file->type == FILE_DISK ? krffs_get_node(file->pos, &node) : -EBADF;
    if (result == 0 && (file->flags & O_APPEND)) {
        file->offset = node.data_size;
    }

    uint32_t done = 0;
    while (result == 0 && done < count) {
        uint32_t size = count - done;
        const void *bytes = get_user_bytes(current_proc->page_dir, buf + done, false, &size);
        if (bytes == NULL) {
            result = -EFAULT;
            break;
        }

        uint32_t old_pos = node.pos;
        int32_t part = krffs_write(&node, file->offset, bytes, size);
        if (node.pos != old_pos) {
            move_files(old_pos, node.pos);
        }
        if (part < 0) {
            result = part;
            break;
        }

        done += (uint32_t) part;
        file->offset += (uint32_t) part;
    }

    result = leave(result);

    return done > 0 && result != -EIO ? (int32_t) done : result;
}

int32_t file_lseek(uint32_t fd, int32_t offset, uint32_t whence)
{
    file_t *file = get_file(fd);
    if (file == NULL) {
        return -EBADF;
    }
    if (file->type == FILE_CONSOLE) {
        return -ESPIPE;
    }
    if (whence > SEEK_END) {
        return -EINVAL;
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = file->type == FILE_DISK ? krffs_get_node(file->pos, &node) : -EBADF;
    if (result == 0) {
        int32_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? (int32_t) file->offset : (int32_t) node.data_size;

        result = base + offset;
        if (result < 0) {
            result = -EINVAL;
        } else {
            file->offset = (uint32_t) result;
        }
    }

    return leave(result);
}

int32_t file_dup2(uint32_t old_fd, uint32_t new_fd)
{
    file_t *file = get_file(old_fd);
    if (file == NULL || new_fd >= MAX_FILES) {
        return -EBADF;
    }

    // The new descriptor is closed first if it is open, and then stands for the same file at the same position
    if (new_fd != old_fd) {
        current_proc->files[new_fd] = *file;
    }

    return (int32_t) new_fd;
}

int32_t file_stat(const char *path, uint32_t buf)
{
    const char *name;
    int32_t kind = parse_path(path, &name);
    if (kind < 0) {
        return kind;
    }

    stat_t stat;
    memset(&stat, 0, sizeof(stat));
    stat.nlink = 1;

    if (kind == 1) {
        stat.mode = KRFFS_MODE_DIR | KRFFS_ROOT_PERMISSIONS;
    } else {
        enter_krffs();

        krffs_node_t node;
        uint32_t mode;
        uint32_t time;
        int32_t result = krffs_find(name, &node);
        if (result == 0) {
            result = krffs_get_info(&node, &mode, &time);
        }

        result = leave(result);
        if (result != 0) {
            return result;
        }

        stat.ino = node.pos;
        stat.mode = (uint16_t) mode;
        stat.size = node.data_size;
        stat.atime = time;
        stat.mtime = time;
        stat.ctime = time;
    }

    return copy_to_user(current_proc->page_dir, buf, &stat, sizeof(stat)) ? 0 : -EFAULT;
}

int32_t file_getdents64(uint32_t fd, uint32_t buf, uint32_t count)
{
    file_t *file = get_file(fd);
    if (file == NULL) {
        return -EBADF;
    }
    if (file->type != FILE_DIR) {
        return -ENOTDIR;
    }

    enter_krffs();

    int32_t result = 0;
    uint32_t done = 0;
    for (;;) {
        krffs_node_t node;
        uint32_t pos = file->offset;
        result = krffs_next(&pos, &node);
        if (result <= 0) {
            break;
        }

        dirent64_t entry;
        memset(&entry, 0, sizeof(entry));
        result = krffs_get_name(&node, entry.name);
        if (result != 0) {
            break;
        }

        // An entry ends at a multiple of 8, so that the numbers of the next one are in their places
        uint32_t size = (DIRENT_NAME_OFF + strlen(entry.name) + 1 + 7) & ~7u;
        if (size > count - done) {
            // The entries that do not fit wait for the next call; a buffer that holds none is too small
            result = done > 0 ? 0 : -EINVAL;
            break;
        }

        entry.ino = node.pos;
        entry.off = pos;
        entry.reclen = (uint16_t) size;
        entry.type = DIRENT_TYPE_FILE;
        if (!copy_to_user(current_proc->page_dir, buf + done, &entry, size)) {
            result = -EFAULT;
            break;
        }

        done += size;
        file->offset = pos;
    }

    result = leave(result);

    return done > 0 ? (int32_t) done : result;
}

int32_t file_unlink(const char *path)
{
    const char *name;
    int32_t kind = parse_path(path, &name);
    if (kind != 0) {
        return kind < 0 ? kind : -EISDIR;
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = krffs_find(name, &node);
    if (result == 0) {
        result = krffs_remove(&node);
    }
    if (result == 0) {
        forget_files(node.pos);
    }

    return leave(result);
}

int32_t file_rename(const char *old_path, const char *new_path)
{
    const char *old_name;
    const char *new_name;
    int32_t old_kind = parse_path(old_path, &old_name);
    int32_t new_kind = parse_path(new_path, &new_name);
    if (old_kind != 0) {
        return old_kind < 0 ? old_kind : -EINVAL;
    }
    if (new_kind != 0) {
        return new_kind < 0 ? new_kind : -EISDIR;
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = krffs_find(old_name, &node);
    if (result == 0 && strcmp(old_name, new_name) != 0) {
        // A file that has the new name already gives way, as on Linux
        krffs_node_t other;
        result = krffs_find(new_name, &other);
        if (result == 0) {
            result = krffs_remove(&other);
            if (result == 0) {
                forget_files(other.pos);

                // The free space that the other file has left may have changed the sizes around this one
                result = krffs_get_node(node.pos, &node);
            }
        } else if (result == -ENOENT) {
            result = 0;
        }

        if (result == 0) {
            result = krffs_rename(&node, new_name);
        }
    }

    return leave(result);
}

int32_t file_touch(uint32_t fd)
{
    file_t *file = get_file(fd);
    if (file == NULL) {
        return -EBADF;
    }
    if (file->type == FILE_CONSOLE || file->type == FILE_DIR) {
        return 0;
    }

    enter_krffs();

    krffs_node_t node;
    int32_t result = file->type == FILE_DISK ? krffs_get_node(file->pos, &node) : -EBADF;
    if (result == 0) {
        result = krffs_set_time(&node, get_uptime());
    }

    return leave(result);
}

int32_t file_chdir(const char *path)
{
    const char *name;
    int32_t kind = parse_path(path, &name);
    if (kind != 0) {
        return kind < 0 ? kind : 0;
    }

    enter_krffs();

    // A name of a file is not a directory, and there is no other directory than the root
    krffs_node_t node;
    int32_t result = krffs_find(name, &node);
    if (result == 0) {
        result = -ENOTDIR;
    }

    return leave(result);
}

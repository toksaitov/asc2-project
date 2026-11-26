/*
    A program that checks the files: the system calls for them, and through
    them the file system and the disk.

    The bytes of a test file are a series that never repeats in step with a
    sector of the disk, so a sector that lands in a wrong place shows.
*/

#include "user.h"

#include "check.h"

#define FILE_A "test_a"
#define FILE_B "test_b"
#define FILE_C "test_c"
#define FULL_FILE "test_full" // the file that takes all the room

#define MODE 0644

#define BUF_SIZE 4096
#define LONG_SIZE 10000    // bytes of a file that lies on three sectors
#define SMALL_SIZE 100
#define WRITE_PART 777     // the sizes of the pieces in which a file is written and read
#define READ_PART 1000
#define MAX_FILES 8        // file descriptors of a process
#define NAME_SIZE 256      // bytes of the longest name with its zero byte
#define DIRENT_WORDS 128   // words of a buffer for entries of the directory

// The buffer is made of words, so that a number can be written at its start
static unsigned int buf_words[BUF_SIZE / 4];
static unsigned char *const buf = (unsigned char *) buf_words;

// Returns the byte that belongs at a position of a file with the series that starts at `seed`.
static unsigned char get_byte(unsigned int seed, unsigned int pos)
{
    return (unsigned char) (seed + pos * 7 + (pos >> 8) * 13);
}

/*
    Writes `size` bytes of a series to an open file, from the position `pos`
    of the series, in pieces of `part` bytes. Returns the number of bytes
    that the kernel has taken.
*/
static int write_series(int fd, unsigned int seed, int pos, int size, int part)
{
    int done = 0;
    while (done < size) {
        int count = size - done < part ? size - done : part;
        for (int i = 0; i < count; ++i) {
            buf[i] = get_byte(seed, pos + done + i);
        }

        int written = write(fd, buf, count);
        if (written > 0) {
            done += written;
        }
        if (written != count) {
            break;
        }
    }

    return done;
}

/*
    Reads `size` bytes of an open file in pieces of `part` bytes and compares
    them with the series from the position `pos`. Returns 0 if all are there
    and right.
*/
static int read_series(int fd, unsigned int seed, int pos, int size, int part)
{
    int done = 0;
    while (done < size) {
        int count = size - done < part ? size - done : part;

        int got = read(fd, buf, count);
        if (got != count) {
            return 1;
        }
        for (int i = 0; i < count; ++i) {
            if (buf[i] != get_byte(seed, pos + done + i)) {
                return 1;
            }
        }

        done += count;
    }

    return 0;
}

/*
    Makes a file with `size` bytes of a series. Returns 0 if it was made.
*/
static int make_file(const char *name, unsigned int seed, int size)
{
    int fd = creat(name, MODE);
    if (fd < 0) {
        return 1;
    }

    int written = write_series(fd, seed, 0, size, WRITE_PART);

    return close(fd) == 0 && written == size ? 0 : 1;
}

/*
    Returns 0 if a file has exactly `size` bytes of a series.
*/
static int check_file(const char *name, unsigned int seed, int size)
{
    stat_t info;
    if (stat(name, &info) != 0 || (int) info.size != size) {
        return 1;
    }

    int fd = open(name, O_RDONLY, 0);
    if (fd < 0) {
        return 1;
    }

    // After the last byte a read gives nothing
    int result = read_series(fd, seed, 0, size, READ_PART);
    if (read(fd, buf, 1) != 0) {
        result = 1;
    }

    return close(fd) == 0 ? result : 1;
}

static int check_positions(void)
{
    int fd = open(FILE_A, O_RDWR, 0);
    if (fd < 0) {
        return 1;
    }

    int failures = 0;

    // From the start, from the position, and from the end
    failures += lseek(fd, 4090, SEEK_SET) != 4090 || read_series(fd, 1, 4090, 20, 20) != 0;
    failures += lseek(fd, -10, SEEK_CUR) != 4100 || read_series(fd, 1, 4100, 5000, READ_PART) != 0;
    failures += lseek(fd, -3, SEEK_END) != LONG_SIZE - 3 || read_series(fd, 1, LONG_SIZE - 3, 3, 3) != 0;
    failures += lseek(fd, 0, SEEK_CUR) != LONG_SIZE;

    // No position before the start, and no fourth starting point
    failures += lseek(fd, -1, SEEK_SET) != -EINVAL || lseek(fd, 0, 3) != -EINVAL;
    failures += lseek(fd, 0, SEEK_CUR) != LONG_SIZE;

    // The console has no positions
    failures += lseek(STDOUT, 0, SEEK_SET) != -ESPIPE;

    // Bytes in the middle are replaced, and the file keeps its size
    failures += lseek(fd, 5000, SEEK_SET) != 5000 || write_series(fd, 2, 5000, 300, 300) != 300;
    failures += lseek(fd, 4000, SEEK_SET) != 4000 || read_series(fd, 1, 4000, 1000, READ_PART) != 0 ||
                read_series(fd, 2, 5000, 300, 300) != 0 || read_series(fd, 1, 5300, LONG_SIZE - 5300, READ_PART) != 0;

    // A write behind the end leaves a gap of zeros
    failures += lseek(fd, 50, SEEK_END) != LONG_SIZE + 50 || write_series(fd, 3, 0, 10, 10) != 10;
    failures += lseek(fd, LONG_SIZE, SEEK_SET) != LONG_SIZE || read(fd, buf, 50) != 50;
    for (int i = 0; i < 50; ++i) {
        failures += buf[i] != 0;
    }
    failures += read_series(fd, 3, 0, 10, 10) != 0 || read(fd, buf, 1) != 0;

    failures += close(fd) != 0;

    return failures;
}

static int check_open_flags(void)
{
    int failures = 0;

    // `O_APPEND` sends every write to the end, wherever the position is
    int fd = open(FILE_B, O_WRONLY | O_APPEND, 0);
    failures += fd < 0 || lseek(fd, 0, SEEK_SET) != 0 || write_series(fd, 4, SMALL_SIZE, 50, 50) != 50;
    failures += close(fd) != 0 || check_file(FILE_B, 4, SMALL_SIZE + 50) != 0;

    // `O_CREAT` leaves a file that is there as it is, and `O_TRUNC` empties it
    fd = open(FILE_B, O_RDWR | O_CREAT, MODE);
    failures += fd < 0 || close(fd) != 0 || check_file(FILE_B, 4, SMALL_SIZE + 50) != 0;

    fd = open(FILE_B, O_RDWR | O_TRUNC, 0);
    failures += fd < 0 || close(fd) != 0 || check_file(FILE_B, 4, 0) != 0;

    // An empty file can be written again
    failures += make_file(FILE_B, 4, SMALL_SIZE) != 0 || check_file(FILE_B, 4, SMALL_SIZE) != 0;

    // A descriptor does only what it was opened for
    fd = open(FILE_B, O_RDONLY, 0);
    failures += write(fd, buf, 1) != -EBADF || close(fd) != 0;
    fd = open(FILE_B, O_WRONLY, 0);
    failures += read(fd, buf, 1) != -EBADF || close(fd) != 0;

    return failures;
}

static int check_refusals(void)
{
    static char long_name[NAME_SIZE + 1];

    int failures = 0;

    // The longest name is one byte shorter than this one
    for (int i = 0; i < NAME_SIZE; ++i) {
        long_name[i] = 'n';
    }

    failures += open("nothing", O_RDONLY, 0) != -ENOENT;
    failures += open("", O_RDONLY, 0) != -ENOENT;
    failures += open("dir/name", O_RDONLY | O_CREAT, MODE) != -ENOENT;
    failures += open(long_name, O_RDONLY | O_CREAT, MODE) != -ENAMETOOLONG;
    failures += open(FILE_B, O_WRONLY | O_RDWR, 0) != -EINVAL;
    failures += open(FILE_B, O_RDONLY | O_DIRECTORY, 0) != -ENOTDIR;
    failures += open("/", O_WRONLY, 0) != -EISDIR;
    failures += open((const char *) 0x80001000, O_RDONLY, 0) != -EFAULT;

    // The name with one byte less is a name
    long_name[NAME_SIZE - 1] = '\0';
    int fd = creat(long_name, MODE);
    failures += fd < 0 || close(fd) != 0 || unlink(long_name) != 0;

    // Descriptors that are not open
    failures += close(7) != -EBADF || close(MAX_FILES) != -EBADF || close(-1) != -EBADF;
    failures += read(7, buf, 1) != -EBADF || write(MAX_FILES, buf, 1) != -EBADF || lseek(7, 0, SEEK_SET) != -EBADF;

    // A buffer outside the memory of the program
    fd = open(FILE_B, O_RDWR, 0);
    failures += read(fd, (void *) 0x80001000, 10) != -EFAULT || write(fd, (void *) 0x00800000, 10) != -EFAULT;
    failures += close(fd) != 0 || check_file(FILE_B, 4, SMALL_SIZE) != 0;

    // A process has `MAX_FILES` descriptors, and three of them are the console
    int fds[MAX_FILES];
    int count = 0;
    while (count < MAX_FILES && (fds[count] = open(FILE_B, O_RDONLY, 0)) >= 0) {
        ++count;
    }
    failures += count != MAX_FILES - 3 || open(FILE_B, O_RDONLY, 0) != -EMFILE;
    for (int i = 0; i < count; ++i) {
        failures += fds[i] != 3 + i || close(fds[i]) != 0;
    }

    return failures;
}

static int check_dup2(void)
{
    int failures = 0;

    int fd = open(FILE_B, O_RDONLY, 0);
    failures += fd != 3 || lseek(fd, 10, SEEK_SET) != 10;

    // The new descriptor reads on where the old one was
    failures += dup2(fd, 6) != 6 || read_series(6, 4, 10, 20, 20) != 0;

    // A descriptor that is open is closed first, and a descriptor may be given itself
    int other_fd = open(FILE_A, O_RDONLY, 0);
    failures += other_fd != 4 || dup2(fd, other_fd) != other_fd || read_series(other_fd, 4, 10, 20, 20) != 0;
    failures += dup2(fd, fd) != fd || read_series(fd, 4, 10, 20, 20) != 0;

    failures += dup2(7, 5) != -EBADF || dup2(fd, MAX_FILES) != -EBADF;
    failures += close(fd) != 0 || close(other_fd) != 0 || close(6) != 0;

    // A child has the open files of its parent
    fd = open(FILE_B, O_RDONLY, 0);
    failures += lseek(fd, 30, SEEK_SET) != 30;

    int pid = fork();
    if (pid == 0) {
        exit(read_series(fd, 4, 30, 20, 20));
    }

    int status = -1;
    failures += pid < 0 || waitpid(pid, &status, 0) != pid || status != 0;
    failures += close(fd) != 0;

    return failures;
}

/*
    Makes a file grow that has another file behind it. It cannot grow where
    it is, so the file system moves it. Two descriptors that were open before
    the move must find the file at its new place.
*/
static int move_file(void)
{
    int failures = 0;

    failures += make_file(FILE_C, 5, SMALL_SIZE) != 0;
    failures += make_file("test_d", 6, SMALL_SIZE) != 0;

    int read_fd = open(FILE_C, O_RDONLY, 0);
    int write_fd = open(FILE_C, O_WRONLY | O_APPEND, 0);
    failures += read_fd < 0 || write_fd < 0;

    failures += write_series(write_fd, 5, SMALL_SIZE, 5000, WRITE_PART) != 5000;
    failures += read_series(read_fd, 5, 0, SMALL_SIZE + 5000, READ_PART) != 0;
    failures += write_series(write_fd, 5, SMALL_SIZE + 5000, 10, 10) != 10;
    failures += read_series(read_fd, 5, SMALL_SIZE + 5000, 10, 10) != 0;
    failures += close(read_fd) != 0 || close(write_fd) != 0;

    failures += check_file(FILE_C, 5, SMALL_SIZE + 5010) != 0;
    failures += check_file("test_d", 6, SMALL_SIZE) != 0;
    failures += unlink("test_d") != 0;

    return failures;
}

static int check_names(void)
{
    int failures = 0;

    // A new name: the file is the same, and the old name is free
    failures += rename(FILE_C, "test_e") != 0 || check_file("test_e", 5, SMALL_SIZE + 5010) != 0;
    failures += open(FILE_C, O_RDONLY, 0) != -ENOENT;

    // The name of another file: that file is removed
    failures += rename("test_e", FILE_B) != 0 || check_file(FILE_B, 5, SMALL_SIZE + 5010) != 0;
    failures += open("test_e", O_RDONLY, 0) != -ENOENT;

    // Its own name, and names that cannot be
    failures += rename(FILE_B, FILE_B) != 0 || check_file(FILE_B, 5, SMALL_SIZE + 5010) != 0;
    failures += rename("nothing", "test_e") != -ENOENT || rename(FILE_B, "dir/name") != -ENOENT;
    failures += rename(FILE_B, "/") != -EISDIR || rename("/", "test_e") != -EINVAL;

    // A removed file is gone, and a descriptor that had it open is of no use
    int fd = open(FILE_B, O_RDONLY, 0);
    failures += unlink(FILE_B) != 0 || unlink(FILE_B) != -ENOENT || unlink("/") != -EISDIR;
    failures += read(fd, buf, 1) != -EBADF || close(fd) != 0;
    failures += open(FILE_B, O_RDONLY, 0) != -ENOENT;

    return failures;
}

/*
    Counts the files of the directory, with a buffer of `size` bytes for the
    entries. `count_of_tests` gets the number of files whose names start
    with `test_`. Returns the number of files, or a negative number of an
    error.
*/
static int count_files(int size, int *count_of_tests)
{
    // An entry starts with 8-byte numbers, so the buffer is an array of words: see `utils/ls.c`
    unsigned int entries[DIRENT_WORDS];

    int fd = open("/", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return fd;
    }

    int count = 0;
    *count_of_tests = 0;

    int result;
    while ((result = getdents64(fd, entries, size)) > 0) {
        for (int pos = 0; pos < result;) {
            dirent64_t *entry = (dirent64_t *) ((char *) entries + pos);

            const char *prefix = "test_";
            int i = 0;
            while (prefix[i] != '\0' && entry->name[i] == prefix[i]) {
                ++i;
            }
            if (prefix[i] == '\0') {
                ++*count_of_tests;
            }

            ++count;
            pos += entry->reclen;
        }
    }

    close(fd);

    return result < 0 ? result : count;
}

static int check_directory(void)
{
    int failures = 0;

    // The directory is a directory, and a file is a file with its size and its permissions
    stat_t info;
    failures += stat("/", &info) != 0 || (info.mode & S_IFMT) != S_IFDIR;
    failures += stat(".", &info) != 0 || (info.mode & S_IFMT) != S_IFDIR;
    failures += stat(FILE_A, &info) != 0 || info.mode != (S_IFREG | MODE) || info.size != LONG_SIZE + 60 || info.nlink != 1;
    failures += stat("nothing", &info) != -ENOENT || stat(FILE_A, (stat_t *) 0x80001000) != -EFAULT;
    failures += chdir("/") != 0 || chdir(FILE_A) != -ENOTDIR || chdir("nothing") != -ENOENT;

    // The listing has the one test file that is left, whatever the size of the buffer
    int tests;
    int count = count_files(sizeof(unsigned int) * DIRENT_WORDS, &tests);
    failures += count < 2 || tests != 1;

    // A buffer for one entry at a time, and a buffer that is too small for an entry
    int other_tests;
    failures += count_files(40, &other_tests) != count || other_tests != 1;
    failures += count_files(16, &other_tests) != -EINVAL;

    // The directory is read with `getdents64` only, and a file is not a directory
    int fd = open("/", O_RDONLY, 0);
    failures += fd < 0 || read(fd, buf, 1) != -EISDIR || close(fd) != 0;
    fd = open(FILE_A, O_RDONLY, 0);
    failures += getdents64(fd, buf, BUF_SIZE) != -ENOTDIR || close(fd) != 0;

    // A touch sets the time and leaves the bytes; the console has no time, and a touch of it does nothing
    fd = open(FILE_A, O_WRONLY, 0);
    failures += utimensat(fd, NULL, NULL, 0) != 0 || utimensat(fd, FILE_A, NULL, 0) != -EINVAL;
    failures += utimensat(7, NULL, NULL, 0) != -EBADF || utimensat(STDOUT, NULL, NULL, 0) != 0;
    failures += close(fd) != 0;

    stat_t new_info;
    failures += stat(FILE_A, &new_info) != 0 || new_info.mtime < info.mtime || new_info.size != info.size;

    return failures;
}

/*
    Two processes write a file each at the same time. One process at a time
    works in the file system, and a write to the disk takes long, so each of
    them waits for the other again and again.
*/
static int write_together(void)
{
    int pid = fork();
    if (pid == 0) {
        exit(make_file("test_child", 7, LONG_SIZE) != 0 || check_file("test_child", 7, LONG_SIZE) != 0);
    }

    int failures = make_file("test_parent", 8, LONG_SIZE) != 0 || check_file("test_parent", 8, LONG_SIZE) != 0;

    int status = -1;
    failures += pid < 0 || waitpid(pid, &status, 0) != pid || status != 0;

    failures += check_file("test_child", 7, LONG_SIZE) != 0 || check_file("test_parent", 8, LONG_SIZE) != 0;
    failures += unlink("test_child") != 0 || unlink("test_parent") != 0;

    return failures;
}

/*
    Writes a file until the disk has no room for it: first whole buffers,
    and then smaller and smaller pieces, down to one byte. A write that does
    not fit takes the bytes that fit, or fails with `ENOSPC` if none does.
    The bytes of a file are in one piece on the disk, so the file ends up
    with the largest free piece, and smaller free pieces may be left.

    The file is large, and a series for all of it would take the machine
    minutes. Every piece starts with a word that tells where in the file the
    piece is, and has the same bytes after that. Returns the number of bytes
    that the file has got, or -1 if anything but the lack of room has ended a
    write.
*/
static int fill_disk(void)
{
    int fd = creat(FULL_FILE, MODE);
    if (fd < 0) {
        return -1;
    }

    for (int i = 0; i < BUF_SIZE; ++i) {
        buf[i] = get_byte(9, i);
    }

    int size = 0;
    int part = BUF_SIZE;
    while (size >= 0 && part > 0) {
        *(int *) buf = size;

        int written = write(fd, buf, part);
        if (written > 0) {
            size += written;
        } else if (written != -ENOSPC) {
            size = -1;
        }
        if (written != part) {
            part /= 2;
        }
    }

    if (close(fd) != 0) {
        return -1;
    }

    return size;
}

/*
    Returns 0 if the file that `fill_disk` has written has `size` bytes, and
    every whole buffer of it starts with its place in the file and goes on
    with the bytes of the series.
*/
static int check_full_file(int size)
{
    stat_t info;
    if (stat(FULL_FILE, &info) != 0 || (int) info.size != size) {
        return 1;
    }

    int fd = open(FULL_FILE, O_RDONLY, 0);
    if (fd < 0) {
        return 1;
    }

    int result = 0;
    for (int pos = 0; pos + BUF_SIZE <= size; pos += BUF_SIZE) {
        if (lseek(fd, pos, SEEK_SET) != pos || read(fd, buf, 16) != 16 || *(int *) buf != pos) {
            result = 1;
        }
        for (int i = 4; i < 16; ++i) {
            if (buf[i] != get_byte(9, i)) {
                result = 1;
            }
        }
    }

    return close(fd) == 0 ? result : 1;
}

static int check_full_disk(void)
{
    int failures = make_file(FILE_B, 4, SMALL_SIZE) != 0;

    int size = fill_disk();
    failures += size < BUF_SIZE || check_full_file(size) != 0;

    // The file cannot take a byte more
    int fd = open(FULL_FILE, O_WRONLY | O_APPEND, 0);
    failures += write(fd, buf, 1) != -ENOSPC || close(fd) != 0;

    // Another file is whole, and the bytes come back
    failures += check_file(FILE_B, 4, SMALL_SIZE) != 0;
    failures += unlink(FULL_FILE) != 0;
    failures += fill_disk() != size || check_full_file(size) != 0;
    failures += unlink(FULL_FILE) != 0 || unlink(FILE_B) != 0;

    return failures;
}

int main(void)
{
    check(make_file(FILE_A, 1, LONG_SIZE) == 0 && check_file(FILE_A, 1, LONG_SIZE) == 0 &&
          make_file(FILE_B, 4, SMALL_SIZE) == 0 && check_file(FILE_B, 4, SMALL_SIZE) == 0,
          "a file keeps the bytes that were written to it");
    check(check_positions() == 0, "a file is read and written at any position");
    check(check_open_flags() == 0, "the flags of open do what they say");
    check(check_refusals() == 0, "the kernel refuses wrong requests for files");
    check(check_dup2() == 0, "a descriptor is copied by dup2 and by fork");
    check(move_file() == 0, "a file that grows moves to a larger place, and its open descriptors follow");
    check(check_names() == 0, "files are renamed and removed");
    check(check_directory() == 0, "stat and the listing of the directory tell what is on the disk");
    check(write_together() == 0, "two processes write to the disk at the same time");
    check(check_full_disk() == 0, "a file grows until the disk has no room for it, and the bytes come back");

    check(unlink(FILE_A) == 0, "the test files are removed");

    return get_result();
}

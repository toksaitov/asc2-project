/*
    A program that checks `fork`, `exit`, and `waitpid`.
*/

#include "user.h"

#include "check.h"

#define EXIT_CODE_OF_CHILD 42

#define MAX_DEPTH 15 // a chain of processes is shorter than this: the kernel has fewer entries for processes

#define SIGN_FILE "test_sign" // a file that tells a child to end

static int shared = 1; // a variable that a child changes

/*
    Makes a child that ends with `code`, and waits for it. Returns what
    `waitpid` has stored, or -1 if a call has failed.
*/
static int get_status_of_exit(int code)
{
    int pid = fork();
    if (pid == 0) {
        exit(code);
    }

    int status;
    if (pid < 0 || waitpid(pid, &status, 0) != pid) {
        return -1;
    }

    return status;
}

/*
    A child has a copy of the memory of its parent: what it changes there,
    the parent does not see. Returns 0 if that is so.
*/
static int change_memory_in_child(void)
{
    int local = 2;

    int parent_pid = getpid();
    int pid = fork();
    if (pid == 0) {
        // The child sees the values that the parent had, and has a number of its own
        int is_copy = shared == 1 && local == 2 && getpid() != parent_pid;

        shared = 10;
        local = 20;

        exit(is_copy && shared == 10 && local == 20 ? 0 : 1);
    }

    int status;
    if (pid <= 0 || pid == parent_pid || waitpid(pid, &status, 0) != pid || status != 0) {
        return 1;
    }

    return shared == 1 && local == 2 ? 0 : 1;
}

/*
    Waits for two children with `waitpid(-1)`, which takes the first child
    that has ended. The first child does not end before a file appears, and
    the parent makes that file when the second child has come. Returns 0 if
    both came in that order with their codes, and no third one.
*/
static int wait_for_any_child(void)
{
    int slow_pid = fork();
    if (slow_pid == 0) {
        stat_t info;
        while (stat(SIGN_FILE, &info) != 0) {
        }

        exit(1);
    }

    int quick_pid = fork();
    if (quick_pid == 0) {
        exit(2);
    }

    int status;
    if (slow_pid <= 0 || quick_pid <= 0 || waitpid(-1, &status, 0) != quick_pid || EXIT_CODE(status) != 2) {
        return 1;
    }

    int fd = creat(SIGN_FILE, 0644);
    if (fd < 0 || close(fd) != 0 || waitpid(-1, &status, 0) != slow_pid || EXIT_CODE(status) != 1 ||
        unlink(SIGN_FILE) != 0) {
        return 1;
    }

    return waitpid(-1, &status, 0) == -ECHILD ? 0 : 1;
}

/*
    Asks for children that the process does not have: itself, its parent,
    and a number that no process has. Returns 0 if the kernel refuses all.
*/
static int wait_for_strangers(void)
{
    int status;

    return waitpid(getpid(), &status, 0) == -ECHILD &&
           waitpid(1, &status, 0) == -ECHILD &&
           waitpid(12345, &status, 0) == -ECHILD ? 0 : 1;
}

/*
    Makes a chain of processes: each makes a child and waits for it, until
    `fork` fails because the machine is full. The process that could not make
    a child ends with its place in the chain and the reason: the place in the
    high four bits of its exit code, and the number of the error in the low
    four. Every process before it ends with the code of its child, so the
    first one gets that code too.
*/
static int make_chain(int depth)
{
    int pid = fork();
    if (pid < 0) {
        return (depth << 4) | -pid;
    }
    if (pid == 0) {
        exit(depth == MAX_DEPTH ? 0 : make_chain(depth + 1));
    }

    int status;
    if (waitpid(pid, &status, 0) != pid || STOP_REASON(status) != 0) {
        return 0;
    }

    return EXIT_CODE(status);
}

/*
    Leaves children behind. A child of this program makes two children of
    its own and ends without a wait for them: one of them has ended by then,
    and the other one still works. The kernel must free both by itself. It
    counts its free pages when the last process has ended, and stops with a
    message if one is lost.
*/
static int leave_children(void)
{
    int pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            exit(0);
        }
        if (fork() == 0) {
            spin(16);
            exit(0);
        }

        spin(4);
        exit(0);
    }

    int status;

    return pid > 0 && waitpid(pid, &status, 0) == pid && status == 0 ? 0 : 1;
}

int main(void)
{
    check(change_memory_in_child() == 0, "a child has a copy of the memory, and a number of its own");

    int status = get_status_of_exit(EXIT_CODE_OF_CHILD);
    check(EXIT_CODE(status) == EXIT_CODE_OF_CHILD && STOP_REASON(status) == 0, "the parent gets the exit code of a child");

    // The parent gets the low byte of the code
    check(EXIT_CODE(get_status_of_exit(-1)) == 255 && EXIT_CODE(get_status_of_exit(256 + 7)) == 7,
          "an exit code is one byte");

    check(wait_for_any_child() == 0, "a wait for any child takes the child that ends first");
    check(wait_for_strangers() == 0, "a wait for a process that is not a child is refused");

    int chain_code = make_chain(1);
    int chain_error = chain_code & 0xf;
    check((chain_code >> 4) >= 2 && (chain_error == ENOMEM || chain_error == EAGAIN),
          "fork fails when the machine is full, and the processes before it end well");

    // The memory of the chain is free again
    check(make_chain(1) == chain_code, "a second chain is as long as the first");

    check(leave_children() == 0, "a process may end before its children");

    return get_result();
}

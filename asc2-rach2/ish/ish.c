/*
 * ish: the input loop, the builtins, the path search, the redirections and
 * the running of a command in a child process.
 */

#include "user.h"

#include "ish_cstring_utilities.h"
#include "ish_shell_utilities.h"

static const unsigned long Max_Input_String_Length = 255;
static const unsigned long Max_Argument_Count = 255;
static const unsigned long Max_Executable_Path_Length = 256;

static const char Environment_Variable_Home[] = "HOME=";
static const char Environment_Variable_Path[] = "PATH=";

static const char Builtin_Command_CD[] = "cd";
static const char Builtin_Command_Exit[] = "exit";

static const char Prompt[] = "$ ";

static const int Standard_Input_File_Flags = O_RDONLY;
static const int Standard_Output_File_Mode = 00644;

static const char Fork_Error_Message[] = "ish: fork failure\n";
static const char Redirection_Error_Message[] = "ish: cannot open a redirection\n";
static const char Input_Length_Error_Message[] = "ish: command line too long\n";
static const char Not_Found_Error_Message[] = ": not found\n";
static const char Not_Runnable_Error_Message[] = ": cannot run\n";

/* The statuses a shell reports for what it could not run, which are the
 * conventional ones: 127 for a name that resolved to nothing, 126 for a
 * command found but not runnable, and 128 + N for a child that the kernel
 * stopped for the reason N. */
static const int Status_Not_Found = 127;
static const int Status_Not_Runnable = 126;
static const int Status_Signalled_Base = 128;

/** Write `subject` and then `message` to standard error. */
static void ish_report(const char *subject, const char *message, unsigned long message_length)
{
    write(2, subject, ish_get_cstring_length(subject));
    write(2, message, message_length);
}

/**
 * Wait for the child `pid` and return its status as a shell reports it: the
 * exit status, 128 + N for a child that the kernel stopped for the reason N,
 * or 126 when the wait fails.
 *
 * The wait status has the exit status in bits 8 to 15 and the reason of a
 * stop in bits 0 to 6.
 */
static int ish_wait_for_child(int pid)
{
    int child_status = 0;

    if (waitpid(pid, &child_status, 0) != pid) {
        return Status_Not_Runnable;
    }
    if ((child_status & 0x7F) != 0) {
        return Status_Signalled_Base + (child_status & 0x7F);
    }
    return (child_status >> 8) & 0xFF;
}

/**
 * Make `descriptor` stand for the file of `opened`, the result of an `open`,
 * and close `opened`. Returns 0, or -1 when the open or the move has failed.
 */
static int ish_move_descriptor(int opened, int descriptor)
{
    if (opened < 0 || dup2(opened, descriptor) < 0) {
        return -1;
    }
    if (opened != descriptor) {
        close(opened);
    }
    return 0;
}

/**
 * Run the command line `input` and return its status.
 *
 * `input` is split in place into arguments, and the `<` and `>` redirections
 * are taken out of them. `cd` and `exit` run in this process: `cd` with no
 * operand goes to `home`, and `exit` ends the shell with its operand's
 * status, or with `last_status`, the status of the line before, when it has
 * none. Any other command is looked up with `stat`, first as written and
 * then joined to each element of `paths`; a joined path longer than
 * `Max_Executable_Path_Length` is skipped, since cut short it could name
 * another program. A child opens the files of the redirections, the input
 * file read-only and the output file with `creat` at mode 0644, moves them
 * onto descriptors 0 and 1 and calls `execve` with `envp`, and the parent
 * waits for it.
 *
 * `home` and `paths` are the values of HOME and PATH, or null.
 *
 * Returns `last_status` for an empty line, the child's status from
 * `ish_wait_for_child`, 0 or 1 for `cd`, 127 for a command not found, and
 * 126 for a failed fork, or from a child whose redirection cannot be opened
 * or whose `execve` fails.
 */
static int ish_run_line(char *input, int last_status, char *home, char *paths, char **envp)
{
    char *arguments[Max_Argument_Count + 1];
    arguments[Max_Argument_Count] = 0;

    ish_build_argument_array_from_input(input, arguments, Max_Argument_Count);

    char *stdin_file = ish_extract_last_stdin_redirection_file(arguments);
    char *stdout_file = ish_extract_last_stdout_redirection_file(arguments);

    const char *command = arguments[0];
    if (!command) {
        return last_status;
    }

    if (ish_are_cstrings_equal(command, Builtin_Command_CD)) {
        char *directory = arguments[1] ? arguments[1] : home;

        return directory && chdir(directory) == 0 ? 0 : 1;
    }

    if (ish_are_cstrings_equal(command, Builtin_Command_Exit)) {
        exit(arguments[1] ? ish_get_integer_from_cstring(arguments[1]) : last_status);
    }

    char *executable = arguments[0];

    char candidate[Max_Executable_Path_Length + 1];
    candidate[Max_Executable_Path_Length] = '\0';

    /* The shell reads nothing from the result: it asks whether the file is there. */
    stat_t stat_result;
    if (stat(executable, &stat_result) != 0) {
        executable = 0;
        for (char *cursor = paths; cursor && !executable;) {
            char *path = ish_get_token_in_cstring(cursor, ish_is_path_separator, &cursor);
            if (path) {
                int cut = ish_combine_path_elements(path, arguments[0], candidate,
                                                    Max_Executable_Path_Length);

                if (!cut && stat(candidate, &stat_result) == 0) {
                    executable = candidate;
                }
            }
        }
    }

    if (!executable) {
        ish_report(command, Not_Found_Error_Message, sizeof(Not_Found_Error_Message) - 1);

        return Status_Not_Found;
    }

    int status = 0;
    int pid = fork();
    if (pid == 0) {
        if ((stdin_file &&
             ish_move_descriptor(open(stdin_file, Standard_Input_File_Flags, 0), 0) != 0) ||
            (stdout_file &&
             ish_move_descriptor(creat(stdout_file, Standard_Output_File_Mode), 1) != 0)) {
            write(2, Redirection_Error_Message, sizeof(Redirection_Error_Message) - 1);
            exit(Status_Not_Runnable);
        }
        execve(executable, arguments, envp);
        ish_report(command, Not_Runnable_Error_Message, sizeof(Not_Runnable_Error_Message) - 1);
        exit(Status_Not_Runnable);
    } else if (pid > 0) {
        status = ish_wait_for_child(pid);
    } else {
        write(2, Fork_Error_Message, sizeof(Fork_Error_Message) - 1);
        status = Status_Not_Runnable;
    }

    return status;
}

/**
 * Read command lines from descriptor 0 and run them.
 *
 * Each line is run as its newline arrives, in reads of up to 255 bytes, so a
 * line may span reads. A last line with no newline runs at end of file, and
 * a read error discards it. A line longer than 255 bytes, not counting its
 * newline, is reported and discarded up to its newline. A prompt is written
 * before the read of a new line when the input is the console: the console
 * has no positions, so `lseek` fails on it and works on a file.
 *
 * Returns the status of the last line; the `exit` builtin ends the shell
 * earlier.
 *
 * `_start` in `user/crt0.s` passes `argc`, the null-terminated `argv` and
 * the null-terminated `envp` from the initial process stack.
 */
int main(int argc, char **argv, char **envp)
{
    char *home = ish_get_first_environment_variable(Environment_Variable_Home,
                                                    sizeof(Environment_Variable_Home) - 1, envp);
    char *paths = ish_get_first_environment_variable(Environment_Variable_Path,
                                                     sizeof(Environment_Variable_Path) - 1, envp);

    int prompting = lseek(0, 0, SEEK_CUR) < 0;

    char input[Max_Input_String_Length];
    char line[Max_Input_String_Length + 1];
    unsigned long length = 0;
    int discarding = 0;
    int status = 0;
    int bytes_read;

    /* A person at the console is greeted with the file `motd`, if the disk has one. */
    if (prompting) {
        int motd = open("motd", O_RDONLY, 0);
        if (motd >= 0) {
            while ((bytes_read = read(motd, input, Max_Input_String_Length)) > 0) {
                write(1, input, bytes_read);
            }
            close(motd);
        }
    }

    for (;;) {
        if (prompting && length == 0 && !discarding) {
            write(1, Prompt, sizeof(Prompt) - 1);
        }

        bytes_read = read(0, input, Max_Input_String_Length);
        if (bytes_read <= 0) {
            break;
        }

        int i;
        for (i = 0; i < bytes_read; ++i) {
            if (input[i] == '\n') {
                if (!discarding) {
                    line[length] = '\0';
                    status = ish_run_line(line, status, home, paths, envp);
                }
                length = 0;
                discarding = 0;
            } else if (!discarding) {
                if (length < Max_Input_String_Length) {
                    line[length++] = input[i];
                } else {
                    write(2, Input_Length_Error_Message, sizeof(Input_Length_Error_Message) - 1);
                    discarding = 1;
                }
            }
        }
    }
    if (bytes_read == 0 && length != 0 && !discarding) {
        line[length] = '\0';
        status = ish_run_line(line, status, home, paths, envp);
    }

    return status;
}

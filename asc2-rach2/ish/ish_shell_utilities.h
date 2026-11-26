/*
 * The parts of a command line ish takes apart: the argument array, the
 * environment entries and the redirections.
 */

#ifndef ISH_SHELL_UTILITIES_H
#define ISH_SHELL_UTILITIES_H

#include "ish_common.h"
#include "ish_cstring_utilities.h"

static const char ISH_Standard_Input_Redirection_Lexeme[] = "<";
static const char ISH_Standard_Output_Redirection_Lexeme[] = ">";

/**
 * Split `input` in place into whitespace-separated tokens.
 *
 * Leading and repeated white space is skipped, and the character that ends
 * each token is overwritten with a NUL, so the entries of `arguments` point
 * into `input`. At most `max_argument_count` tokens are taken and the rest
 * are dropped, and a null pointer is written after the last one, so
 * `arguments` must have one slot more than `max_argument_count`.
 *
 * Returns the number of tokens after the first, the operand count of the
 * command in `arguments[0]`, or 0 when there is no token.
 */
unsigned long ish_build_argument_array_from_input(char *input, char **arguments,
                                                  unsigned long max_argument_count);

/**
 * The value of the first entry of `environment` that begins with `key`, or
 * null when none does.
 *
 * `key` includes its `=`, such as `"PATH="`, and `key_length` is its length
 * without the NUL. The match is exact and case-sensitive. The value points
 * into the entry.
 */
char *ish_get_first_environment_variable(const char *key, unsigned long key_length,
                                         char **environment);

/**
 * The file name of the last `<` redirection in the null-terminated
 * `arguments`, or null when there is none or the last one has no file name.
 *
 * Every `<` and the file name after it are taken out of the array, and the
 * arguments behind them move down, so a redirection may stand anywhere in
 * the line and the command keeps all its other arguments.
 */
char *ish_extract_last_stdin_redirection_file(char **arguments);

/** As `ish_extract_last_stdin_redirection_file`, for `>`. */
char *ish_extract_last_stdout_redirection_file(char **arguments);

#endif

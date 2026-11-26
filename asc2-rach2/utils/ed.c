/*
    ed: a small line editor in the manner of the editor of the first Unix
    systems.

        ed [file]

    The editor keeps the lines of a text in memory, and one of them is the
    current line. A command is a line: at most two addresses, a letter, and
    what the letter needs. An address is the number of a line, `.` for the
    current line, or `$` for the last one; `,` alone means all the lines.

        3           goes to the line 3 and prints it
        (empty)     goes to the next line and prints it
        2,5p        prints the lines 2 to 5; `,p` prints all
        2,5n        the same with the numbers of the lines
        3a          appends lines after the line 3, until a line with only `.`
        3i          inserts lines before the line 3, in the same way
        2,5c        changes the lines: removes them and takes new ones
        2,5d        removes the lines
        s/old/new/  puts `new` in place of the first `old` of the current
                    line; with `g` at the end, of every `old`; it takes
                    addresses too, as in `1,$s/old/new/g`
        =           prints the number of lines
        w [file]    writes the text to the file and prints its bytes
        q           ends the editor; if the text has changed since it was
                    written, the first `q` only prints `?`
        Q           ends the editor at once
        h           prints this list in short

    The answer to a command that the editor cannot do is `?`, as it was in
    1971.
*/

#include "user.h"

#define MAX_LINES 512   // lines of a text
#define POOL_SIZE 12288 // bytes of all its lines together
#define LINE_SIZE 256   // bytes of a line with its zero byte
#define NAME_SIZE 64    // bytes of the name of the file with its zero byte
#define IN_SIZE   256   // bytes that are read from the input at a time

/*
    The lines live in a pool one after another, in the order in which they
    were typed or read. A line that is removed or changed leaves its bytes
    behind; when the pool is full, the lines that are left move to the
    other pool, in their order.
*/
static char pools[2][POOL_SIZE];
static int pool;                           // the pool that is in use
static unsigned int used;                  // its bytes that are taken
static unsigned short starts[MAX_LINES];   // where the line of each number begins; the first line is the number 1, at the index 0
static unsigned short lengths[MAX_LINES];
static int count;                          // lines of the text
static int current;                        // the number of the current line; 0 while the text is empty
static int is_changed;                     // the text differs from what was written last
static int was_asked;                      // the last command was a `q` that was refused
static char file_name[NAME_SIZE];

static char input[IN_SIZE];                // bytes of the input that no line has taken yet
static int input_count;
static int input_next;

// Moves the lines that are left to the other pool. Returns 0 if `more` bytes fit behind them then.
static int tidy(unsigned int more)
{
    char *to = pools[1 - pool];
    unsigned int at = 0;

    for (int i = 0; i < count; ++i) {
        const char *from = pools[pool] + starts[i];
        starts[i] = (unsigned short) at;
        for (int k = 0; k < lengths[i]; ++k) {
            to[at++] = from[k];
        }
    }
    pool = 1 - pool;
    used = at;

    return used + more <= POOL_SIZE ? 0 : -1;
}

// Puts a line of `length` bytes after the line `after` (0 puts it first). Returns 0, or -1 if the text is full.
static int add_line(int after, const char *text, int length)
{
    if (count == MAX_LINES || (used + length > POOL_SIZE && tidy(length) != 0)) {
        return -1;
    }

    for (int i = count; i > after; --i) {
        starts[i] = starts[i - 1];
        lengths[i] = lengths[i - 1];
    }
    starts[after] = (unsigned short) used;
    lengths[after] = (unsigned short) length;
    for (int k = 0; k < length; ++k) {
        pools[pool][used++] = text[k];
    }
    ++count;
    is_changed = 1;

    return 0;
}

// Removes the lines from `first` to `last`.
static void remove_lines(int first, int last)
{
    int removed = last - first + 1;

    for (int i = last; i < count; ++i) {
        starts[i - removed] = starts[i];
        lengths[i - removed] = lengths[i];
    }
    count -= removed;
    is_changed = 1;
}

/*
    Reads a line of the input into `line`, without its end. Returns its
    length, or -1 at the end of the input. A line that is too long is cut.
*/
static int get_line(char *line)
{
    int length = 0;

    for (;;) {
        if (input_next == input_count) {
            input_count = read(STDIN, input, IN_SIZE);
            input_next = 0;
            if (input_count <= 0) {
                input_count = 0;
                return length > 0 ? length : -1;
            }
        }

        char c = input[input_next++];
        if (c == '\n') {
            return length;
        }
        if (c != '\r' && length < LINE_SIZE - 1) {
            line[length++] = c;
        }
    }
}

// Reads the file into the text. Returns the number of its bytes, or -1 if it cannot be read or does not fit.
static int read_file(const char *path)
{
    static char line[LINE_SIZE];
    char buf[IN_SIZE];
    int length = 0;
    int bytes = 0;
    int got;

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    while ((got = read(fd, buf, IN_SIZE)) > 0) {
        bytes += got;
        for (int i = 0; i < got; ++i) {
            if (buf[i] == '\n' || length == LINE_SIZE - 1) {
                if (add_line(count, line, length) != 0) {
                    close(fd);
                    return -1;
                }
                length = 0;
            }
            if (buf[i] != '\n') {
                line[length++] = buf[i];
            }
        }
    }
    close(fd);
    if (length > 0 && add_line(count, line, length) != 0) {
        return -1;
    }

    return got < 0 ? -1 : bytes;
}

// Writes the text to the file. Returns the number of its bytes, or -1.
static int write_file(const char *path)
{
    char buf[IN_SIZE];
    int filled = 0;
    int bytes = 0;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        const char *text = pools[pool] + starts[i];
        for (int k = 0; k <= lengths[i]; ++k) {
            buf[filled++] = k == lengths[i] ? '\n' : text[k];
            if (filled == IN_SIZE) {
                if (write(fd, buf, filled) != filled) {
                    close(fd);
                    return -1;
                }
                bytes += filled;
                filled = 0;
            }
        }
    }
    if (filled > 0 && write(fd, buf, filled) != filled) {
        close(fd);
        return -1;
    }
    close(fd);

    return bytes + filled;
}

// Prints the lines from `first` to `last`, with their numbers if `has_numbers`.
static void print_lines(int first, int last, int has_numbers)
{
    for (int i = first; i <= last; ++i) {
        if (has_numbers) {
            print_number(STDOUT, (unsigned int) i);
            write(STDOUT, "\t", 1);
        }
        write(STDOUT, pools[pool] + starts[i - 1], lengths[i - 1]);
        write(STDOUT, "\n", 1);
    }
    current = last;
}

// Takes lines from the input, until a line with only `.`, and puts them after the line `after`. Returns 0, or -1 if the text is full.
static int take_lines(int after)
{
    static char line[LINE_SIZE];
    int length;

    while ((length = get_line(line)) >= 0) {
        if (length == 1 && line[0] == '.') {
            break;
        }
        if (add_line(after, line, length) != 0) {
            return -1;
        }
        ++after;
        current = after;
    }

    return 0;
}

/*
    Puts `new` in place of `old` in a line: of its first `old`, or of every
    one if `is_every`. Returns 1 if the line has changed, 0 if it has no
    `old`, and -1 if the new line is too long or the text is full.
*/
static int change_line(int number, const char *old, int old_length, const char *new, int new_length, int is_every)
{
    static char line[LINE_SIZE];
    const char *text = pools[pool] + starts[number - 1];
    int length = lengths[number - 1];
    int out = 0;
    int changes = 0;
    int i = 0;

    while (i < length) {
        int k = 0;
        if (old_length > 0 && (changes == 0 || is_every) && i + old_length <= length) {
            while (k < old_length && text[i + k] == old[k]) {
                ++k;
            }
        }
        if (old_length > 0 && k == old_length) {
            if (out + new_length > LINE_SIZE - 1) {
                return -1;
            }
            for (k = 0; k < new_length; ++k) {
                line[out++] = new[k];
            }
            i += old_length;
            ++changes;
        } else {
            if (out == LINE_SIZE - 1) {
                return -1;
            }
            line[out++] = text[i++];
        }
    }
    if (changes == 0) {
        return 0;
    }

    // The new line takes the place of the old one
    if (add_line(number, line, out) != 0) {
        return -1;
    }
    remove_lines(number, number);

    return 1;
}

// Reads a number of a line at `*text` and moves `*text` behind it. Returns 1 if an address was there.
static int get_address(const char **text, int *number)
{
    const char *at = *text;

    if (*at == '.') {
        *number = current;
        ++at;
    } else if (*at == '$') {
        *number = count;
        ++at;
    } else if (*at >= '0' && *at <= '9') {
        int value = 0;
        while (*at >= '0' && *at <= '9') {
            if (value < 100000) {
                value = value * 10 + (*at - '0');
            }
            ++at;
        }
        *number = value;
    } else {
        return 0;
    }
    *text = at;

    return 1;
}

static void help(void)
{
    // Every line fits a row of the screen of the machine
    print(STDOUT, "3      go to line 3\n"
                  ",p     print all\n"
                  "2,5n   with numbers\n"
                  "a i c  add lines; end\n"
                  "       with a line: .\n"
                  "2,5d   remove lines\n"
                  "s/a/b/ put b for a\n"
                  "w write   q quit\n");
}

/*
    Does a command. Returns 1 if the editor is to end, 0 if the command is
    done, and -1 if it cannot be done.
*/
static int run(const char *text)
{
    int first = current;
    int last = current;
    int has_address = 0;
    int was_refused = was_asked;

    was_asked = 0;

    if (*text == ',') {
        first = 1;
        last = count;
        has_address = 1;
        ++text;
        if (get_address(&text, &last)) {
            /* `,5` is from the first line to the line 5 */
        }
    } else if (get_address(&text, &first)) {
        has_address = 1;
        last = first;
        if (*text == ',') {
            ++text;
            if (!get_address(&text, &last)) {
                last = count;
            }
        }
    }

    char letter = *text;
    if (letter != '\0') {
        ++text;
    }

    // The commands that need no line of the text
    if (letter == 'q' || letter == 'Q') {
        if (letter == 'q' && is_changed && !was_refused) {
            was_asked = 1;
            return -1;
        }
        return 1;
    }
    if (letter == 'h') {
        help();
        return 0;
    }
    if (letter == '=') {
        print_number(STDOUT, (unsigned int) count);
        write(STDOUT, "\n", 1);
        return 0;
    }
    if (letter == 'w') {
        while (*text == ' ') {
            ++text;
        }
        if (*text != '\0') {
            int i = 0;
            while (text[i] != '\0' && i < NAME_SIZE - 1) {
                file_name[i] = text[i];
                ++i;
            }
            file_name[i] = '\0';
        }
        int bytes = file_name[0] == '\0' ? -1 : write_file(file_name);
        if (bytes < 0) {
            return -1;
        }
        print_number(STDOUT, (unsigned int) bytes);
        write(STDOUT, "\n", 1);
        is_changed = 0;
        return 0;
    }

    // Lines are appended also to an empty text, and after the line 0, which is before the first one
    if (letter == 'a' || (letter == 'i' && count == 0)) {
        if (first < 0 || first > count || *text != '\0') {
            return -1;
        }
        return take_lines(first);
    }

    // An empty command goes to the next line, and a number alone to that line
    if (letter == '\0' && !has_address) {
        first = current + 1;
        last = first;
    }
    if (first < 1 || last > count || first > last) {
        return -1;
    }

    if (letter == '\0' || letter == 'p' || letter == 'n') {
        if (letter != '\0' && *text != '\0') {
            return -1;
        }
        print_lines(first, last, letter == 'n');
        return 0;
    }
    if (letter == 'i') {
        return *text != '\0' ? -1 : take_lines(first - 1);
    }
    if (letter == 'd' || letter == 'c') {
        if (*text != '\0') {
            return -1;
        }
        remove_lines(first, last);
        current = first <= count ? first : count;
        if (letter == 'c') {
            return take_lines(first - 1);
        }
        return 0;
    }
    if (letter == 's') {
        // The sign after the `s` parts the old text from the new one and ends the new one
        char sign = *text;
        if (sign == '\0') {
            return -1;
        }
        const char *old = text + 1;
        int old_length = 0;
        while (old[old_length] != '\0' && old[old_length] != sign) {
            ++old_length;
        }
        if (old_length == 0 || old[old_length] == '\0') {
            return -1;
        }
        const char *new = old + old_length + 1;
        int new_length = 0;
        while (new[new_length] != '\0' && new[new_length] != sign) {
            ++new_length;
        }
        int is_every = new[new_length] == sign && new[new_length + 1] == 'g';

        int changed = 0;
        for (int i = first; i <= last; ++i) {
            int result = change_line(i, old, old_length, new, new_length, is_every);
            if (result < 0) {
                return -1;
            }
            if (result > 0) {
                changed = i;
            }
        }
        if (changed == 0) {
            return -1;
        }
        print_lines(changed, changed, 0);
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[])
{
    static char line[LINE_SIZE];

    if (argc > 2) {
        print(STDERR, "ed: one file at the most\n");
        return 1;
    }
    if (argc == 2) {
        int i = 0;
        while (argv[1][i] != '\0' && i < NAME_SIZE - 1) {
            file_name[i] = argv[1][i];
            ++i;
        }
        file_name[i] = '\0';

        int bytes = read_file(file_name);
        if (bytes < 0) {
            // A file that is not there yet is a new text
            count = 0;
            print(STDOUT, file_name);
            print(STDOUT, ": a new file\n");
        } else {
            print_number(STDOUT, (unsigned int) bytes);
            write(STDOUT, "\n", 1);
        }
        current = count;
        is_changed = 0;
    }

    for (;;) {
        int length = get_line(line);
        if (length < 0) {
            break;
        }
        line[length] = '\0';

        int result = run(line);
        if (result > 0) {
            break;
        }
        if (result < 0) {
            print(STDOUT, "?\n");
        }
    }

    return 0;
}

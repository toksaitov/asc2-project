/*
    Mark: the benchmark of the machine.

    Four kinds of work, each with an answer that is known beforehand. A kind
    of work whose answer is wrong gets no points, and then the machine gets
    none: a fast wrong machine is worth nothing.

    * processor: plain instructions. A sieve finds the primes below 4096
      and a sort puts 64 numbers in order, both in C, where nearly every
      instruction costs a request on the bus; and a short loop in assembly
      stirs a number 60,000 times out of the cache of the chip.
    * accelerator: a neuron of twelve inputs learns, and a small network
      answers questions, with the instructions of the array (`dotv`,
      `learn`, `dotm`, `dot4`; the jobs of `race.c`). The first answers are
      compared with those of plain code, all of them with a known sum.
    * system: calls of the kernel, children that are made and end, and the
      file of the shell read from the disk twice.
    * screen: pictures of rectangles, lines, and text.

    The points of a kind of work are 1000 times the time that the first
    machine of this kind took for it on its board, divided by the time that
    it takes now: 1000 points mean "as fast as the first machine", 2000
    "twice as fast". The points of the machine are the mean of the four.

    The result goes into the file `mark.last`, and the next run tells what
    has changed since.

    The button held down for two seconds ends the program.
*/

#include "user.h"

#include "lib/machine.h"
#include "lib/show.h"

/* The work and its answers */
#define SIEVE_SIZE   4096
#define SIEVE_PRIMES 564         /* primes below 4096 */
#define SORT_COUNT   64
#define MIX_SEED     0x2545f491u
#define MIX_ROUNDS   60000
#define MIX_ANSWER   0x3e19770eu /* what `mix` makes of the seed in that many rounds */

#define UNITS        32          /* steps of learning or answers in one call of a job of `race_asm.s` */
#define LEARN_CALLS  480
#define THINK_CALLS  720
#define LEARN_ANSWER 4280884473u /* the sums of what the calls return; a program built with `-DCALIBRATE` prints them */
#define THINK_ANSWER 22301280u

#define KERNEL_CALLS 300
#define CHILDREN     4
#define FILE_NAME    "ish"
#define FILE_PASSES  2
#define BUF_SIZE     512

#define PICTURES     100

#define RESULT_FILE  "mark.last"
#define RESULT_US    12000000    /* how long the result stays on the screen */

/* The kinds of work */
#define PROCESSOR   0
#define ACCELERATOR 1
#define SYSTEM      2
#define SCREEN      3
#define KINDS       4

/*
    The microseconds that the first machine took for each kind of work on
    its board: 1000 points. Measured on 2026-11-26 on the FPGA board with
    the chip at 12.5 MHz behind the RP2040, under the kernel whose `fork`
    shares the pages of code; the runs differed by about one part in a
    hundred.
*/
static const unsigned int first_us[KINDS] = { 2151000, 2572000, 3808000, 2808000 };

static const char *const names[KINDS] = { "processor", "accelerator", "system", "screen" };
static const char *const short_names[KINDS] = { "cpu", "ai", "os", "scr" };

/* The functions of `mark_asm.s`, `race_asm.s`, and `lib/array.s` */

unsigned int mix(unsigned int seed, int rounds);
int learn_fast(const unsigned int *examples, int count);
int learn_plain(const unsigned int *examples, int count, signed char *weights);
int think_fast(const unsigned int *samples, int count, unsigned int output);
int think_plain(const unsigned int *samples, int count, unsigned int output, const unsigned int *hidden);
void set_weights(unsigned int w0, unsigned int w1, unsigned int w2);
void get_weights(unsigned int *words);

static unsigned int took_us[KINDS];
static unsigned int points[KINDS];
static int is_right[KINDS];

static unsigned int seed = MIX_SEED;

static unsigned int rnd(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return seed;
}

/* The processor */

static unsigned char sieve[SIEVE_SIZE];
static unsigned int numbers[SORT_COUNT];

static int work_processor(void)
{
    unsigned int primes = 0;
    unsigned int before = 0;
    unsigned int after = 0;
    int in_order = 1;
    int i;
    int j;

    for (i = 2; i < SIEVE_SIZE; ++i) {
        sieve[i] = 1;
    }
    for (i = 2; i < SIEVE_SIZE; ++i) {
        if (sieve[i]) {
            ++primes;
            for (j = i * i; j < SIEVE_SIZE; j += i) {
                sieve[j] = 0;
            }
        }
    }

    for (i = 0; i < SORT_COUNT; ++i) {
        numbers[i] = rnd() & 0xffff;
        before += numbers[i];
    }
    for (i = 1; i < SORT_COUNT; ++i) {
        unsigned int number = numbers[i];
        for (j = i; j > 0 && numbers[j - 1] > number; --j) {
            numbers[j] = numbers[j - 1];
        }
        numbers[j] = number;
    }
    for (i = 0; i < SORT_COUNT; ++i) {
        after += numbers[i];
        if (i > 0 && numbers[i - 1] > numbers[i]) {
            in_order = 0;
        }
    }

    return primes == SIEVE_PRIMES && in_order && before == after && mix(MIX_SEED, MIX_ROUNDS) == MIX_ANSWER;
}

/* The accelerator: the jobs of `race.c`, with its data */

static unsigned int examples[4 * UNITS]; /* for learning: the twelve inputs in three words, and the right answer */
static unsigned int samples[UNITS];      /* for thinking: the four inputs of a question */
static unsigned int hidden[3];           /* the weights of the three neurons of the network */
static unsigned int output;              /* and of its output */

/* A word of four signed bytes between -63 and 63 */
static unsigned int rnd_word(void)
{
    unsigned int word = 0;
    int i;

    for (i = 0; i < 4; ++i) {
        unsigned int r = rnd();
        word |= (((r & 63) + ((r >> 6) & 63) - 63) & 0xff) << (8 * i);
    }

    return word;
}

/* The examples follow a rule that the neuron does not know: twelve weights, and the answer is the sum of the products, in 128ths */
static void make_data(void)
{
    unsigned int rule[3];
    int i;
    int j;

    seed = MIX_SEED;
    for (i = 0; i < 3; ++i) {
        rule[i] = rnd_word();
        hidden[i] = rnd_word();
    }
    output = rnd_word() & 0xffffff;
    for (i = 0; i < UNITS; ++i) {
        int sum = 0;
        for (j = 0; j < 3; ++j) {
            examples[4 * i + j] = rnd_word();
        }
        for (j = 0; j < 12; ++j) {
            sum += ((signed char *) rule)[j] * ((signed char *) &examples[4 * i])[j];
        }
        examples[4 * i + 3] = (unsigned int) (sum >> 7);
        samples[i] = rnd_word();
    }
}

/* Plain code does the first calls of both jobs too; returns 1 if its answers and its weights are those of the array. Not timed. */
static int has_plain_answers(void)
{
    unsigned int plain[3];
    unsigned int chip[3];
    int same = 1;
    int i;

    plain[0] = plain[1] = plain[2] = 0;
    set_weights(0, 0, 0);
    for (i = 0; i < 2; ++i) {
        if (learn_fast(examples, UNITS) != learn_plain(examples, UNITS, (signed char *) plain)) {
            same = 0;
        }
    }
    get_weights(chip);
    for (i = 0; i < 3; ++i) {
        if (chip[i] != plain[i]) {
            same = 0;
        }
    }

    set_weights(hidden[0], hidden[1], hidden[2]);
    if (think_fast(samples, UNITS, output) != think_plain(samples, UNITS, output, hidden)) {
        same = 0;
    }

    return same;
}

static unsigned int learn_sum; /* the sums of what the timed calls returned */
static unsigned int think_sum;

static int work_accelerator(void)
{
    int i;

    learn_sum = 0;
    think_sum = 0;
    set_weights(0, 0, 0);
    for (i = 0; i < LEARN_CALLS; ++i) {
        learn_sum += (unsigned int) learn_fast(examples, UNITS);
    }
    set_weights(hidden[0], hidden[1], hidden[2]);
    for (i = 0; i < THINK_CALLS; ++i) {
        think_sum += (unsigned int) think_fast(samples, UNITS, output);
    }

#ifdef CALIBRATE
    /* For new work: the sums that `LEARN_ANSWER` and `THINK_ANSWER` must be */
    put_str("mark: the sums of the accelerator are ");
    put_dec(learn_sum);
    put_str(" and ");
    put_dec(think_sum);
    put_str("\n");
#endif

    return learn_sum == LEARN_ANSWER && think_sum == THINK_ANSWER;
}

/* The system */

static int work_system(void)
{
    static char buf[BUF_SIZE];
    stat_t status;
    int me = getpid();
    int right = 1;
    int i;

    for (i = 0; i < KERNEL_CALLS; ++i) {
        if (getpid() != me) {
            right = 0;
        }
    }

    /* A child has a copy of the memory of the program; it ends at once with a code that the program knows */
    for (i = 0; i < CHILDREN; ++i) {
        int end = 0;
        int child = fork();
        if (child == 0) {
            exit(40 + i);
        }
        if (child < 0 || waitpid(child, &end, 0) != child || EXIT_CODE(end) != 40 + i) {
            right = 0;
        }
    }

    if (stat(FILE_NAME, &status) != 0) {
        return 0;
    }
    for (i = 0; i < FILE_PASSES; ++i) {
        unsigned int bytes = 0;
        int got;
        int fd = open(FILE_NAME, O_RDONLY, 0);
        if (fd < 0) {
            return 0;
        }
        while ((got = read(fd, buf, BUF_SIZE)) > 0) {
            /* A program of this machine begins with the four bytes of an ELF file */
            if (bytes == 0 && (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F')) {
                right = 0;
            }
            bytes += (unsigned int) got;
        }
        close(fd);
        if (got < 0 || bytes != status.size) {
            right = 0;
        }
    }

    return right;
}

/* The screen */

static int work_screen(void)
{
    unsigned int frames = RP_SCR_FRAMES;
    int picture;
    int i;

    for (picture = 0; picture < PICTURES; ++picture) {
        scr_clear();
        for (i = 0; i < 16; ++i) {
            scr_rect((i * 8 + picture * 3) & 127, 8 + ((i * 5 + picture) & 31), 6, 12, i & 1 ? SCR_FLIP : SCR_LIGHT);
        }
        for (i = 0; i < 6; ++i) {
            RP_SCR_MOVE = SCR_POINT(picture * 2, 63 - i * 4);
            RP_SCR_LINE = SCR_POINT(127 - i * 20, 8 + picture);
        }
        scr_text(0, 0, "MARK: the screen");
        scr_show();
    }

    return RP_SCR_FRAMES - frames == PICTURES;
}

/* The text of a number; returns its length */
static int to_text(char *text, unsigned int value)
{
    char digits[10];
    int count = 0;
    int length = 0;

    do {
        digits[count++] = (char) ('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count > 0) {
        text[length++] = digits[--count];
    }
    text[length] = '\0';

    return length;
}

/* The table of the kinds of work: the points of those that are done, and a mark at the one that is worked on */
static void show_table(int working)
{
    int kind;

    scr_clear();
    show_header("MARK");
    for (kind = 0; kind < KINDS; ++kind) {
        scr_text(2 + kind, 1, names[kind]);
        if (kind == working) {
            scr_text(2 + kind, 0, ">");
            scr_text(2 + kind, 14, "works");
        } else if (kind < working) {
            if (is_right[kind]) {
                scr_dec(2 + kind, 14, points[kind]);
            } else {
                scr_text(2 + kind, 14, "WRONG");
            }
        }
    }
    scr_text(7, 0, "1000 = first machine");
    scr_show();
}

/* Reads the points of the last run from the file; 0 if there was none */
static unsigned int read_last(void)
{
    char text[16];
    unsigned int value = 0;
    int got;
    int i;

    int fd = open(RESULT_FILE, O_RDONLY, 0);
    if (fd < 0) {
        return 0;
    }
    got = read(fd, text, sizeof(text));
    close(fd);
    for (i = 0; i < got && text[i] >= '0' && text[i] <= '9'; ++i) {
        value = value * 10 + (unsigned int) (text[i] - '0');
    }

    return value;
}

/* Writes the points of the machine and of the four kinds of work as a line of text; returns 0 if the file has them */
static int write_result(unsigned int total)
{
    char text[64];
    int length = to_text(text, total);
    int kind;
    int fd;
    int written;

    for (kind = 0; kind < KINDS; ++kind) {
        text[length++] = ' ';
        length += to_text(text + length, points[kind]);
    }
    text[length++] = '\n';

    fd = open(RESULT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return -1;
    }
    written = write(fd, text, (unsigned int) length);
    close(fd);

    return written == length ? 0 : -1;
}

int main(void)
{
    unsigned int last = read_last();
    unsigned int total = 0;
    int all_right = 1;
    int is_leaving;
    int kind;

    if (scr_open() != 0) {
        put_str("mark: another program has the screen\n");
        return 1;
    }
    put_str("mark: four kinds of work with checked answers; hold the button for two seconds to end\n");

    is_leaving = show_title("MARK", "A benchmark: four\nkinds of work, each\nwith a known answer.\n1000 points: as fast\nas the first machine.",
                            "hold: end");
    make_data();
    if (!is_leaving && !has_plain_answers()) {
        put_str("mark: the accelerator and plain code gave different answers\n");
        all_right = 0;
    }

    for (kind = 0; kind < KINDS && !is_leaving; ++kind) {
        unsigned int start;

        show_table(kind);
        start = RP_TIME;
        if (kind == PROCESSOR) {
            is_right[kind] = work_processor();
        } else if (kind == ACCELERATOR) {
            is_right[kind] = work_accelerator();
        } else if (kind == SYSTEM) {
            is_right[kind] = work_system();
        } else {
            is_right[kind] = work_screen();
        }
        took_us[kind] = RP_TIME - start;

        /* The times are divided by 16 first, so that the product fits a word */
        points[kind] = is_right[kind] ? (first_us[kind] >> 4) * 1000 / ((took_us[kind] >> 4) + 1) : 0;
        total += points[kind];
        if (!is_right[kind]) {
            all_right = 0;
        }
        is_leaving = show_button() == SHOW_LEAVE;
    }
    if (is_leaving) {
        scr_close();
        put_str("mark: ended before the work was done; no points\n");
        return 0;
    }
    total = all_right ? total / KINDS : 0;

    /* The result: the points of the machine in large digits, the four kinds of work, and the last run */
    {
        char text[12];
        int length = to_text(text, total);
        int col;

        scr_clear();
        show_header("MARK");
        show_big((SCR_WIDTH - 12 * length) / 2, 11, text, 3);
        for (kind = 0; kind < KINDS; ++kind) {
            int row = 4 + (kind >> 1);
            int left = (kind & 1) ? 11 : 0;
            scr_text(row, left, short_names[kind]);
            if (is_right[kind]) {
                scr_dec(row, left + 4, points[kind]);
            } else {
                scr_text(row, left + 4, "WRONG");
            }
        }
        if (last == 0) {
            scr_text(6, 0, "the first run here");
        } else {
            unsigned int percent = (total >= last ? total - last : last - total) * 100 / last;

            col = scr_dec(6, 5, last);
            scr_text(6, 0, "last");
            if (percent == 0) {
                scr_text(6, col, ": the same");
            } else {
                scr_text(6, col + 1, total >= last ? "+" : "-");
                col = scr_dec(6, col + 2, percent);
                scr_text(6, col, "%");
            }
        }
        scr_text(7, 0, "press: end");
        scr_show();
    }
    if (all_right && write_result(total) != 0) {
        put_str("mark: cannot write the file mark.last\n");
    }
    show_wait(RESULT_US);
    scr_close();

    put_str("Mark: ");
    put_dec(total);
    put_str(" points.");
    for (kind = 0; kind < KINDS; ++kind) {
        put_str(kind == 0 ? " The " : ", the ");
        put_str(names[kind]);
        put_str(" ");
        if (is_right[kind]) {
            put_dec(points[kind]);
            put_str(" in ");
            put_dec(took_us[kind] / 1000);
            put_str(" ms");
        } else {
            put_str("gave a wrong answer");
        }
    }
    put_str(". 1000 points mean as fast as the first machine of this kind on its board.");
    if (last != 0) {
        put_str(" The last run gave ");
        put_dec(last);
        put_str(" points.");
    }
    put_str("\n");

    return all_right ? 0 : 1;
}

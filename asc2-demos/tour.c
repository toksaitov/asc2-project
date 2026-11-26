/*
    Tour: shows the machine with nobody at the button.

    The tour starts the programs of the showcase one after another. Before
    each one it shows a card with the name of the program and what is to be
    seen. A program that the tour starts gets one argument more than its
    name, and with it the program shows itself: it goes through its
    pictures and ends after about half a minute (`show_tour` of
    `lib/show.c`).

    The button works as in every program: a press goes on, and a hold of
    two seconds ends the program that is running, after which the tour goes
    on with the next one. A hold while a card of the tour is on the screen
    ends the tour.

    The tour is a program of the kernel: it makes a child for every
    program (`fork`), the child becomes the program (`execve`), and the
    tour waits for its end (`waitpid`). Only one process has the screen at
    a time, so the tour gives the screen back before it starts a child.
*/

#include "user.h"

#include "lib/machine.h"
#include "lib/show.h"

#define CARD_US 4000000 /* how long a card of the tour stays */
#define END_US  8000000 /* and the last one */

typedef struct stop
{
    char *name;       /* the program */
    const char *what; /* what is to be seen: up to three lines of at most 21 characters */
} stop_t;

/* What is beautiful comes first, then what the accelerator does, then the numbers, and the film ends the tour. */
static const stop_t stops[] = {
    { "shapes", "3D: models turn in\nspace, as lines and\nwith filled sides." },
    { "flight", "3D: a flight by night\nthrough a valley." },
    { "race", "The AI accelerator\nagainst plain code:\nwhich one is faster?" },
    { "pilot", "A neuron starts with\nnothing and learns to\nfly the burkut." },
    { "fence", "Three neurons learn\nto keep the sheep in\nand the wolves out." },
    { "scope", "Twelve numbers clean\na noisy signal." },
    { "guess", "The chip learns to\nguess the next press\nof the button." },
    { "sched", "A neuron inside the\nkernel decides how\nlong a program runs." },
    { "mark", "The benchmark: how\nfast is the machine?" },
    { "film", "A film: a day in the\nhigh mountains." }
};

#define STOPS ((int) (sizeof(stops) / sizeof(stops[0])))

/* Shows a card for a time or until the button does something; returns what it did. `number` is 0 for the last card. */
static int show_card(int number, const char *name, const char *what, const char *hint, unsigned int us)
{
    int event;

    if (scr_open() != 0) {
        return -1;
    }
    show_forget(); /* what the button did in the program before is not for the tour */

    scr_clear();
    show_header("TOUR");
    if (number > 0) {
        int col = scr_dec(2, 0, (unsigned int) number);
        scr_text(2, col + 1, "of");
        col = scr_dec(2, col + 4, (unsigned int) STOPS);
        scr_text(2, col, ":");
        scr_text(2, col + 2, name);
    }
    show_lines(number > 0 ? 4 : 2, what);
    scr_text(7, 0, hint);
    scr_show();

    event = show_wait(us);
    scr_close();

    return event;
}

int main(void)
{
    int shown = 0;
    int event = SHOW_NONE;
    int i;

    put_str("tour: the programs of the showcase one after another; a hold of the button ends a program, and during a card of the tour it ends the tour\n");

    for (i = 0; i < STOPS; ++i) {
        char *arguments[3];
        char *environment[1];
        int child;

        event = show_card(i + 1, stops[i].name, stops[i].what, "hold now: end tour", CARD_US);
        if (event < 0) {
            put_str("tour: another program has the screen\n");
            return 1;
        }
        if (event == SHOW_LEAVE) {
            break;
        }

        /* The console has the screen while the program loads: it shows this line, and a terminal keeps it */
        put_str("tour ");
        put_dec((unsigned int) (i + 1));
        put_str(" of ");
        put_dec((unsigned int) STOPS);
        put_str(": ");
        put_str(stops[i].name);
        put_str("\n");

        arguments[0] = stops[i].name;
        arguments[1] = "tour";
        arguments[2] = NULL;
        environment[0] = NULL;
        child = fork();
        if (child == 0) {
            execve(stops[i].name, arguments, environment);
            put_str("tour: cannot start ");
            put_str(stops[i].name);
            put_str("\n");
            exit(1);
        }
        if (child < 0) {
            put_str("tour: cannot make a process\n");
            return 1;
        }
        waitpid(child, NULL, 0);
        ++shown;
    }

    if (event != SHOW_LEAVE) {
        show_card(0, "", "That was Almurut II.\n\nEvery program starts\nby its name: type ls\nfor the list.", "", END_US);
    }

    put_str("The tour showed ");
    put_dec((unsigned int) shown);
    put_str(" of ");
    put_dec((unsigned int) STOPS);
    put_str(" programs. Every one of them starts by its name from the shell.\n");

    return 0;
}

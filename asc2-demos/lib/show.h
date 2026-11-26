/*
    How the programs of the showcase present themselves: one look and one
    rule for the button in all of them.

    * A program starts with a title card: its name, a few lines that say
      what is seen, and what the button does.
    * A press of the button is "next"; the button held down for two seconds
      ends the program.
    * A speed is shown as a bar, and a result as a large number.
*/

#ifndef SHOW_H
#define SHOW_H

/* What the button has done */
#define SHOW_NONE  0
#define SHOW_NEXT  1 /* a press */
#define SHOW_LEAVE 2 /* held down for two seconds */

/*
    Returns what the button has done since the last call. A press counts
    when the button is let go, so that a hold is not a press first.
*/
int show_button(void);

/* Waits for a time in microseconds, or until the button does something; returns what it did. */
int show_wait(unsigned int us);

/*
    For the tour, which shows a program with nobody at the button. `argc`
    is the number of the arguments of the program: the tour starts a
    program with one argument more than its name, and without one this
    call does nothing and returns 0. Otherwise it returns 1, and from then
    on `show_button` answers every `next` seconds as if the button had been
    pressed (never if `next` is 0) and after `leave` seconds as if it had
    been held. The button itself still counts.
*/
int show_tour(int argc, unsigned int next, unsigned int leave);

/* Returns 1 once the time that `show_tour` has given the program is over: for a program that watches the button by itself. */
int show_tour_over(void);

/* Forgets what the button has done so far: for a program that has the screen again after another one had it. */
void show_forget(void);

/* Draws the name of what is shown in dark letters on a lit bar in the first row of text. */
void show_header(const char *name);

/*
    Shows the title card and waits for a few seconds or for a press. `text`
    has up to four lines of at most 21 characters, parted by '\n'; `hint`
    is the last row: what the button does. Returns 1 if the button was held
    down: the program ends.
*/
int show_title(const char *name, const char *text, const char *hint);

/* Draws lines of text parted by '\n' from a row of text on. */
void show_lines(int row, const char *text);

/* Draws a bar across the screen, 7 rows high from the row `y` on, filled as far as `done` of `goal`. */
void show_bar(int y, unsigned int done, unsigned int goal);

/*
    Draws large characters (the digits, '.', and 'x') with the left top
    corner at a place; a cell of a character is `scale` pixels wide and
    high, and a character has 5 rows of cells. Returns the column after the
    text.
*/
int show_big(int x, int y, const char *text, int scale);

#endif

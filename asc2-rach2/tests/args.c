/*
    A program that prints what the kernel has given it at its start: the
    arguments and the environment. It ends with the number of arguments.
    `execs.c` starts it.
*/

#include "user.h"

// Prints the strings of a list that ends with a NULL, each in brackets on a line of its own.
static void print_list(const char *name, char *list[])
{
    for (int i = 0; list[i] != NULL; ++i) {
        print(STDOUT, name);
        print(STDOUT, " ");
        print_number(STDOUT, i);
        print(STDOUT, ": [");
        print(STDOUT, list[i]);
        print(STDOUT, "]\n");
    }
}

int main(int argc, char *argv[], char *envp[])
{
    print_number(STDOUT, argc);
    print(STDOUT, " arguments\n");

    print_list("argument", argv);
    print_list("variable", envp);

    return argc;
}

/*
    Messages of the kernel on the console.
*/

#ifndef _KIO_H_
#define _KIO_H_

// Prints a character.
void k_putc(char ch);

// Prints a string.
void k_puts(const char *str);

/*
    Prints a string in which `%` starts a place for the next argument:

    * `%c`: a character;
    * `%s`: a string;
    * `%d`: a number with a sign, in decimal;
    * `%u`: a number without a sign, in decimal;
    * `%x`: a number in hexadecimal;
    * `%p`: an address: `0x` and eight hexadecimal digits;
    * `%%`: the percent sign.
*/
void k_printf(const char *format, ...);

#endif

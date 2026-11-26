/*
 * The definition every ish source shares: `ish_check`, the shell's assertion.
 */

#ifndef ISH_COMMON_H
#define ISH_COMMON_H

#include "user.h"

/**
 * Exit the process with status -1, which a parent reads as 255, when `value`
 * is false.
 *
 * A failed precondition is fatal and never recovered.
 */
#define ish_check(value) \
    do {                 \
        if (!(value))    \
            exit(-1);    \
    } while (0)

#endif

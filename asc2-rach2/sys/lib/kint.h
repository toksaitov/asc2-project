/*
    Integer types of fixed sizes. The kernel is built without the C library,
    so it names the types of the compiler by itself.
*/

#ifndef _KINT_H_
#define _KINT_H_

typedef signed char        int8_t;
typedef unsigned char      uint8_t;

typedef signed short       int16_t;
typedef unsigned short     uint16_t;

typedef signed int         int32_t;
typedef unsigned int       uint32_t;

typedef signed long long   int64_t;
typedef unsigned long long uint64_t;

// An integer that holds any address
typedef unsigned int       uintptr_t;

#endif

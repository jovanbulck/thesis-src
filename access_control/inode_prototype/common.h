#ifndef COMMON_H
#define COMMON_H

#ifndef NSANCUS_COMPILE
    #include <sancus/sm_support.h>
    #include <sancus_support/uart.h>
    #include <msp430.h>
    #include <sancus_support/tsc.h>
#endif // NSANCUS_COMPILE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define SUCCESS         1       // a positive value, to indicate success
#define FAILURE         EOF     // a negative value, to indicate failure

int putchar(int c);

#ifdef NSANCUS_COMPILE
    // HACK TO MAKE SANCUS TESTABLE ON PC
    typedef unsigned int sm_id;
    sm_id sancus_get_caller_id(void);
    void sancus_set_caller_id(sm_id);

    struct SancusModule {};

    //empty defines
    #define SM_FUNC(str)
    #define SM_ENTRY(str)
    #define SM_DATA(str)
    #define DECLARE_SM(name, vendor)
#endif // NSANCUS_COMPILE

void __attribute__((noinline)) printf_int(const char* fmt, unsigned int i);
void __attribute__((noinline)) printf_long(const char* fmt, unsigned long i);
void __attribute__((noinline)) printf_str(const char* string);
void __attribute__((noinline)) printf_char(const char);

void print_buf(const char* msg, uint8_t* buf, size_t len);

extern bool DEBUG;

#define RED         "\033[1;31m"
#define CYAN        "\033[1;36m"
#define NONE        "\033[0m"       // to flush the previous color property

extern intptr_t public_int;
#ifndef NODEBUG
#define printdebug_int(string, integer) \
    do { \
        if (DEBUG) { \
            public_int = (intptr_t) integer; \
            printf_int(string, public_int); \
        } \
    } while(0)
    
#define printdebug(string) \
    do { \
        if (DEBUG) \
            puts(string); \
    } while (0)

#define printdebug_str(string) \
    do { \
        if (DEBUG) \
            printf_str(string); \
    } while (0)

#define printdebug_char(c) \
    do { \
        if (DEBUG) \
            printf_char(c); \
    } while (0)

#define printf_cyan(str) \
    do { \
        printf_str(CYAN); \
        printf_str(str); \
        printf_str(NONE); \
    } while(0)

#else //NODEBUG
#define printdebug_int(string, integer)
#define printdebug(string)
#define printf_cyan(str)
#define printdebug_str(str)
#define printdebug_char(c)
#endif

#define printerr(string) \
    do { \
        printf_str(RED); \
        puts(string); \
        printf_str(NONE); \
    } while(0)

#define printerr_int(string, integer) \
    do { \
        printf_str(RED); \
        public_int = (intptr_t) integer; \
        printf_int(string, public_int); \
        printf_str(NONE); \
    } while(0)

#endif // COMMON_H

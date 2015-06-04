#ifndef COMMON_H
#define COMMON_H

#include <sancus/sm_support.h>
#include <sancus_support/uart.h>
#include <msp430.h>
#include <stdio.h>

void __attribute__((noinline)) printf_str(const char* str);
void __attribute__((noinline)) printf_int(const char* fmt, unsigned int i);
void __attribute__((noinline)) printf_int_int(const char* fmt, unsigned int i, unsigned int j);

#define printerr(str) puts(RED str NONE)
#define printerr_int(str, int)  printf_int(RED str NONE, int)
#define printerr_int_int(str, i, j)  printf_int_int(RED str NONE, i, j)

#define EXIT        while(1) {}
#define SUCCESS     0
#define FAILURE     -1

#define SAY_HI(str) \
do { \
    sm_id caller_id = sancus_get_caller_id(); \
    SAY_HI_ID(str, caller_id); \
} while(0)

#define SAY_HI_ID(name, caller_id) \
do { \
    printf_int_int("[" name "] self_id = %d ; caller_id = %d\n", \
    sancus_get_self_id(), caller_id); \
} while(0)


#ifndef NOCOLOR
#define BLACK       "\033[1;30m"
#define RED         "\033[1;31m"
#define GREEN       "\033[0;32m"
#define YELLOW      "\033[0;33m"
#define BLUE        "\033[0;34m"
#define MAGENTA     "\033[0;35m"
#define CYAN        "\033[0;36m"
#define WHITE       "\033[0;37m"
#define BOLD        "\033[1m"
#define NONE        "\033[0m"       // to flush the previous color property
#else
#define BLACK       ""
#define RED         ""
#define GREEN       ""
#define YELLOW      ""
#define BLUE        ""
#define MAGENTA     ""
#define CYAN        ""
#define WHITE       ""
#define BOLD        ""
#define NONE        ""
#endif


#endif //COMMON_H

#include "common.h"

#ifndef NSANCUS_COMPILE
int putchar(int c)
{
    if (c == '\n')
        putchar('\r');

    uart_write_byte(c);
    return c;
}
#endif

bool DEBUG = true;

intptr_t public_int;

void __attribute__((noinline)) printf_int(const char* fmt, unsigned int i) {
    printf(fmt, i);
}

void __attribute__((noinline)) printf_long(const char* fmt, unsigned long i) {
    printf(fmt, i);
}

void __attribute__((noinline)) printf_str(const char* string) {
    printf("%s", string);
}

void __attribute__((noinline)) printf_char(const char c) {
    printf("%c", c);
}

void print_buf(const char* msg, uint8_t* buf, size_t len)
{
    printf("%s: ", msg);

    for (size_t i = 0; i < len; i++)
        printf("%02x ", buf[i]);

    printf("\n");
}


#ifdef NSANCUS_COMPILE
    sm_id caller_id = 0; //HACKHACKHACK

    sm_id sancus_get_caller_id(void) {
        return caller_id;
    }

    void sancus_set_caller_id(sm_id sm) {
        caller_id = sm;
    }
#endif // NSANCUS_COMPILE

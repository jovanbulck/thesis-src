#include "common.h"

void __attribute__((noinline)) printf_str(const char* str)
{
    printf("%s", str);
}

void __attribute__((noinline)) printf_int(const char* fmt, unsigned int i) {
    printf(fmt, i);
}

void __attribute__((noinline)) printf_int_int(const char* fmt, unsigned int i, unsigned int j) {
    printf(fmt, i, j);
}

int my_global_var = -1;

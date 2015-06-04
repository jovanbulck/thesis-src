/*
 * serialisation.h: a header defining macros that
 *  1. convert a specific data type into a sequence of bytes (serialisation / marshalling)
 *      and write it byte per byte to disk at consecutive addresses.
 *  2. read a number of bytes from disk at consecutive addresses and convert it to a 
 *      specified data type (deserialisation / unmarshalling).
 *  3. if the request cannot be satisfied, the macros print an errmsg and return FAILURE
 */
#ifndef SERIALISATION_H
#define SERIALISATION_H

#include "disk-driver.h"
#include "common.h"
#include <string.h>

// ############################ BYTE #################################

#define put_byte(addr, byte) \
    do { \
        if (disk_put_byte(addr, byte) == EOF) { \
            printerr("\t[basic-file-sys] invalid addr - EOF on writing byte to disk"); \
            return FAILURE; \
        } \
    } while (0)

#define get_byte(addr, result_ptr) \
    do { \
        int tmp_rv = disk_get_byte(addr); \
        if (tmp_rv == EOF) { \
            printerr_int("\t[basic-file-sys] invalid addr '%d' - EOF on reading byte from disk\n", addr); \
            return FAILURE; \
        } \
        *result_ptr = (unsigned char) tmp_rv;\
    } while (0)


// ############################ OTHER DATATYPE (GENERIC) ########################

/*
 * NOTE: only pass constant expressions as addr argument; it is evaluated more then once
 */
#define put_val(addr, datatype, value) \
    do { \
        unsigned char bytes[sizeof(datatype)]; \
        memcpy(&bytes, &value, sizeof(datatype)); \
        int i; \
        for (i = 0; i < sizeof(datatype); i++) \
            put_byte(addr + i, bytes[i]); \
    } while (0)

#define get_val(addr, datatype, result_ptr) \
    do { \
        unsigned char bytes[sizeof(datatype)]; \
        int i; \
        for (i = 0; i < sizeof(datatype); i++) \
            get_byte(addr + i, &bytes[i]); \
        memcpy(result_ptr, &bytes, sizeof(datatype)); \
    } while (0)

#endif // SERIALISATION_H

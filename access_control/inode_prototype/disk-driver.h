/*
 * disk-driver: a generic interface-layer for fetching and writing data to a storage disk
 *  This layer abstracts the physical disk to a contiguous array of addressable bytes. It 
 *  is thus responsible for translating byte addresses to physical device addresses.
 *
 * TODO flash memory can only be written as a whole block... (byte addressable, but not
 *  byte-writable)
 */
#ifndef RAM_DISK_DRIVER_H
#define RAM_DISK_DRIVER_H
#include "common.h"

//extern struct SancusModule diskDriver;

//TODO how to make this implementation dependend, while keeping available in header??
//--> in superblock + get fction

#define DISK_SIZE       1152

/*
 * Used for testing: to substract the overhead of verifying the SM first time
 */
void SM_FUNC("fileSys") ping_disk_module(void);

/*
 * a typedef uniquely identifying a byte on the physical disk; starts from zero
 */
typedef unsigned long disk_addr_t;
#define NIL_DISK_ADDR    (DISK_SIZE + 1)

int SM_FUNC("fileSys") disk_bulk_erase(void);

int SM_FUNC("fileSys") disk_erase_sector(disk_addr_t addr);

/*
 * disk_get_byte: returns the byte value on disk at the specified @param(addr).
 * @return: the char value on disk; EOF iff out of bounds
 */
int SM_FUNC("fileSys") disk_get_byte(disk_addr_t addr);

int SM_FUNC("fileSys") disk_get_bytes(disk_addr_t addr, unsigned char *buf, unsigned int len);

/*
 * disk_putbyte: writes out a specified @param(val) byte on disk at @param(addr).
 * @return: the value written out on success; EOF iff out of bounds
 */
int SM_FUNC("fileSys") disk_put_byte(disk_addr_t addr, unsigned char val);

#endif // disk-driver.h

/*
 * ram-disk: a disk-driver implementation that simulates a storage disk with a 
 *  char array in Sancus-protected memory.
 */
#include "disk-driver.h"

//DECLARE_SM(diskDriver, 0x1234);

unsigned char SM_DATA("fileSys") disk[DISK_SIZE];

int SM_ENTRY("fileSys") disk_get_byte(disk_addr_t idx) {
    printdebug_int("\t\tram-disk: reading from addr %d", idx);
    printdebug_int("; byte-value is '0x%x'\n", (idx > DISK_SIZE)? EOF : disk[idx]);
    return (idx > DISK_SIZE)? EOF : disk[idx];
}

int SM_ENTRY("fileSys") disk_put_byte(disk_addr_t idx, unsigned char val) {
    printdebug_int("\t\tram-disk: writing byte-value '0x%x'", val);
    printdebug_int(" to addr %d\n", idx);
    return (idx > DISK_SIZE)? EOF : (disk[idx] = val);
}

void SM_ENTRY("fileSys") ping_disk_module(void) {
    return;
}

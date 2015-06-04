/*
 * A header defining valid file permissions, used in access control lists assiciated with
 *  an inode/file and client/Sancus SM. Permissions can be combined by bitwise-inclusive
 *  OR-ing the flags below.
 */
#ifndef FILE_PERM_H
#define FILE_PERM_H

typedef unsigned char perm_t;

// bit flags (in hex; 1 byte)
#define F_NIL           0x00        // no permissions
#define F_ALL           0xFF        // owner permissions
#define F_RD            0x01 
#define F_WR            0x02
#define F_UNLINK        0x04        // if on, SM is allowed to unlink the file TODO usefull? --> only creator

#endif // file-perm-h

/*
 * basic-file-sys: a layer on top of the disk-driver, providing the concept of a logical
 *  contiguous file/inode, identified by inode_nb. The content of a logical file is byte-
 *  addressable with a logical offset from the start (zero). This layer thus abstracts the
 *  disk from an unordered char array to an ordered set of logical files/inodes with
 *  associated meta data (access control list, size) and byte-addressable content.
 */
#ifndef BASIC_FILE_SYS_H
#define BASIC_FILE_SYS_H
#include "common.h"
#include "file-perms.h"

extern struct SancusModule fileSys;

#ifndef INODE_T_DEF // include guard
#define INODE_T_DEF
    typedef unsigned int inode_nb_t;    //TODO 256 enough?
    #define NIL_INODE_NB    0
#endif // inode_t_def

/*
 * mkfs: initializes an empty file system on disk
 * @return: a value >= 0 on success; a value < 0 on failure
 */
int SM_FUNC("fileSys") mkfs(void);

/*
 * init_inode: intitializes the inode specified by @param(nb) on disk. The new inode will
 *  have the specified initial permissions. If the inode already exists, it is not touched
 *  and this function returns a negative value.
 * @return: a value >= 0 on success; a negative value if the request cannot be satisfied.
 */
int SM_FUNC("fileSys") init_inode(inode_nb_t nb, sm_id sm, perm_t init_perms);

/*
 * free_inode: marks the specified inode as unused and frees any data blocks associated.
 * @return: a value >= 0 on success; a negative value if the request cannot be satisfied.
 */
int SM_FUNC("fileSys") free_inode(inode_nb_t nb);

/*
 * get_acl: returns the access control list permission flags for the specified SM and inode.
 * @return: the associated permission flags; F_NIL if none associated
 */
int SM_FUNC("fileSys") get_acl(inode_nb_t nb, sm_id sm);

/*
 * set_acl: associates the specified access control list permission flags to the specified
 *  SM and inode.
 * @return: a value >= 0 on success; a negative value if the request cannot be satisfied.
 */
int SM_FUNC("fileSys") set_acl(inode_nb_t nb, sm_id sm, int flags);

/*
 * Returns the size (nb of bytes) associated with the specified inode. XXX needed for seek
 */
int SM_FUNC("fileSys") get_size(inode_nb_t nb);

/*
 * get_char: returns the on-disk character value in the file specified by @param(nb), at
 *  the logical offset @param(offset) from the start (zero).
 * @return the on-disk byte value; EOF if out of bounds or @param(nb) invalid
 */
int SM_FUNC("fileSys") get_char(inode_nb_t nb, unsigned int offset);

/*
 * get_char: writes the specified @param(c) in the on-disk file specified by @param(nb),
 *  at the logical offset @param(offset) from the start (zero).
 * @return: the value written out on success; EOF if out of bounds or @param(nb) invalid
 */
int SM_FUNC("fileSys") put_char(inode_nb_t nb, unsigned int offset, unsigned char c);


int SM_FUNC("fileSys") back_end_print_file(inode_nb_t nb, unsigned int pos);

int SM_FUNC("fileSys") dump_inode_table(void);

#endif // basic-file-sys.h

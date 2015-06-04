/* 
 * file-sys.h: a header defining a UNIX-like file sytem interface. The file system is
 * implemented as a Sancus-protected SM, allowing  other SM's / 'clients' to create a 
 * 'semi-protected' (i.e. protected iff file-sys SM in TCB) buffer / 'file' in a 'back-end-
 * disk'. This back-end-disk is intended to be pluggable:
 *   1. RAM-disk implementation allows a form of secure shared memory between SMs
 *   2. flash disk implementation allows secure persistent storage for SMs
 * After opening, a client is returned a 'file-descriptor', which can be used for future 
 * accesses: reading/writing byte-per-byte, using an internal file pointer. Moreover a 
 * client can share its file with other SM's, using fine-grained permissions in an access
 * control list per file. Finally, a client closes and unlinks / deletes the file.
 */
#ifndef FILE_SYS_H
#define FILE_SYS_H
#include "common.h"
#include "file-perms.h"

extern struct SancusModule fileSys;

/*
 * file-descriptor: a symbolic identifier, provided by client for all file operations
 */
typedef unsigned int FD;

/*
 * A typedef uniquely identifying a logical file on the disk; starts counting from 1
 */
#ifndef INODE_T_DEF // include guard
#define INODE_T_DEF
    typedef unsigned int inode_nb_t;
    #define NIL_INODE_NB    0
#endif // inode_t_def

/*
 * Used for testing: to substract the overhead of verifying the SM first time
 */
void SM_ENTRY("fileSys") ping(void);

/*
 * f_mkfs: initializes an empty file system on back-end
 *  TODO auto at first client call??
 * @return: a value >= 0 on success; a value < 0 on failure
 */
int SM_ENTRY("fileSys") f_mkfs(void);

/*
 * Creates a new file, associated with @param(nb) in the back-end. The calling SM will be
 *  the owner (F_ALL perm) of the newly created empty file. If the file already exists, no
 *  new one will be created, the existing one will be untouched and a negative value will
 *  be returned. TODO (merge with open??)
 * @param nb: the inode nb of the file to create
 * @return: a value >= 0 on success; else a negative value.
 * @note: before a file can be used, the caller should also f_open() it.
 */
int SM_ENTRY("fileSys") f_creat(inode_nb_t nb);

/*
 * Open an existing file: returns a file descriptor for the file identified by @arg(nb).
 *  The given @arg(perms) are checked against the associated file permissions for the calling
 *  SM. If opening is successfull, the file descriptor can be used for future accesses and
 *  the file_pos is at the start of the file. Opening is unsuccessfull if either the 
 *  specified @arg(nb) doesn't exist; the given @arg(perms) aren't a subset of the 
 *  caller's file permissions, or the client has already MAX_NB_OPEN_FILES open files.
 * @param nb    : the id of the file to open
 * @param perms : the requested permissions on the open file
 * @return: the file descriptor (>= 0 converted to an unsigned int) if successfull; else a 
 *  negative value.
 * @note: according to UNIX semantics, opening the same file twices, results in two 
 *  independent file descriptors, associated with independent file positions.
 */
int SM_ENTRY("fileSys") f_open(inode_nb_t nb, perm_t perms);

/*
 * Closes the file descriptor @param(fd)'s connection to the open file. On success, this 
 *  means the file descriptor cannot be used anymore to access the file. The function returns
 *  unsuccessfully iff the given file descriptor isn't valid.
 * @param fd: the file descriptor of the file to close
 * @return: a value >= 0 on success; a negative value if the request could not be satisfied
 */
int SM_ENTRY("fileSys") f_close(FD fd);

/*
 * Removes the file, associated with @param(nb) in the back-end. On success, this means 
 *  any client still having a file desciptor connection to the file, won't be able to access
 *  the file anymore. This function checks the calling SM's permissions and returns unsuccess-
 *  fully if the file doesn't exists or the caller doesn't have the F_UNLINK permission.
 * @param nb: the id of the file to unlink
 * @return: a value >= 0 on success; a negative value if the request could not be satisfied
 * TODO what if some files still have an open fd? --> open_count??
 --> make sure you have revoked the permission of all sm's that you assigned them to...
 */
int SM_ENTRY("fileSys") f_unlink(inode_nb_t nb);

/*
 * Reads a single byte from the file associated with @param(fd). On success, the internal
 *  file_pos is afterwards incremented.
 * @param fd: the open file descriptor associated with the file to access
 * @return: the read character (converted to an unsigned int); else EOF if the request
 *  could not be satisfied.
 * @note: since the provided char is transfered via a CPU register, this function can be
 *  used to safely (i.e. without 3th party interference) read confidential data from a file. 
 */
int SM_ENTRY("fileSys") f_getc(FD fd);

/*
 * (Over)writes the provided @param(c) into the file associated with @param(fd). The value
 *  is written at file_pos++
 * @param fd    : the open file descriptor associated with the file to access
 * @param c     : the char to write into the file
 * @return: the character written (converted to an unsigned int) or EOF if the request
 *  could not be satisfied
 * @note: since the provided char is transfered via a CPU register, this function can be
 *  used to safely (i.e. without 3th party interference) write confidential data into a file.
 */
int SM_ENTRY("fileSys") f_putc(FD fd, unsigned char c);

/*
 * Sets the value of the client's internal file_pos to @param(position).
 * @param fd        : the open file descriptor associated with the file to access.
 * @param position  : the (positive) new value for file_pos. This will be relative 
 *  to the start of the file.
 * @return: the position on success; else EOF is the request could not be satisfied
 */
 // TODO maybe UNIX semantics: origin argument for relative pos
int SM_ENTRY("fileSys") f_seek(FD fd, unsigned int position);

int SM_ENTRY("fileSys") f_seek_fw(FD fd);

int SM_ENTRY("fileSys") f_seek_bw(FD fd);

int SM_ENTRY("fileSys") f_seek_end(FD fd);

#define f_seek_start(fd) \
    f_seek(fd, 0);

/*
 * Changes permissions to the file associated with @param(nb) for the Sancus SM with the 
 *  specified @param(id). The @param(perm_flags) will replace any already existing 
 *  permissions for the SM and file. On success, any access by a SM on the file, not allowed 
 *  by the specified permissions, will be denied. This function return unsuccessfully iff 
 *  the specified file doesn't exist or the caller doesn't have the F_ALL permission.
 * @param nb            : the unique id for the file to access
 * @param id            : the id of the SM to change the permissions for
 * @param perm_flags    : the permissions to assign, will replace any existing permissions
 * @return: a value >= 0 on success; a negative value if the request could not be satisfied
 */
int SM_ENTRY("fileSys") f_chmod(inode_nb_t nb, sm_id id, perm_t perm_flags);
//TODO functions that take symbolic notation (+r -w etc) to allow more fine grained mode spec?

/*
 * Deletes all permissions to the file associated with @param(fd) for the Sancus SM with 
 *  the specified @param(id).
 * @param file          : the unique id for the file to access
 * @param id            : the id of the SM to change the permissions for
 * @return: a value >= 0 on success; a negative value if the request could not be satisfied.
 */
#define revoke_all_permissions(file, id) \
    f_chmod(file, id, F_NIL);

/*
 * Pretty printing of a file associated with @param(fd) on stdout; iff (#ifndef NODEBUG)
 * @param fd        : the open file descriptor associated with the file to print 
 * @return: a value >= 0 on success; a negative value if the request could not be satisfied.
 */
int SM_ENTRY("fileSys") print_file(FD fd);

int SM_ENTRY("fileSys") fs_dump_inode_table(void);

#endif //file-sys-h

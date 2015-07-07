
// OPEN_FILE struct that holds an ACL linked list of FILE_PERM structs that each hold the perm flags and sm_id

struct FILE_PERM SM_DATA("sfs") *fd_cache[MAX_NB_OPEN_FILES];

// macros to aid access control

// check whether fd is within bounds and belongs to the caller
#define CHK_FD(fd, sm) \
    if ((fd < 0 || fd >= MAX_NB_OPEN_FILES) || !fd_cache[fd] || fd_cache[fd]->sm_id != sm) \
    { \
        printerror_int_int("the provided file descriptor %d isn't valid or " \
            "doesn't belong to calling SM %d", fd, sm); \
        return FAILURE; \
    }

#define CHK_PERM(p_have, p_want) \
do { \
    if ((p_have & p_want) != p_want) { \
        printerror_int_int("permission check failed p_have=0x%x ; p_want = 0x%x", \
            p_have, p_want); \
        return FAILURE; \
    } \
} while(0)

//example wrapper

int SM_ENTRY("sfs") sfs_getc(int fd)
{
    sm_id caller_id = sancus_get_caller_id();
   
    CHK_FD(fd, caller_id)
    CHK_PERM(fd_cache[fd]->flags, SFS_READ);
 
    return (cfs_read(fd, &buf, 1) > 0)? buf : EOF;
}

int SM_ENTRY("sfs") sfs_chmod(filename_t name, sm_id id, int perm_flags)
{
    sm_id caller_id = sancus_get_caller_id();

    // only root can chmod
    struct FILE_PERM *p_caller;
    if (two_phase_lookup(name, SFS_ROOT, id, &p_caller) < 0)
        return FAILURE;
    
    if (perm_flags == SFS_NIL)
        return revoke_acl(p_caller->file, id);
   
    return add_acl(p_caller->file, id, perm_flags);
}

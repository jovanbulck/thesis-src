/*
 * an implementation of the file-sys interface, as an additional layer on top of the
 *  basic-file-sys interface. This layer stores non-persistent client-specific info on open
 *  files (e.g. internal file posisiton). Moreover, this layer is responsible for performing
 *  access control and translating client calls to basic-file-system-calls.
 */
#include "file-sys.h"
#include "basic-file-sys.h"
#include "disk-driver.h" // for pinging

DECLARE_SM(fileSys, 0x1234);

// ############################### FILE SYS PARAM #################################
//TODO maybe header with all file system parameters???

#define MAX_NB_SM           5   //TODO use the correct Sancus cst here
#define MAX_NB_OPEN_FILES   3   // the max number of open files per client

// ############################### GLOBAL PROTECTED VARIABLES #####################

// open-file: entry in the per-client-open-file-list
struct OPEN_FILE {
    inode_nb_t inode;
    perm_t perm_flags;          // specified at open(); <= ACL perm entry for this client
    unsigned int file_pos;      // the client-specific offset for the next read/write
    char padding[2];
};

// client: represents a client of the allocator API (sizeof = 32 bytes)
struct CLIENT {
    sm_id id;
    // per-client-open-file-list, indexed by FD
    struct OPEN_FILE open_files[MAX_NB_OPEN_FILES];
    struct CLIENT *next;
    char padding[4];
};

struct CLIENT SM_DATA("fileSys") client_pool[MAX_NB_SM];
struct CLIENT SM_DATA("fileSys") *client_free_lst;
struct CLIENT SM_DATA("fileSys") *client_lst_head; //  XXX hash table

// ############################### FILE SYS API ###################################

// helper function definitions TODO SM_FUNC("fileSys")
void init_client_list(void);
struct CLIENT *lookup_client(sm_id);
struct CLIENT *create_new_client(sm_id);
int remove_client(sm_id);

#define LOOKUP_CLIENT(id, client) \
    do { \
        client = lookup_client(id); \
        if (!client) { \
            printerr("  [file-sys] client lookup failed. Returning..."); \
            return FAILURE; \
        } \
    } while (0)

#define IS_VALID_FD(fd) \
    (fd >= 0 && fd < MAX_NB_OPEN_FILES)

#define CHK_FD(fd, client) \
    if (!IS_VALID_FD(fd) || client->open_files[fd].inode == NIL_INODE_NB) { \
        printerr("  [file-sys] the provided file descriptor isn't valid. Returning..."); \
        return FAILURE; \
    }

#define CHK_PERM(p_have, p_want) \
    if ((p_have & p_want) != p_want) { \
        printerr("  [file-sys] permission check failed. Returning..."); \
        return FAILURE; \
    }

int SM_ENTRY("fileSys") f_mkfs(void) {
    printdebug("    [file-sys] f_mkfs");
    init_client_list();
    return mkfs();
}

void SM_ENTRY("fileSys") ping(void) {
    ping_disk_module();
    return;
}

int SM_ENTRY("fileSys") f_creat(inode_nb_t nb) {
    sm_id caller = sancus_get_caller_id();
    printdebug("  [file-sys] f_creat");
    return init_inode(nb, caller, F_ALL);
}

int SM_ENTRY("fileSys") f_open(inode_nb_t nb, perm_t p_want) {
    sm_id caller = sancus_get_caller_id();
    
    // 1. look up client; create new one if necessary
    struct CLIENT *client = lookup_client(caller);
    if (!client && !(client = create_new_client(caller))) {
        printerr("  [file-sys] f_open  - couldn't create new client struct");
        return FAILURE;
    }
    
    // 2. chk caller permissions
    printdebug("  [file-sys] f_open - look up and check caller's permissions on back-end");
    perm_t p_have = get_acl(nb, caller);
    CHK_PERM(p_have, p_want);
    
    // 3. find free file descriptor
    int i;
    for (i = 0; i < MAX_NB_OPEN_FILES; i++)
        if (client->open_files[i].inode == NIL_INODE_NB)
            break;  // the i th file descr entry is free
    if (i == MAX_NB_OPEN_FILES) {
        printerr("  [file-sys] f_open - client already has the max number of open files");
        return FAILURE;
    }
    
    // 4. set file descriptor association in per-client-open-file-table
    struct OPEN_FILE *entry = &client->open_files[i];
    entry->inode = nb;
    entry->perm_flags = p_want;
    entry->file_pos = 0;
    
    return i;
}

int SM_ENTRY("fileSys") f_close(FD fd) {
    sm_id caller = sancus_get_caller_id();
    printdebug("  [file-sys] f_close");    
    struct CLIENT *client;
    LOOKUP_CLIENT(caller, client);
    
    if (!IS_VALID_FD(fd))
        return FAILURE;
    
    // close the file
    client->open_files[fd].inode = NIL_INODE_NB;
    
    // remove the client, iff last file closed
    int i;
    for (i = 0; i < MAX_NB_OPEN_FILES; i++)
        if (client->open_files[i].inode != NIL_INODE_NB)
            break; // the client has at least one remaining open file
    if (i == MAX_NB_OPEN_FILES)
        remove_client(caller);
    
    return SUCCESS;
}

int SM_ENTRY("fileSys") f_unlink(inode_nb_t nb) {
    sm_id caller = sancus_get_caller_id();
    
    printdebug("  [file-sys] f_unlink - look up and check permissions on back-end");
    int p_have = get_acl(nb, caller);
    CHK_PERM(p_have, F_UNLINK);
    
    // remove file on back-end TODO also close the file?
    printdebug("  [file-sys] f_unlink - removing file on back-end");
    return free_inode(nb);
}

int SM_ENTRY("fileSys") f_getc(FD fd) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);

    struct OPEN_FILE *f = &client->open_files[fd];
    CHK_PERM(f->perm_flags, F_RD);
    
    printdebug("  [file-sys] f_getc - fetching char from back-end");
    return get_char(f->inode, f->file_pos++);
}

int SM_ENTRY("fileSys") f_putc(FD fd, unsigned char c) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);

    struct OPEN_FILE *f = &client->open_files[fd];
    CHK_PERM(f->perm_flags, F_WR);
    
    printdebug("  [file-sys] f_putc - writing char to back-end");
    return put_char(f->inode, f->file_pos++, c);
}

int SM_ENTRY("fileSys") f_chmod(inode_nb_t nb, sm_id id, perm_t perm_flags) {
    sm_id caller = sancus_get_caller_id();

    printdebug("  [file-sys] f_chmod - check caller's permissions");
    int p_caller = get_acl(nb, caller);
    CHK_PERM(p_caller, F_ALL);
    
    printdebug("  [file-sys] f_chmod - change permissions in the on-disk inode");
    if (set_acl(nb, id, perm_flags) < 0) {
        printerr("  [file-sys] f_chmod - changing on-disk permissions failed");
        return FAILURE;
    }
    
    // change any permissions cached in the client-open-file-list
    struct CLIENT *client = lookup_client(id);
    if (client) {
        int i;
        for (i = 0; i < MAX_NB_OPEN_FILES; i++)
            if (client->open_files[i].inode == nb)
                client->open_files[i].perm_flags = perm_flags;
    }
    
    return SUCCESS;
}

int SM_ENTRY("fileSys") f_seek(FD fd, unsigned int position) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);
    
    printdebug_int("  [file-sys] f_seek to position %d\n", position);

    struct OPEN_FILE *f = &client->open_files[fd];
    printdebug("  [file-sys] f_seek - checking inode size");
    if (position > get_size(f->inode))
        return EOF; // don't seek past end
    
    return (f->file_pos = position);
}


int SM_ENTRY("fileSys") f_seek_fw(FD fd) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);
    struct OPEN_FILE *f = &client->open_files[fd];
    
    printdebug("  [file-sys] f_seek_fw - checking inode size");
    if (f->file_pos == get_size(f->inode))
        return EOF;
    
    f->file_pos++;
    return f->file_pos;
}

int SM_ENTRY("fileSys") f_seek_bw(FD fd) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);

    struct OPEN_FILE *f = &client->open_files[fd];
    if (f->file_pos == 0)
        return EOF;
    
    f->file_pos--;
    return f->file_pos;
}

int SM_ENTRY("fileSys") f_seek_end(FD fd) {
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);

    struct OPEN_FILE *f = &client->open_files[fd];

    printdebug("  [file-sys] f_seek_end - fetching inode size");    
    return (f->file_pos = get_size(f->inode));
}

int SM_ENTRY("fileSys") print_file(FD fd) {
    #ifdef NODEBUG
        return SUCCESS;
    #endif
    
    sm_id caller = sancus_get_caller_id();
    struct CLIENT *client;
    LOOKUP_CLIENT(caller, client);
    CHK_FD(fd, client);

    struct OPEN_FILE *f = &client->open_files[fd];    
    CHK_PERM(f->perm_flags, F_RD);

    // disable debug output for pretty printing
    DEBUG = false;
    puts("-----------------------------------------------------------------------------");
    printf_int("\tPRINT FILE with fd %d ", fd);
    printf_int("for client SM %d\n", caller);

    back_end_print_file(f->inode, f->file_pos);

    puts("-----------------------------------------------------------------------------");
    // re-enable debug output
    DEBUG = true;
    return SUCCESS;
}

int SM_ENTRY("fileSys") fs_dump_inode_table(void) {
    return dump_inode_table();
}

// ############################### HELPER FUNCS ###################################

/*
 * Initializes the client_free_lst with MAX_NB_SM clients.
 */
void SM_FUNC("fileSys") init_client_list(void) {
    client_lst_head = NULL; // no clients registered

    // initialize the free list
    client_free_lst = &client_pool[0];
    int i;
    for (i = 0; i < MAX_NB_SM - 1; i++)
        client_pool[i].next = &client_pool[i+1];
    client_pool[i].next = NULL;
}

/*
 * Returns a pointer to the CLIENT entry associated with @param(id) or NULL iff not found
 * @note : the function is O(n) with n the number of registered clients TODO hash
 */
struct CLIENT SM_FUNC("fileSys") *lookup_client(sm_id id) {
    struct CLIENT *cur;
    for (cur = client_lst_head; cur != NULL; cur = cur->next)
        if (cur->id == id)
            return cur;
    return NULL;
}

/*
 * Creates a new CLIENT entry and returns a pointer to it (NULL on failure)
 */
struct CLIENT SM_FUNC("fileSys") *create_new_client(sm_id id) {
    struct CLIENT *ret = client_free_lst;
    if (!ret) {
        printerr("  [file-sys] could not create new client: max number reached");
        return NULL;
    }
    
    // re-arrage pointers
    client_free_lst = client_free_lst->next;
    ret->next = client_lst_head;
    client_lst_head = ret;
    
    // initialize the new client
    ret->id = id;
    int i;
    for (i = 0; i < MAX_NB_OPEN_FILES; i++)
        ret->open_files[i].inode = NIL_INODE_NB;
    return ret;
}

/*
 * Remove the associated client entry from the client list
 */
int SM_FUNC("fileSys") remove_client(sm_id id) {
    struct CLIENT *cur, *prev;
    for (cur = prev = client_lst_head; cur != NULL; prev = cur, cur = cur->next)
        if (cur->id == id)
            break;
    if (!cur)
        return FAILURE; // client not found
    
    if (cur == client_lst_head)
        client_lst_head = cur->next;
    else
        prev->next = cur->next;
    cur->next = client_free_lst;
    client_free_lst = cur;
    return SUCCESS;    
}

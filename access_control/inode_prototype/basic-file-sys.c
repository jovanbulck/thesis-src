/*
 * An implementation of the basic-file-sys interface, providing a logical disk view
 *  as an inode table followed by data blocks. Currently with these simplifications:
 * 
 *  1. No superblock: all file system parameters (inode table size, disk block size, data 
 *      block free list, disk size, ...) are "hardcoded"
 *  2. MAX_FILE_LENGTH: equals the number of *direct* data block ptrs in the inode
 *  3. MAX_ACL_LENGTH: equals the number of *direct* acl entries in the inode
 *  4. keep the free data block bitmap in memory (not on disk)
 *  5. no directory: clients pass the inode numbers directly, also eliminating the need
 *      to search for a free inode entry
 *  6. MAX_NB_FILES: equals the size of the on-disk inode table (as in real FS)
 */
#include "basic-file-sys.h"
#include "disk-driver.h"
#include "serialisation.h"

// ########################### FILE SYS PARAM #############################

#define MAX_NB_INODES           8
#define INODE_TABLE_SIZE        (INODE_SIZE * MAX_NB_INODES)    // 128

// TODO (DISK_SIZE - INODE_TABLE_SIZE) % DATA_BLOCK_SIZE should be 0
#define DATA_BLOCK_SIZE         32
#define NB_DATA_BLOCKS          ((DISK_SIZE - INODE_TABLE_SIZE) / DATA_BLOCK_SIZE)  // 32

#define ACL_LENGTH              2
#define MAX_NB_DATA_BLOCKS      4
#define MAX_FILE_LENGTH         (MAX_NB_DATA_BLOCKS * DATA_BLOCK_SIZE)

// ########################### LOGICAL INODE LAYOUT #########################

/*
 * the sequence number of a logical data block; starts from zero
 */
typedef unsigned char block_nb_t; //TODO limiting to 256 data blocks?
#define NIL_BLOCK_NB        (NB_DATA_BLOCKS + 1)

struct acl_entry {
    sm_id who;
    perm_t perms;
    char padding;
};
// sizeof(struct) isn't equal to sum of sizeof of each member (compiler struct align)
#define ACL_ENTRY_SIZE      (sizeof(sm_id) + sizeof(perm_t) + 1)    // 4 bytes

struct the_inode {
    unsigned char in_use; //TODO use a "mode" byte here --> also indicate ACL length && public rw
    unsigned int size;
    struct acl_entry acl[ACL_LENGTH];
    block_nb_t data[MAX_NB_DATA_BLOCKS];
    char padding;
};
#define INODE_SIZE  (1 + sizeof(int) + ACL_ENTRY_SIZE*ACL_LENGTH + sizeof(block_nb_t)*MAX_NB_DATA_BLOCKS + 1)   // 16 bytes 

/*
 * All data (including inodes) is written to disk as a consecutive sequence of bytes. To 
 *  address the individual inode-fields, one can use the macros below to get the offset in
 *  bytes relative from the start (zero).
 */
#define IN_USE_IDX          0
#define SIZE_IDX            1
#define ACL_ID_IDX(i)       (1 + sizeof(int) + i * ACL_ENTRY_SIZE)
#define ACL_PERM_IDX(i)     (1 + sizeof(int) + sizeof(sm_id) + i*ACL_ENTRY_SIZE)
#define BLOCK_NB_IDX(i)     (1 + sizeof(int) + ACL_ENTRY_SIZE*ACL_LENGTH + i*sizeof(block_nb_t))

// ########################### DISK USAGE BIT MAP ###########################

// an in-memory bitmap keeping track of the data block usage: true iff data block i in use
bool SM_DATA("fileSys") disk_usage[NB_DATA_BLOCKS];

// ########################### BASIC FS API ###############################

//helper function declarations TODO bug SM_FUNC("fileSys")
block_nb_t alloc_data_block(void);
void free_data_block(block_nb_t);

/*
 * Returns the disk address for the start of the specified inode
 */
#define get_istart(inode_nb) \
    ((inode_nb - 1) * INODE_SIZE)

/*
 * Returns failure iff inode nb out of bounds.
 */
#define validate_inode_nb(nb) \
    if (nb == NIL_INODE_NB || nb > MAX_NB_INODES) { \
        printerr("\t[basic-file-sys] inode_nb out of bounds"); \
        return FAILURE; \
    }

/*
 * Retuns FAILURE and print errmsg iff specified inode_nb out of bounds or not in use.
 */
#define validate_inode(nb) \
    do {  \
        validate_inode_nb(nb); \
        printdebug("\t[basic-file-sys] validate_inode - fetching in_use field from disk"); \
        int in_use; \
        get_byte(get_istart(nb) + IN_USE_IDX, &in_use); /*TODO disk access...*/\
        if ((unsigned char) in_use == 'N') { \
            printerr("\t[basic-file-sys] inode_nb not in use; initialize first"); \
            return FAILURE; \
        } \
    } while (0)

#define get_disk_addr(block_nb, offset) \
    (INODE_TABLE_SIZE + block_nb * DATA_BLOCK_SIZE) + (offset % DATA_BLOCK_SIZE)

int SM_FUNC("fileSys") mkfs(void) {
    printdebug("\t[basic-file-sys] mkfs - init the in-memory disk usage bitmap");
    int i;
    for (i = 0; i < NB_DATA_BLOCKS; i++)
        disk_usage[i] = false;
    
    printdebug("\t[basic-file-sys] mkfs - bulk erasing the disk");
    // disk_bulk_erase();
    disk_erase_sector(0);
    
    printdebug("\t[basic-file-sys] mkfs - init the on-disk inode-table in_use field");
    for (i = 1; i <= MAX_NB_INODES; i++) {
        disk_addr_t istart = get_istart(i);
        put_byte(istart + IN_USE_IDX, '\0');
    }
    return SUCCESS;
}

int SM_FUNC("fileSys") dump_inode_table() {
    printdebug_int("\t[basic-file-sys] dump_inode_table with INODE_SIZE=%d", INODE_SIZE);
    printdebug_int(" MAX_NB_INODES=%d", MAX_NB_INODES);
    printdebug_int(" INODE_TABLE_SIZE=%d\n", INODE_TABLE_SIZE);
    
    unsigned char buf[INODE_TABLE_SIZE];
    disk_get_bytes(get_istart(1), buf, INODE_TABLE_SIZE);

    printdebug("-----------------------------------------");
    printdebug_str("inode_layout:\tIU| SIZE| SMID  P  D| SMID  P  D| B  B  B  B  D");
    int i, j;
    for (i = 0; i < MAX_NB_INODES; i++) {
        printdebug_int("\ninode_nb %d:\t", i+1);
        for (j = 0; j < INODE_SIZE; j++)
            printdebug_int("%02x ", buf[i*INODE_SIZE + j]);
	}
    printdebug("\n-----------------------------------------");
    return SUCCESS;
}

int SM_FUNC("fileSys") init_inode(inode_nb_t nb, sm_id sm, perm_t init_flags) {
    validate_inode_nb(nb);
    disk_addr_t istart = get_istart(nb);

    printdebug("\t[basic-file-sys] init_inode - validate inode is not already in use");
    int in_use;
    get_byte(istart + IN_USE_IDX, &in_use);
    if ((unsigned char) in_use != '\0') {
        printerr("\t[basic-file-sys] cannot initialize - inode_nb already in use");
        return FAILURE;
    }

    printdebug("\t[basic-file-sys] init_inode - marking as used");
    put_byte(istart + IN_USE_IDX, 'Y');
    
    printdebug("\t[basic-file-sys] init_inode - zeroing size");
    int size = 0;
    int idx = istart + SIZE_IDX;
    put_val(idx, int, size);
    
    printdebug("\t[basic-file-sys] init_inode - init specified permissions");
    idx = istart + ACL_ID_IDX(0);
    put_val(idx, sm_id, sm);
    idx = istart + ACL_PERM_IDX(0);
    put_val(idx, perm_t, init_flags);

    printdebug("\t[basic-file-sys] init_inode - disable all other permissions");
    int i;
    perm_t nil_perm = F_NIL;
    for (i = 1; i < ACL_LENGTH; i++) {
        idx = istart + ACL_PERM_IDX(i);
        put_val(idx, perm_t, nil_perm);
    }

    printdebug("\t[basic-file-sys] init_inode - init nil data blocks");
    block_nb_t nil_block = NIL_BLOCK_NB;
    for (i = 0; i < MAX_NB_DATA_BLOCKS; i++) {
        idx = istart + BLOCK_NB_IDX(i);
        put_val(idx, block_nb_t, nil_block);
    }

    return SUCCESS;
}

int SM_FUNC("fileSys") free_inode(inode_nb_t nb) {
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    printdebug("\t[basic-file-sys] free_inode - free all associated data blocks");
    int i, idx;
    for (i = 0; i < MAX_NB_DATA_BLOCKS; i++) {
        block_nb_t block_nb;
        idx = istart + BLOCK_NB_IDX(i);
        get_val(idx, block_nb_t, &block_nb);
        if (block_nb != NIL_BLOCK_NB) {
            printdebug_int("\t[basic-file-sys] freeing logical block %d\n", i);
            free_data_block(block_nb);
        }
    }
    
    printdebug("\t[basic-file-sys] free_inode - mark the inode as unused");
    put_byte(istart + IN_USE_IDX, 'N');
    
    return SUCCESS;
}

int SM_FUNC("fileSys") get_acl(inode_nb_t nb, sm_id sm) {
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    printdebug("\t[basic-file-sys] get_acl - search the access control list");
    int i, idx;
    for (i = 0; i < ACL_LENGTH; i++) {
        sm_id id;
        idx = istart + ACL_ID_IDX(i);
        get_val(idx, sm_id, &id);
        if (id == sm) {
            perm_t rv;
            idx = istart + ACL_PERM_IDX(i);
            get_val(idx, perm_t, &rv);
            return rv;
        }
    }
    printdebug("\t[basic-file-sys] get_acl - no entry in the access control list");
    return F_NIL;
}

int SM_FUNC("fileSys") set_acl(inode_nb_t nb, sm_id sm, int flags) {
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    // search the access control list; override any existing permission
    printdebug("\t[basic-file-sys] set_acl - searching for existing ACL entry");
    int i, idx;
    for (i = 0; i < ACL_LENGTH; i++) {
        sm_id id;
        idx = istart + ACL_ID_IDX(i);
        get_val(idx, sm_id, &id);
        if (id == sm) {
            idx = istart + ACL_PERM_IDX(i);
            put_val(idx, perm_t, flags);
            return SUCCESS;
        }
    }
    // no existing entry in the access control list; try to create new one
    //TODO (maybe ACL size field in inode struct --> also allows faster checking)
    printdebug("\t[basic-file-sys] set_acl - searching new free ACL entry");
    for (i = 0; i < ACL_LENGTH; i++) {
        perm_t p;
        idx = istart + ACL_PERM_IDX(i);
        get_val(idx, perm_t, &p);
        if (p == F_NIL) {
            idx = istart + ACL_ID_IDX(i);
            put_val(idx, sm_id, sm);
            idx = istart + ACL_PERM_IDX(i);
            put_val(idx, perm_t, flags);
            return SUCCESS;
        }
    }
    printdebug("\t[basic-file-sys] set_acl - no more free ACL entries");    
    return FAILURE;
}

int SM_FUNC("fileSys") get_size(inode_nb_t nb) {
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    printdebug("\t[basic-file-sys] get_size: fetching size from disk");
    int rv;
    int idx = istart + SIZE_IDX;
    get_val(idx, int, &rv);
    return rv;
}

int SM_FUNC("fileSys") get_char(inode_nb_t nb, unsigned int offset) {
    if (offset >= MAX_FILE_LENGTH)
        return EOF; //XXX also eof if over inode->size
    validate_inode(nb);
    
    // 1. get the nb of the block containing the requested location TODO cache
    printdebug("\t[basic-file-sys] get_char - fetching block_nb containing offset location");
    int block_idx = offset / DATA_BLOCK_SIZE;
    block_nb_t block_nb;
    int idx = get_istart(nb) + BLOCK_NB_IDX(block_idx);
    get_val(idx, block_nb_t, &block_nb);
    if (block_nb == NIL_BLOCK_NB)
        return EOF;

    // 2. calculate the disk address and fetch the requested char
    printdebug("\t[basic-file-sys] get_char - fetching requested byte");    
    disk_addr_t addr = get_disk_addr(block_nb, offset);
    unsigned char rv;
    get_byte(addr, &rv);
    
    return rv;
}

int SM_FUNC("fileSys") put_char(inode_nb_t nb, unsigned int offset, unsigned char c) {
    if (offset >= MAX_FILE_LENGTH)
        return EOF;
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    // 1. get the nb of the block containing the requested location TODO cache
    printdebug("\t[basic-file-sys] put_char - fetching block_nb containing offset location");
    int block_idx = offset / DATA_BLOCK_SIZE;
    block_nb_t block_nb;
    int idx = istart + BLOCK_NB_IDX(block_idx);
    get_val(idx, block_nb_t, &block_nb);
    
    // 2. create new data block if necessary
    if (block_nb == NIL_BLOCK_NB) {
        printdebug("\t[basic-file-sys] put_char - allocating new data block");    
        block_nb = alloc_data_block();
        if (block_nb == NIL_BLOCK_NB) {
            printerr("\t[basic-file-sys] put_char - could not allocate new data block");
            return EOF;
        }
        idx = istart + BLOCK_NB_IDX(block_idx);
        put_val(idx, block_nb_t, block_nb);
    }

    // 3. calculate the disk address and write out the char
    printdebug("\t[basic-file-sys] put_char - writing out the byte");
    disk_addr_t addr = get_disk_addr(block_nb, offset);
    put_byte(addr, c);
    
    printdebug("\t[basic-file-sys] put_char - incrementing inode-size");
    int size;
    idx = istart + SIZE_IDX;
    get_val(idx, int, &size);
    size++;
    put_val(idx, int, size);
    
    return c;
}

int SM_FUNC("fileSys") back_end_print_file(inode_nb_t nb, unsigned int pos) {
    validate_inode(nb);
    disk_addr_t istart = get_istart(nb);
    
    int size = get_size(nb);
    
    // walk the data blocks; i is the nb of chars read so far
    int i;
    block_nb_t cur_block_nb;
    int cur_block_idx = 0;
    block_nb_t pos_block = NIL_BLOCK_NB;
    for (i = 0; i < size; i++) {
        if ((i % DATA_BLOCK_SIZE) == 0) {
            char *tabs = (pos_block == cur_block_nb)? "\"\t" : "\"\t\t";
            // fetch the next data block number
            int idx = istart + BLOCK_NB_IDX(cur_block_idx);
            get_val(idx, block_nb_t, &cur_block_nb);
            cur_block_idx++;
            if (cur_block_nb == NIL_BLOCK_NB) {
                printerr("print_file: could not fetch next data block");
                return EOF;
            }
            if (i > 0) {
                printf_str(tabs);
                printf_int("size = %d\n", DATA_BLOCK_SIZE);
            }
            printf_int("data block number %d:\t\"", cur_block_nb);
        }
        
        if (i == pos) {
            printf_cyan("[FP->]");
            pos_block = cur_block_nb;
        }
        disk_addr_t addr = get_disk_addr(cur_block_nb, i);
        unsigned char rv;
        get_byte(addr, &rv);
        putchar(rv);
    }
    
    char *tabs = "\t\t";
    if (pos_block == NIL_BLOCK_NB) {
        printf_cyan("[FP->]");
        tabs = "\t";
    }
    putchar('"');

    // align the last file chunk with spaces on stdout
    int last_size = size - (cur_block_idx-1)*DATA_BLOCK_SIZE;
    for (i = 0; i < DATA_BLOCK_SIZE-last_size; i++)
        putchar(' ');
    printf_str(tabs);
    printf_int("size = %d\n", last_size);
    printf_int("\tFILE_POS = %d\t", pos);
    printf_int("\tFILE_SIZE = %d", size);
    
    printf_str("\n\tACL = [ ");
    
    // walk the access control list
    int idx;
    for (i = 0; i < ACL_LENGTH; i++) {
        sm_id id;
        idx = istart + ACL_ID_IDX(i);
        get_val(idx, sm_id, &id);
        if (i > 0) printf_str(" ; ");
        printf_int("(%d, ", id);
        
        perm_t p;
        idx = istart + ACL_PERM_IDX(i);
        get_val(idx, perm_t, &p);
        printf_int("0x%x)", p);
    }
    printf_str(" ]\n");
    
    return SUCCESS;
}

//##################### HELPER FUNCTION DEFS ##############################

block_nb_t SM_FUNC("fileSys") alloc_data_block(void) {
    unsigned int i;
    for (i = 0; i < NB_DATA_BLOCKS; i++)
        if (!disk_usage[i]) {
            disk_usage[i] = true;
            return i;
        }
    // no free block found
    return NIL_BLOCK_NB;
}

void SM_FUNC("fileSys") free_data_block(block_nb_t idx) {
    if (idx >= NB_DATA_BLOCKS)
        return;
    disk_usage[idx] = false;
}

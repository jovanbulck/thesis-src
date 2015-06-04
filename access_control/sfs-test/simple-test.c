#include "../../common.h"
#include "../sfs/sfs.h"

#define EXIT            while (1) {}
#define ASSERT(cond) \
do { \
    if(!(cond)) \
    { \
    puts("assertion failed; exiting..."); \
    EXIT \
    } \
} while(0)

#define FILE_ONE    'a'
#define FILE_TWO    'b'

DECLARE_SM(clientA, 0x1234);
#define A           "[clientA] "
#define printa(str) printdebug(A str)
#define A_ID        2                   //TODO assert

DECLARE_SM(clientB, 0x1234);
#define B           "[clientB] "
#define printb(str) printdebug(B str)
#define B_ID        3

int SM_DATA("clientB") fdb1;
int SM_DATA("clientB") fdb2;

void SM_ENTRY("clientB") open_b_files(void)
{
    printdebug_int(B "I was called by %d ", sancus_get_caller_id());
    printdebug_int("and I have id %d\n", sancus_get_self_id());

    sfs_ping();

    printb("validating bunch of files");
    sfs_attest(FILE_ONE, clientA.id);
    sfs_attest(FILE_TWO, clientA.id);
    
    printb("opening bunch of files");
    // SFS_OPEN_EXISTING to be sure the file already exists (also sure through attest)
    fdb1 = sfs_open(FILE_ONE, SFS_READ, SFS_OPEN_EXISTING);
    fdb2 = sfs_open(FILE_TWO, SFS_READ | SFS_WRITE, SFS_OPEN_EXISTING);
}

void SM_ENTRY("clientB") close_b_files(void)
{
    printb("closing bunch of files files");
    sfs_close(fdb1);
    sfs_close(fdb2);
}

void SM_FUNC("clientB") write_and_read(int fd, char a, char b)
{
    sfs_seek(fd, 0, SFS_SEEK_SET);
    sfs_putc(fd, a);
    sfs_putc(fd, b);
    
    sfs_seek(fd, 0, SFS_SEEK_SET);
    printdebug_int(B "the char is '%c'\n", sfs_getc(fd));
    printdebug_int(B "the char is '%c'\n", sfs_getc(fd));
}

void SM_ENTRY("clientB") access_b_files(void)
{
    printb("accessing bunch of files");
    sfs_getc(fdb1);

    printb("writing 'J0' to fdb2"); 
    write_and_read(fdb2, 'J', '0');
    
    printb("overwriting the chars with 'AZ'");
    write_and_read(fdb2, 'A', 'Z');
 
    printb("one more time");   
    write_and_read(fdb2, 'A', 'Z');
}

int SM_FUNC("clientA") create_file(filename_t name, int b_flags)
{
    printa("opening a new file");
    //SFS_CREATOR to be sure a new file is created
    int fd = sfs_open(name, SFS_CREATOR, 100);
    
    printa("assigning B permissions");
    sfs_chmod(name, B_ID, b_flags);
    
    return fd;
}

void SM_ENTRY("clientA") run_clientA(void)
{
    int fd1, fd2;
    printdebug_int(A "Hi from run_clientA, I have id %d\n", sancus_get_self_id());
    ASSERT(A_ID == sancus_get_self_id());
    ASSERT(B_ID == sancus_get_id(clientB.public_start));
    
    sfs_ping();
    //sfs_dump();
    
    fd1 = create_file(FILE_ONE, SFS_READ);
    fd2 = create_file(FILE_TWO, SFS_READ | SFS_WRITE);
    sfs_dump();
    
    printa("opening and accessing SM B files");
    open_b_files();
    access_b_files();
    
    printa("revoking B permissions");
    // will also close B files...
    sfs_chmod(FILE_ONE, B_ID, SFS_NIL);
    sfs_chmod(FILE_TWO, B_ID, SFS_NIL);
    sfs_dump();

    /*
    printa("accessing B files (shouldn't work)");
    access_b_files();
    
    printa("closing b files");
    close_b_files();
    sfs_dump();
    */
    
    printa("closing a files");
    sfs_close(fd1);
    sfs_close(fd2);
    
    printa("removing files");
    sfs_remove(FILE_ONE);
    sfs_remove(FILE_TWO);
    sfs_dump();

    printa("exiting");    
}


int main()
{
    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    puts("\n---------------\nmain started");
    printdebug_int("[main] I have id %d\n", sancus_get_self_id());
    
    sancus_enable(&sfs);
    sancus_enable(&clientA);
    sancus_enable(&clientB);    

    /*    
    puts("reading");
    int jo = fdb1;
    puts("writing");
    fdb1 = 5;
    */

    run_clientA();
    
    puts("[main] exiting\n-----------------");
    EXIT
}

void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    puts("\nVIOLATION HAS BEEN DETECTED: stopping the system");
    while (1) {}
}

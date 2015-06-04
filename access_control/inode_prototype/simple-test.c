#include "common.h"
#include "file-sys.h"

DECLARE_SM(clientA, 0x1234);
#define THE_FILE    5

void SM_ENTRY("clientA") run_clientA(void) {
    printdebug_int("[clientA] I have id %d\n", sancus_get_self_id());
    
    printdebug_int("[clientA] creating a file with inode_nb %d\n", THE_FILE);
    f_creat(THE_FILE);
    
    printdebug("[clientA] dumping inode table after f_creat");
    fs_dump_inode_table();
    
    printdebug("[clientA] trying to create the file again; should fail");
    f_creat(THE_FILE);
    
    printdebug("[clientA] opening the file");
    int fd = f_open(THE_FILE, F_RD | F_WR);
    
    printdebug("[clientA] writing some chars");
    int i;
    for (i = 0; i < 10; i++)
        f_putc(fd, 'A'+i);
    
    printdebug("[clientA] getting them back");
    f_seek_start(fd);
    for (i = 0; i < 10; i++)
        printdebug_int("it is '%c'\n", f_getc(fd));
    
    printdebug("[clientA] closing the file");
    f_close(fd);
    
    printdebug("[clientA] unlinking the file");
    f_unlink(THE_FILE);
}

int main()
{
    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    puts("\n---------------\nmain started");
    printdebug_int("I have id %d\n", sancus_get_self_id());

    sancus_enable(&fileSys);
    sancus_enable(&clientA);

    printdebug("[simple-test] making new file system");
    f_mkfs();
    
    printdebug("[simple-test] dumping init inode table");
    fs_dump_inode_table();

    printdebug("[simple-test] running client A");
    run_clientA();    

    puts("main done\n-----------------");
    while (1) {}
}

void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    puts("\nVIOLATION HAS BEEN DETECTED: stopping the system");
    while (1) {}
}

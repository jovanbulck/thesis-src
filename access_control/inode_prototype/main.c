#include "common.h"
#include "file-sys.h"

extern struct SancusModule diskDriver;
extern struct SancusModule fileSys;

int main(void) {

    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    
    puts("\n---------------");
    puts("main started");
    printdebug_int("I have id %d\n", sancus_get_self_id());

    sancus_enable(&fileSys);
    sancus_enable(&diskDriver);
    
    printf("making new file system\n");
    f_mkfs();
    printf("done\n");

    inode_nb_t inb = 1;
    printf("creating file %d \n", inb);
    f_creat(inb);
    printf("done\n");
    
    printf("now opening rw\n");
    FD fd = f_open(inb, F_RD | F_WR);
    printf("done\n");
    
    int i;
    for (i = 0; i < 5; i++) {
        printf("now writing char '%c'\n", 'A' + i);
        if (f_putc(fd, 'A' + i) == EOF) {
            printerr_int("EOF on write with i = %d\n",  i);
            break;
        }
        printf("done\n");
    }
    
    
    printf("now assigning rights to SM 1\n");
    f_chmod(inb, 1, F_RD);
    printf("done; now calling SM 1\n");
    call_SM1(inb);
    //sancus_set_caller_id(0);
    
    printf("now closing\n");
    f_close(fd);
    printf("done\n");
    
    printf("now unlinking\n");
    f_unlink(inb);
    printf("-----------------------------------\n main done\n");
    
    while(1) {}
}


void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    puts("\nVIOLATION HAS BEEN DETECTED: stopping the system");
    while (1) {}
}

/*
 * shared_mem_test: SM client1 writes NB_CHARS bytes into a file, shared with SM client2
 *  client 2 reads all the bytes in the file and prints them on stdout
 * @note: compile with -DNODEBUG to eliminate the overhead of the printdebug statements
 */
#include "common.h"
#include "file-sys.h"
#include <sancus_support/tsc.h>

extern struct SancusModule diskDriver;
extern struct SancusModule fileSys;
DECLARE_SM(client1, 0x1234);
DECLARE_SM(client2, 0x1234);

#define NB_CHARS        100

#define SHARED_FILE_NB  1
#define DUMMY_FILE_1    5
#define DUMMY_FILE_2    6

#define client1_id      3   //XXX is this possible dynamically??
#define client2_id      4

void SM_ENTRY("client2") run_client2(void) {
    sm_id id = sancus_get_caller_id();
    printdebug_int("Hi, I am 'client2' with id %d", sancus_get_self_id());
    printdebug_int(" and was called by %d\n", id);
    
    printdebug("asking the file-system to open the file in read-only mode");
    FD d = f_open(SHARED_FILE_NB, F_RD);
    printdebug_int("Done: the file descriptor is %d\n", d);
   
    printdebug("Now reading and printing all chars in the file");
    char c;
    int i;
    //while ((c = f_getc(d)) != EOF)
    for (i = 0; i < NB_CHARS; i++) {
        c = f_getc(d);
        //putchar(c);    
    }
    //putchar('\n');
    
    printdebug("^^ these are all the chars; now closing the file");
    
    f_close(d);
}

void SM_ENTRY("client2") client2_trigger_init(void) {
    printdebug("client2: triggering initial client struct creation");
    f_creat(DUMMY_FILE_2);
    FD d = f_open(DUMMY_FILE_2, F_RD);
}

void SM_ENTRY("client1") run_client1(void) {
    sm_id id = sancus_get_caller_id();
    printdebug_int("Hi, I am 'client1' with id %d", sancus_get_self_id());
    printdebug_int(" and was called by %d\n", id);
    
    printdebug("Asking the file system to create a file");
    f_creat(SHARED_FILE_NB);
    
    printdebug("Asking the file system to open the file");
    FD d = f_open(SHARED_FILE_NB, F_RD | F_WR);
    printdebug_int("Done: the file descriptor is %d\n", d);
    
    printdebug_int("Now writing %d chars\n", NB_CHARS);
    int i;
    for (i = 0; i < NB_CHARS/2; i++)
        f_putc(d, 'A');
    
    FD d2 = f_open(DUMMY_FILE_1, F_RD | F_WR);
    f_putc(d2, 'j');
    
    for (i = 0; i < NB_CHARS/2; i++)
        f_putc(d, 'A');
    
    
    printdebug("done; now giving read only access to client2");
    f_chmod(SHARED_FILE_NB, client2_id, F_RD);
    
    run_client2();
    
    
    printdebug("now printing the file");
        print_file(d);
    f_seek(d, 15);
    print_file(d);
    
    print_file(d2);
    
    printdebug("now closing and unlinking the file");
    f_close(d);
    f_unlink(SHARED_FILE_NB);
}

void SM_ENTRY("client1") client1_trigger_init(void) {
    printdebug("client1: triggering initial client struct creation");
    f_creat(DUMMY_FILE_1);
    FD d = f_open(DUMMY_FILE_1, F_RD);
}

void write_unprotected_chars(char *buf) {
    int i;
    printdebug("now producing the chars");
    for (i = 0; i < NB_CHARS; i++)
        buf[i] = 'A';
}

void read_unprotected_chars(char *buf) {
    printdebug("now consuming the chars");
    int i; char c;
    for (i = 0; i < NB_CHARS; i++)
        c = buf[i];
    //putchar(buf[i]);
    //putchar('\n');
    return;
}

void run_unprotected_shared_mem() {
    char *buf = malloc(sizeof(char) * NB_CHARS);
    
    write_unprotected_chars(buf);
    read_unprotected_chars(buf);
    
    free(buf);
}

int main()
{
    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    puts("\n---------------");
    puts("main started");
    printdebug_int("I have id %d\n", sancus_get_self_id());

    sancus_enable(&fileSys);
    sancus_enable(&diskDriver);
    sancus_enable(&client1);
    sancus_enable(&client2);

 
    printf("making new file system\n");
    f_mkfs();
    printf("done\n");


    /*int i;
    for (i = 100; i < 300; i+= 100) {*/
        //NB_CHARS += i;
        printf("\tnb_chars is %d\n", NB_CHARS);
        
        tsc_t ts1 = tsc_read();    
        client1_trigger_init();
        client2_trigger_init();
        tsc_t ts2 = tsc_read();
        tsc_t diff1 = ts2 - ts1;
        printf("the number of cycles for init is %llu\n", diff1);
        
        puts("(triggered init) ############################");

        ts1 = tsc_read();    
        run_client1();
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
    
        puts("############################");
    
        ts1 = tsc_read();
        run_unprotected_shared_mem();
        ts2 = tsc_read();
        tsc_t diff2 = ts2 - ts1;
    
        printf("The number of cycles for the protected shared mem is %llu\n", diff1);
        printf("The number of cycles for unprotected shared mem is %llu\n", diff2);   
        printf("The overhead nb_cycles is %llu\n", diff1 - diff2);
    //}
    
    puts("main done");
    while (1) {}
}

void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    puts("\nVIOLATION HAS BEEN DETECTED: stopping the system");
    while (1) {}
}

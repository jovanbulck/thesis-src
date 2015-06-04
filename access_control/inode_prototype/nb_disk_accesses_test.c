#include "common.h"
#include "file-sys.h"
#include <sancus_support/tsc.h>

//extern struct SancusModule diskDriver;
extern struct SancusModule fileSys;
//DECLARE_SM(client1, 0x1234);
DECLARE_SM(client2, 0x1234);

#define THE_FILE        1
#define ANOTHER_FILE    2
#define YET_ANOTHER_FILE    3
#define NB_CHARS        10

#define client1_id      3   //XXX is this possible dynamically??
#define client2_id      3

void SM_ENTRY("client2") run_client2(void) {
    sm_id id = sancus_get_caller_id();
    printdebug_int("Hi, I am 'client2' with id %d", sancus_get_self_id());
    printdebug_int(" and was called by %d\n", id);
    
    printdebug("asking the file-system to open the file in read-only mode");
    FD d = f_open(THE_FILE, F_RD);
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

void run_client1(void) {
    sm_id id = sancus_get_caller_id();
    printdebug_int("Hi, I am 'client1' with id %d", sancus_get_self_id());
    printdebug_int(" and was called by %d\n", id);
    
    printdebug("Asking the file system to create a file");
    int i;
    tsc_t ts1, ts2, diff1;
    for (i = 0 ; i <  5; i++) {
        ts1 = tsc_read();    
        f_creat(THE_FILE);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for creat THE_FILE is %llu\n", diff1);
        ts1 = tsc_read();
        f_unlink(THE_FILE);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for unlink is %llu\n", diff1);
    }
    
    f_creat(THE_FILE);
    
    // create and open another file, so that client isn't removed
    f_creat(ANOTHER_FILE);
    f_open(ANOTHER_FILE, F_RD);
    
    printdebug("Asking the file system to open the file");
    FD d;
    for (i=0; i < 5; i++) {
        ts1 = tsc_read();    
        d = f_open(THE_FILE, F_RD | F_WR);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for open THE_FILE is %llu\n", diff1);
        ts1 = tsc_read();
        f_close(d);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for close is %llu\n", diff1);
    }  
    printdebug_int("Done: the file descriptor is %d\n", d);
    
    d = f_open(THE_FILE, F_RD | F_WR);
    
    printdebug_int("Now writing %d chars\n", NB_CHARS);
    for (i = 0; i < NB_CHARS; i++) {
        ts1 = tsc_read();
        f_putc(d, 'A');
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for putc is %llu\n", diff1);
    }
    
    ts1 = tsc_read();
    f_seek(d,0);
    ts2 = tsc_read();
    diff1 = ts2 - ts1;
    printf("the number of cycles for f_seek is %llu\n", diff1);
    for (i = 0; i < NB_CHARS; i++) {
        ts1 = tsc_read();
        f_getc(d);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for getc is %llu\n", diff1);
    }
    
    printdebug("done; now giving read only access to client2");
    
    for (i = 0; i < 5; i++) {
        ts1 = tsc_read();
        f_chmod(THE_FILE, client2_id, F_RD);
        ts2 = tsc_read();
        diff1= ts2 - ts1;
        printf("the number of cycles for chmod is %llu\n", diff1);
    }    
    
    //run_client2();
    
    printdebug("now printing the file");
    print_file(d);
    f_seek(d, 15);
    print_file(d);
    
    printdebug("now closing and unlinking the file");
    f_close(d);
    f_unlink(THE_FILE);
}

void  __attribute__((noinline)) call_dummy(void) {
    return;
}

int main()
{
    WDTCTL = WDTPW | WDTHOLD;
    uart_init();
    puts("\n---------------");
    puts("main started");
    printdebug_int("I have id %d\n", sancus_get_self_id());

    sancus_enable(&fileSys);
    //sancus_enable(&diskDriver);
    //sancus_enable(&client1);
    sancus_enable(&client2);

    /*int i;
    for (i = 100; i < 300; i+= 100) {*/
        //NB_CHARS += i;
        
        int i;
        tsc_t ts1, ts2, diff1;
        
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        char a = 'a';
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for char assignment is %llu\n", diff1); 
        }
        
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        int i = 5;
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for int assignment is %llu\n", diff1); 
        }
        
        char *b;
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        b = malloc(1);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for malloc(1) is %llu\n", diff1);
        
        ts1 = tsc_read();
        free(b);
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for free(b) is %llu\n", diff1);
        }
        
        b = malloc(1);
        
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        *b = 'a';
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for assign to malloced mem is %llu\n", diff1); 
        }
        
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        char c = *b;
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for read from malloced mem is %llu\n", diff1); 
        }
        
        for (i =0; i < 2; i++) {
        ts1 = tsc_read();
        call_dummy();
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for calling a dummy fct is %llu\n", diff1); 
        }
        
        for (int i = 0; i < 2; i++) {
            ts1 = tsc_read();
            ping();
            ts2 = tsc_read();
            diff1 = ts2 - ts1;
            printf("the number of cycles for ping is %llu\n", diff1);
        }
        
        for (i = 0; i < 5; i++) {
        printf("making new file system\n");
        ts1 = tsc_read();    
        f_mkfs();
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
        printf("the number of cycles for mkfs is %llu\n", diff1);
        }
        
        puts("(triggered init) ############################");

        ts1 = tsc_read();    
        run_client1();
        ts2 = tsc_read();
        diff1 = ts2 - ts1;
    
        printf("The number of cycles for run_client1() is %llu\n", diff1);
    
        /*puts("############################");
    
        ts1 = tsc_read();
        run_unprotected_shared_mem();
        ts2 = tsc_read();
        tsc_t diff2 = ts2 - ts1;
    
        printf("The number of cycles for the protected shared mem is %llu\n", diff1);
        printf("The number of cycles for unprotected shared mem is %llu\n", diff2);   
        printf("The overhead nb_cycles is %llu\n", diff1 - diff2);*/
    //}
    
    puts("main done");
    while (1) {}
}

void __attribute__((interrupt(SM_VECTOR))) the_isr(void) {
    puts("\nVIOLATION HAS BEEN DETECTED: stopping the system");
    while (1) {}
}

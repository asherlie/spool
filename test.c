#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "sp.h"

void* tst(void* arg){
    usleep((random() % 100000) + 1000);
    printf("%i\n", *((int*)arg));

    return NULL;
}

int main(){
    struct spool_t s;
    int* arg, count, buf[1000];

    init_spool_t(&s, 30);
    set_routine_target(&s, 178);
    for(int i = 0; i < 400; ++i){
        buf[i] = i;
        arg = buf+i;
        exec_routine(&s, tst, arg);
    }
    /*destroy_spool_t(&s);*/
    await_routine_target(&s);
    return 0;

    count = 0;
    while(getchar()){
        if(count++ %2 == 0)
            pause_exec(&s);
        else
            resume_exec(&s);
    }

    pause_exec(&s);
    /*usleep(10000);*/
    /*puts("PAUSING FOR");*/
    for(int i = 0; i < 5; ++i){
        printf("%i", i);
        fflush(stdout);
        usleep(250000);
        putchar('.');
        fflush(stdout);
        usleep(250000);
        putchar('.');
        fflush(stdout);
    }
    puts("5");
    resume_exec(&s);
    pause_exec(&s);
    resume_exec(&s);
    pause_exec(&s);
    usleep(10000000);
}

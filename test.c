#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "sp.h"

void* tst(void* arg){
    /*usleep((random() % 100000) + 1000);*/
    usleep(500000);
    printf("%i\n", *((int*)arg));

    return NULL;
}

int main(int a, char** b){
    if(a <= 1)return 0;
    struct spool_t s;
    /*int* arg, count, buf[10000];*/
    int* arg, count, count_to = atoi(b[1]),
         n_threads = (a > 2) ? atoi(b[2]) : 1;
    int* buf = malloc(sizeof(int)*count_to);

    printf("nt: %i ct: %i\n", n_threads, count_to);

    init_spool_t(&s, n_threads);
    set_routine_target(&s, count_to);

    /* init_spool_t() takes time to set up threads
     * TODO: should it possibly only return once
     * at least one thread has become ready?
     *
     * could write a separate function called 
     * await_thread_ready()
     * and add another pthread conditional
     */
    usleep(1000);

    for(int i = 0; i < count_to; ++i){
        buf[i] = i;
        arg = buf+i;
        exec_routine(&s, tst, arg);
    }
    /*destroy_spool_t(&s);*/
    await_routine_target(&s);

    free(buf);

    return 0;

    count = 0;
    while(getchar()){
        if(count++ %2 == 0)
            pause_exec(&s);
        else
            resume_exec(&s);
    }
    return 0;
}

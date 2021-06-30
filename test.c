#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sp.h"

void* tst(void* arg){
    /*usleep((random() % 100000) + 1000);*/
    usleep(500000);
    printf("%i\n", *((int*)arg));

    return NULL;
}

void* single_tst(void* arg){
    (void)arg;
    usleep(500000);
    puts("single test has been run");

    return NULL;
}

struct shared{
    char* mem;
};

void* init_mem(void* shared){
    struct shared* s = shared;
    s->mem = calloc(10, 1);

    return NULL;
}

void* set_mem(void* shared){
    struct shared* s = shared;
    strncpy(s->mem, "string", 7);

    return NULL;
}

void* print_mem(void* shared){
    struct shared* s = shared;
    puts(s->mem);

    return NULL;
}

int main(int a, char** b){
    if(a <= 1)return 0;
    struct spool_t s;
    /*int* arg, count, buf[10000];*/
    int* arg, count, count_to = atoi(b[1]),
         n_threads = (a > 2) ? atoi(b[2]) : 1;
    int* buf = malloc(sizeof(int)*count_to);
    struct shared sh_mem;
    struct routine* r[20];
    (void)r;

    printf("nt: %i ct: %i\n", n_threads, count_to);

    init_spool_t(&s, n_threads);

    /* the following would seg fault
     * because there's no guarantee that
     * each routine will finish in order
     */
    #if !1
    exec_routine(&s, init_mem, &sh_mem, 0);
    exec_routine(&s, set_mem, &sh_mem, 0);
    exec_routine(&s, print_mem, &sh_mem, 0);
    #endif
    /* await_single_routine() is provided for
     * situations like this, where a strict
     * ordering of routines is necessary
     * this could be rewritten as the following
     */

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
        exec_routine(&s, tst, arg, 0);
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

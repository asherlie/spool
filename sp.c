#include <stdio.h>
#include <stdlib.h>
#include "sp.h"

void init_tq(struct thread_queue* tq, int n_threads){
    tq->threads = malloc(sizeof(struct thread)*n_threads);
    tq->n_threads = n_threads;
}

void init_spool_t(struct spool_t* s, int n_threads){
    pthread_cond_init(&s->spool_up, NULL);
}

int main(){
}

#include <stdio.h>
#include <stdlib.h>
#include "sp.h"

struct routine* pop_rq(struct routine_queue* rq){
    struct routine* ret = NULL;

    pthread_mutex_lock(&rq->rlock);
    ret = rq->r;
    if(ret)rq->r = rq->r->next;
    pthread_mutex_unlock(&rq->rlock);

    return ret;
}

void* await_instructions(void* v_rq){
    struct routine_queue* rq = v_rq;
    struct routine* r;

    pthread_mutex_t tmplck;
    pthread_mutex_init(&tmplck, NULL);
    while(1){
        pthread_mutex_lock(&tmplck);
        /* it's not guaranteed that only one
         * thread will be woken up
         *
         * to safeguard against this we will attempt
         * to acquire a lock once we're woken up
         * to confirm that only one thread will run
         *
         * HMM - this may not be an issue
         * if too many threads are woken up, IE. 
         * n_threads_awake > n_available_routines
         * some of the threads are guaranteed
         * to re-block on cond_t
         * because pop_rq() is threadsafe
         */
        pthread_cond_wait(&rq->spool_up, &tmplck);

        if(!(r = pop_rq(rq)))continue;

        r->func(r->arg);

        pthread_cond_signal(&rq->spool_up);
        /*pthread_cond_broadcast*/
    }
}

void init_tq(struct thread_queue* tq, int n_threads, struct routine_queue* rq){
    tq->threads = malloc(sizeof(struct thread)*n_threads);
    tq->n_threads = n_threads;
    for(int i = 0; i < n_threads; ++i){
        tq->threads[i].thread_id = i;
        pthread_create(&tq->threads[i].pth, NULL, await_instructions, rq);
    }
}

void init_rq(struct routine_queue* rq){
    pthread_cond_init(&rq->spool_up, NULL);
    pthread_mutex_init(&rq->rlock, NULL);
}

void init_spool_t(struct spool_t* s, int n_threads){
    /*pthread_cond_init(&s->spool_up, NULL);*/
    init_rq(s->rq);
    init_tq(s->tq, n_threads, s->rq);
}

int main(){
}

#include <stdio.h>
#include <stdlib.h>

#include "sp.h"

struct routine* pop_rq(struct routine_queue* rq){
    struct routine* ret;

    pthread_mutex_lock(&rq->rlock);

    ret = rq->first;
    if(ret)rq->first = rq->first->next;

    pthread_mutex_unlock(&rq->rlock);

    return ret;
}

/* this should only be called 
 * when n-1 threads have exited
 *
 * MEANT TO BE CALLED BY await_instructions()
 */
void destroy_rq(struct routine_queue* rq){
    /* ~free rq here~ */
    struct routine* p = rq->first;
    if(p){
        for(struct routine* r = p->next; r; r = r->next){
            free(p);
            p = r;
        }
        free(p);
    }
}

void destroy_tq(struct thread_queue* tq){
    free(tq->threads);
}

void join_tq(struct thread_queue* tq){
    for(int i = 0; i < tq->n_threads; ++i){
        pthread_join(tq->threads[i].pth, NULL);
    }
}

/* destroy_rq() will initiate the
 * process of destroying the rq
 * by first killing all threads
 *
 * destroy_rq() will be called by await_instructions()
 *
 * destroy_spool_t() first sets flag to R_EXIT
 * it then waits for all threads to exit once
 * they're done with their current routine
 * R_EXIT will lead to destroy_rq() to be called
 */
void destroy_spool_t(struct spool_t* s){
    s->rq.flag = R_EXIT;
    join_tq(&s->tq);
    destroy_tq(&s->tq);
}

void* await_instructions(void* v_rq){
    struct routine_queue* rq = v_rq;
    struct routine* r;
    /* if this thread was able to run a routine
     * in the previous iteration, we won't block
     */
    _Bool prev_run = 0;

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
        if(!prev_run && rq->flag != R_EXIT){
            pthread_cond_wait(&rq->spool_up, &tmplck);
        }

        pthread_mutex_unlock(&tmplck);

        /* if we've met our target, initiate exit
         * TODO: why doesn't this work with
         * pauses/resumes between
         */
        /*if(rq->routines_completed == rq->r_target){*/
        /* routines_completed may never be equal to 
         * r_target - it's possible that multiple
         * threads increment routines_completed before
         * this is evaluated
         */

        /* == should be used for now. if >= needs to be
         * used, then r_target should be initialized
         * to INT_MAX rather than -1
         */
        /*if(rq->routines_completed >= rq->r_target){*/
        if(rq->routines_completed == rq->r_target){
            /*rq->flag = R_PAUSE;*/
            rq->flag = R_EXIT;
        }

        if(rq->flag == R_EXIT){
            pthread_mutex_destroy(&tmplck);

            /* just in case a thread has been waiting since
             * before R_EXIT has been set
             */
            pthread_cond_broadcast(&rq->spool_up);
            /* TODO:
             * once this is equal to n_threads, we can
             * safely destroy the entire queue
            */
            /*if(++rq->exited == rq->n_threads) ;*/
            /* the last thread to exit is responsible for
             * destroying the rq
             */
            if(!(--rq->running_threads)){
                destroy_rq(rq);
            }
            return NULL;
        }

        /* if we're meant to pause or we couldn't pop
         * a routine, re-wait
         */
        if(rq->flag == R_PAUSE || !(r = pop_rq(rq))){
            prev_run = 0;
            continue;
        }

        /* if we were able to succesfully pop a routine,
         * we'll attempt to do the same without pausing
         * next iteration
         */
        prev_run = 1;

        r->func(r->arg);
        free(r);
        ++rq->routines_completed;

        /* in case a thread is finishing up func()
         * as we're re-waiting to pause
         *
         * TODO: is this redundant?
         * if we're in R_PAUSE, we don't care if we get woken
         * up, we'll go back to sleep immediately with the
         * `continue`
         * still saves cycles though
         */
        if(rq->flag == R_BUS)pthread_cond_signal(&rq->spool_up);
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

void init_rq(struct routine_queue* rq, int n_threads){
    pthread_cond_init(&rq->spool_up, NULL);
    pthread_mutex_init(&rq->rlock, NULL);
    rq->first = rq->last = NULL;
    rq->flag = R_BUS;
    rq->routines_completed = 0;
    rq->r_target = -1;

    /* only used to determine which thread should
     * destroy the rq instance
     */
    rq->running_threads = n_threads;
}

void init_spool_t(struct spool_t* s, int n_threads){
    /*pthread_cond_init(&s->spool_up, NULL);*/
    init_rq(&s->rq, n_threads);
    init_tq(&s->tq, n_threads, &s->rq);
}

void insert_rq(struct routine_queue* rq, void* (*func)(void*),
                                         void* arg){
    struct routine* r = malloc(sizeof(struct routine));

    r->func = func;
    r->arg = arg;
    r->next = NULL;

    pthread_mutex_lock(&rq->rlock);
    if(!rq->first)rq->first = r;
    else rq->last->next = r;
    rq->last = r;
    pthread_mutex_unlock(&rq->rlock);

    /* spool at least one thread up for this */

    /* TODO: there's a chance that no other threads
     * will be blocking on spool_up when this is called
     * a scaled down example of this always occurs
     * when there is only one thread in the pool
     * what are some workarounds?
     */
    pthread_cond_signal(&rq->spool_up);
}

void exec_routine(struct spool_t* s, void* (*func)(void*),
                                     void* arg){
    insert_rq(&s->rq, func, arg);
}

void pause_exec(struct spool_t* s){
    s->rq.flag = R_PAUSE;
}

void resume_exec(struct spool_t* s){
    s->rq.flag = R_BUS;
    pthread_cond_broadcast(&s->rq.spool_up);
}

void set_routine_target(struct spool_t* s, int target){
    s->rq.r_target = target;
}

/* returns success */
_Bool await_routine_target(struct spool_t* s){
    if(s->rq.r_target == -1)return 0;
    /*pthread_cond_broadcast(&s->rq.spool_up);*/
    join_tq(&s->tq);
    destroy_tq(&s->tq);
    return 1;
}

#if 0
TODO: write destruction/free() functions
TODO: add await/set target functionality
#endif

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
    struct routine* p = rq->first;
    if(p){
        for(struct routine* r = p->next; r; r = r->next){
            free(p);
            p = r;
        }
        free(p);
    }
    pthread_cond_destroy(&rq->spool_up);
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
    pthread_cond_broadcast(&s->rq.spool_up);
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
            rq->set_up = 1;
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

        if(r->running){
            *r->completed = 1;
            pthread_cond_signal(r->running);
        }

        /* in order to keep compatibility between the two blocking methods,
         * routines_completed will not be incremented when the running cond_t
         * is being used
         */
        else ++rq->routines_completed;
        free(r);

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

    /* this is used to ensure that >= 1 thread
     * is ready before exec_routine() is called
     */
    rq->set_up = 0;
}

void init_spool_t(struct spool_t* s, int n_threads){
    /*pthread_cond_init(&s->spool_up, NULL);*/
    init_rq(&s->rq, n_threads);
    init_tq(&s->tq, n_threads, &s->rq);
}

struct routine* insert_rq(struct routine_queue* rq, void* (*func)(void*),
                          void* arg, _Bool create_cond){
    struct routine* r = malloc(sizeof(struct routine)),
                  /* r_spoof is used to wait for
                   * specific routines to finish running
                   * since await_instructions() will free
                   * each struct routine*, we must allocate
                   * a separate one to return to exec_routine()
                   * that is guaranteed to not be freed
                   * all that this struct must contain is a 
                   * pointer to the relevant pthread_cond_t
                   *
                   * this could be done using a separate struct
                   * but isn't in favor of simplicity
                   */
                  * r_spoof = (create_cond) ? malloc(sizeof(struct routine)) : NULL;

    r->func = func;
    r->arg = arg;
    r->next = NULL;
    /* if !running, await_instructions() will not attempt
     * to signal
     */
    r->running = NULL;
    /*free this*/
    if(create_cond){
        r->running = malloc(sizeof(pthread_cond_t));
        r->completed = r_spoof->completed = calloc(1, sizeof(_Bool));

        /*destroy this*/
        pthread_cond_init(r->running, NULL);
        r_spoof->running = r->running;
    }

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

    return r_spoof;
}

/* if create_cond, a spoof routine* will be returned
 * this will be freed and its cond_t destroyed by 
 * await_single_routine()
 */
struct routine* exec_routine(struct spool_t* s,
                        void* (*func)(void*), void* arg,
                        _Bool create_cond){
    /* this has been added to ensure that at least
     * one thread has become ready before we insert
     * routines into the queue
     *
     * this can sometimes be an issue because
     * pthread_cond_signal() may be called before
     * any threads are waiting
     * and it takes time to initialize the mutex
     * used for pthread_cond_wait()
     *
     * this is almost certainly overkill, since it
     * is only important until the first thread
     * becomes ready
     *
     * TODO: decide how i want to solve this problem
     * a more efficient solution might be to just
     * have the user of the library usleep(1000)
     * OR to busy wait, but in a separate function
     * `wait_for_ready()`
     */
    while(!s->rq.set_up);

    return insert_rq(&s->rq, func, arg, create_cond);
}

void await_single_routine(struct routine* r){
    /* this will only occur in case of user error */
    if(!r)return;
    /* no need to bother if routine has completed
     * by the time we're called, just clean up
     */
    if(!(*r->completed)){
        pthread_mutex_t lck;
        pthread_mutex_init(&lck, NULL);

        pthread_mutex_lock(&lck);
        pthread_cond_wait(r->running, &lck);

        pthread_mutex_destroy(&lck);
    }

    pthread_cond_destroy(r->running);
    free(r->completed);
    free(r->running);
    free(r);
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
TODO: implement changes from retval.diff
#endif

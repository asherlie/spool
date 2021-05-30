#include <pthread.h>
#include <stdatomic.h>

enum RFLAGS{R_BUS, R_PAUSE, R_EXIT};

struct thread{
    int thread_id;
    pthread_t pth;
};

/* thread_queue is used only to keep track of all threads */
struct thread_queue{
    /* this is an array for now - no reason for added complexity */
    struct thread* threads;
    /* in case we'd like to support dynamic resizing */
    int n_threads; /* , thread_cap */
};

//struct routine;

struct routine{
    //volatile void* (*func)(void*);
    void* arg;
    void* (*func)(void*);
    struct routine* next;
};

struct routine_queue{
    pthread_cond_t spool_up;
    pthread_mutex_t rlock;
    struct routine* first, * last;
    //volatile int flag;
    /* this will be used to keep track of
     * the number of routines that have finished
     * can be used to implement an await_n() function
     * NOTE: await_n() should target n-n_threads 
     * instructions, assuming that all threads will
     * be saturated
     *
     * OR, since it's an _Atomic int, we can simply
     * increment after func(arg) calls and check if
     * routines_completed == target
     */
    int r_target;
    _Atomic int routines_completed; 
    _Atomic int flag;
};

struct spool_t{
    struct thread_queue tq;
    struct routine_queue rq;
};

void init_spool_t(struct spool_t* s, int n_threads);
void exec_routine(struct spool_t* s, void* (*func)(void*),
                                     void* arg);
void pause_exec(struct spool_t* s);
void resume_exec(struct spool_t* s);
/* this is inexact by nature
 * there's no guarantee that all running
 * routines have finished when target has
 * been met
 *
 * set_routine_target() is guaranteed to stop
 * execution of routines after n have completed
 */
void set_routine_target(struct spool_t* s, int target);

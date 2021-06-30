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

    /* used to wait on a single routine */
    pthread_cond_t* running;
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
    _Atomic int running_threads;
    _Atomic _Bool set_up;
};

struct spool_t{
    struct thread_queue tq;
    struct routine_queue rq;
};

/* is used only as the return value of exec_routine()
 * and to wait for a routine to finish running
 */

void init_spool_t(struct spool_t* s, int n_threads);
struct routine* exec_routine(struct spool_t* s, void* (*func)(void*),
                                     void* arg, _Bool create_cond);
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
_Bool await_routine_target(struct spool_t* s);
void destroy_spool_t(struct spool_t* s);

#if !1
i want to have the option of waiting for a single thread to finish running
something like:
    await_routine_single(spool_t, thread_id)

    thread_id is a timestamp added to the pointer of func
    it is created in and returned by exec_routine()

    each thread pthread_cond_signals a unique 

    optional argument for exec_routine(), a pointer to a
    pthread_cond_t that will be signaled once the routine is done
    in addition to the spool_up cond
#endif

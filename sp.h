#include <pthread.h>

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
};

struct spool_t{
    struct thread_queue tq;
    struct routine_queue rq;
};

void init_spool_t(struct spool_t* s, int n_threads);
void exec_routine(struct spool_t* s, void* (*func)(void*),
                                     void* arg);

#include <pthread.h>

struct thread{
    int thread_id;
    pthread_t pth;
};

struct thread_queue{
    /* this is an array for now - no reason for added complexity */
    struct thread* threads;
    /* in case we'd like to support dynamic resizing */
    int n_threads; /* , thread_cap */;
};

struct spool_t{
    struct thread_queue* tq;
    pthread_cond_t spool_up;
};

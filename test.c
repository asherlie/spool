#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "sp.h"

void* tst(void* arg){
    usleep((random() % 100000) + 1000);
    printf("%i\n", *((int*)arg));

    return NULL;
}

int main(){
    struct spool_t s;
    int* arg;

    init_spool_t(&s, 20);
    for(int i = 0; i < 90; ++i){
        arg = malloc(sizeof(int));
        *arg = i;
        exec_routine(&s, tst, arg);
    }

    usleep(11000000);
}

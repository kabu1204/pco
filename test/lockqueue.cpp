#include <pthread.h>
#include <queue>
#include <ctime>
#include "atomic.h"
#include "cstdio"

struct test_ctl{
    std::queue<u64>* q;
    int start;
    int end;
};

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
u64 done = 0;

pthread_cond_t q_empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

void* lscq_push_fn(void* arg){
    test_ctl* ctl = (test_ctl*)arg;
    int step, end, start, i;
    std::queue<u64>* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    for(i=start;i<end;++i){
        // atomic_add_u64(&cnt, 1);
        pthread_mutex_lock(&mtx);
        q->emplace(i);
        pthread_cond_signal(&q_empty);
        pthread_mutex_unlock(&mtx);
    }
    // printf("push finish: %d %ld\n", end, cnt);
    atomic_add_u64(&done, 1);
    pthread_cond_signal(&cond);
    return NULL;
}
void* lscq_pop_fn(void* arg){
    test_ctl* ctl = (test_ctl*)arg;
    int step, end, start, i;
    std::queue<u64>* q;
    start = ctl->start;
    end = ctl->end;
    q = ctl->q;
    u64 val;
    for(i=start;i<end;++i){
        pthread_mutex_lock(&mtx);
        while(q->empty())
            pthread_cond_wait(&q_empty, &mtx);
        val = q->front();
        q->pop();
        pthread_mutex_unlock(&mtx);
    }
    atomic_add_u64(&done, 1);
    pthread_cond_signal(&cond);
    // printf("pop finish: %d %ld\n", end, cnt2);
    return NULL;
}

int main(){
    int i;
    int n_thread = 32;
    int per_thread = 65536*10/16;
    int total_ops = n_thread*per_thread;
    struct timespec start_time, end_time;
    double cost;

    pthread_t t[32];
    test_ctl ctls[32];
    std::queue<u64> q;
    for(i=0;i<n_thread;i+=2){
        ctls[i+1].q = ctls[i].q = &q;
        ctls[i+1].start = ctls[i].start = (i/2)*per_thread;
        ctls[i+1].end = ctls[i].end = (i/2+1)*per_thread;
    }
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for(i=0;i<n_thread;i+=2){
        pthread_create(&t[i], NULL, lscq_push_fn, &ctls[i]);
    }
    for(i=0;i<n_thread;i+=2){
        pthread_create(&t[i+1], NULL, lscq_pop_fn, &ctls[i+1]);
    }
    // for(i=0;i<n_thread;i+=2){
    //     pthread_join(t[i], NULL);
    //     pthread_join(t[i+1], NULL);
    // }
    while(atomic_load_u64(&done)<32){
        pthread_cond_wait(&cond, &lk);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    cost = (end_time.tv_nsec - start_time.tv_nsec) + (end_time.tv_sec - start_time.tv_sec)*1e9;
    printf("total_ops=%d, %.2f ns/op\n", total_ops, cost/(double)total_ops);
}

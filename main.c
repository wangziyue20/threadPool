#include "threadpool.h"
#include <stdio.h>

void taskFunc(void* arg){
    int num = *(int*)arg;
    printf("thread %ld is working..., number = %d\n", pthread_self(), num);
    sleep(1);
}

int main(){
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    for(int i = 0; i < 100; i++){
        int* num = (int*)malloc(sizeof(int));
        *num = i;
        threadPoolAdd(pool, taskFunc, num);
    }
    sleep(30);
    threadPoolDestory(pool);
    return 0;
}
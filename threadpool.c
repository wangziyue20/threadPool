#include "threadpool.h"

const int NUMBER = 2;

//任务结构体
typedef struct Task{
    void (*function)(void* arg);//函数指针
    void* arg;  //实参地址
}Task;

//线程池结构体
struct ThreadPool{
    Task* TaskQ;        //任务队列
    int queueCapacity;  //任务队列容量
    int queueSize;      //当前任务数
    int queueFront;     //队头
    int queueRear;      //队尾

    pthread_t managerID;    //管理者线程
    pthread_t *threadIDs;    //管理者线程
    int minNum;              //核心线程数
    int maxNum;              //最大线程数
    int busyNum;             //工作线程数
    int liveNum;             //当前线程数
    int exitNum;             //要销毁的线程数

    pthread_mutex_t mutexPool;//锁整个线程池
    pthread_mutex_t mutexBusy;//busyNum
    pthread_cond_t notFull;   //任务队列是否满了
    pthread_cond_t notEmpty;  //任务队列是否空了

    int shutdown;             //是否销毁线程池，销毁为1
};

ThreadPool* threadPoolCreate(int min, int max, int queueSize){
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do{
        if(pool == NULL){
            printf("malloc failed !\n");
            break;
        }

        //1.线程管理
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if(pool->threadIDs == NULL){
            printf("malloc threadIDs failed !\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);//判断线程槽位是否被使用
        pool->busyNum = 0;
        pool->maxNum = max;
        pool->minNum = min;
        pool->liveNum = min;
        pool->exitNum = 0;

        //2.线程同步
        if(pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
        pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
        pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
        pthread_cond_init(&pool->notFull, NULL) != 0){
            printf("mutex or cond init failed \n");
        }

        //3.任务队列
        pool->TaskQ = (Task*)malloc(sizeof(Task) * queueSize);
        if(pool->TaskQ  == NULL){
            printf("malloc TaskQ failed !\n");
            break;
        }
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

        //创建线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        for(int i = 0; i < min; i++){
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        
        return pool;
    } while(0);

    //释放资源

    if(pool->threadIDs) free(pool->threadIDs);
    if(pool->TaskQ) free(pool->TaskQ);
    if(pool) free(pool);
    return NULL;
}

void* worker(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;

    while(1){
        pthread_mutex_lock(&pool->mutexPool);
        //1. 不断检查任务队列
        while(pool->queueSize == 0 && pool->shutdown == 0){
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
            //判断是否销毁
            if(pool->exitNum > 0){
                pool->exitNum--;
                if(pool->liveNum > pool->minNum){
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }

        //2.判断线程池是否关闭
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        //3.从任务队列中取出任务执行
        Task task = pool->TaskQ[pool->queueFront];
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;

        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        //4.执行任务函数
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;       
        pthread_mutex_unlock(&pool->mutexBusy);

        printf("thread: %ld start working...\n", pthread_self());
        task.function(task.arg);//通过函数指针调用函数
        printf("thread %ld worked...\n", pthread_self());
        free(task.arg);
        task.arg = NULL;
        
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;       
        pthread_mutex_unlock(&pool->mutexBusy);
    }
}

void* manager(void* arg){
    ThreadPool* pool = (ThreadPool*) arg;
    while(!pool->shutdown){
        sleep(3);

        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);
        
        //1.任务队列>线程数，线程数小于最大值
        if(liveNum < pool->maxNum && liveNum < queueSize){   
            pthread_mutex_lock(&pool->mutexPool);  

            int counter = 0;
            for(int i = 0; i < pool->maxNum && pool->liveNum < pool->maxNum
            && counter < NUMBER; i++){
                if(pool->threadIDs[i] == 0){
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    pool->liveNum++;
                    counter++; 
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        
        //2.忙线程*2 < 线程数，线程数超过最小值
        if(busyNum * 2 > liveNum && liveNum > pool->maxNum){
            pthread_mutex_lock(&pool->mutexPool);

            pool->exitNum = NUMBER;
            for(int i = 0; i < NUMBER; i++){
                //将正在运行却阻塞的线程唤醒
                pthread_cond_signal(&pool->notEmpty);
            }

            pthread_mutex_unlock(&pool->mutexPool);
        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool){
    pthread_t tid = pthread_self();
    for(int i = 0; i < pool->maxNum; i++){
        if(pool->threadIDs[i] == tid){
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exited\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}

void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg){
    pthread_mutex_lock(&pool->mutexPool);

    //1.判断任务队列是否满了
    while(pool->queueCapacity == pool->queueSize && !pool->shutdown){
        //阻塞生产者线程
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }

    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }

    //2.添加任务
    pool->TaskQ[pool->queueRear].function = func;
    pool->TaskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);

    return busyNum;
}


int threadPoolLiveNum(ThreadPool* pool){
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);

    return liveNum;
}

int threadPoolDestory(ThreadPool* pool){
    if(pool == NULL)
        return 0;

    pool->shutdown = 1;
    //1.回收管理者线程
    pthread_join(pool->managerID, NULL);

    //2.回收阻塞的消费者线程
    // pthread_mutex_lock(&pool->mutexPool);
    
    int num = pool->liveNum - pool->busyNum;
    for(int i = 0; i < num; i++){
        pthread_cond_signal(&pool->notEmpty);
    }

    // pthread_mutex_unlock(&pool->mutexPool);

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    
    //释放堆内存
    if(pool->threadIDs) free(pool->threadIDs);
    if(pool->TaskQ) free(pool->TaskQ);
    if(pool) free(pool);
    pool = NULL;

    return 0;
}


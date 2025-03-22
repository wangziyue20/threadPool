#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct ThreadPool ThreadPool;
//创建线程池
ThreadPool* threadPoolCreate(int min, int max, int queueSize);
//销毁线程池
int threadPoolDestory(ThreadPool* pool);

//添加任务
void threadPoolAdd(ThreadPool* pool, void(*func)(void*), void* arg);

//获取工作的线程池个数
int threadPoolBusyNum(ThreadPool* pool);

//获取当前线程数
int threadPoolLiveNum(ThreadPool* pool);

//工作线程函数
void* worker(void* arg);

//管理者线程函数
void* manager(void* arg);

//线程退出函数
void threadExit(ThreadPool* pool);
#endif
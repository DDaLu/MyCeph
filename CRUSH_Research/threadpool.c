#include "include/threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const int NUMBER = 2;
//任务结构体
typedef struct Task
{
    void* (*function)(void* arg);   //函数指针
    void* arg;     //函数指针的参数
}Task;


//线程池结构体
struct ThreadPool
{
    //任务队列
    Task* taskQ;
    int queueCapacity;  //容量
    int queueSize;       //当前任务个数
    int queueFront;           //对头——>取数据
    int queueRear;              //队尾——>放数据

    //
    pthread_t managerID;      //管理者线程ID
    pthread_t *threadIDs;      //工作线程ID
    int minNum;         //最小线程数
    int maxNum;         //最大线程数
    int busyNum;        //忙的线程数
    int liveNum;        //存活的线程数
    int exitNum;        //要摧毁的线程数
    pthread_mutex_t mutexPool;       //锁整个线程池
    pthread_mutex_t mutexBusy;       //锁busyNum变量
    pthread_cond_t notFull;           //任务队列是不是满了
    pthread_cond_t notEmpty;          //任务队列是不是空了

    int shutDown;            //是不是要摧毁线程池，销毁为1，不销毁为0
};

ThreadPool *threadPoolCreate(int min,int max,int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));    //申请一个threadPool类型内存空间
    do
    {
        if(pool == NULL)
        {
            printf("malloc threadpool fail........\n");
            break;
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);  //申请一个pthread_t类型，大小sizeof(pthread_t) * max为内存空间
        if(pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail.....\n");
            break;
        }

        //初始化threadPool的变量
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;
        pool->exitNum = 0;
        
        if(pthread_mutex_init(&pool->mutexPool,NULL)!=0 || 
        pthread_mutex_init(&pool->mutexBusy,NULL)!=0 ||
        pthread_cond_init(&pool->notEmpty,NULL)!=0   ||
        pthread_cond_init(&pool->notFull,NULL)!=0)
        {
            printf("Init mutex/cond fail.....");
            break;
        }

        //任务队列
        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueFront = 0;
        pool->queueRear = 0;
        pool->queueRear = 0;

        pool->shutDown = 0;
        
        //创建线程
        pthread_create(&pool->managerID,NULL,manager,pool);
        int i;
        for(i=0;i<min;i++)
        {
            pthread_create(&pool->threadIDs[i],NULL,worker,pool);
        }
        return pool;
    } while (0);
    
    //释放资源
    if(pool&&pool->threadIDs) free(pool->threadIDs);
    if(pool&&pool->taskQ) free(pool->taskQ);
    if(pool) free(pool);

    return NULL;
}

//添加任务
void threadPoolAdd(ThreadPool* pool,void* (func)(void*),void*arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutDown)
    {
        //阻塞生产者线程
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);
    }
    if(pool->shutDown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return ;
    }

    //添加任务
    //printf("threadPoolAdd pool->queueRear:%d\n",pool->queueRear);
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;
    // printf("threadPoolAdd pool->queueSize:%d\n",pool->queueSize);
    
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while(1)      
    {
        pthread_mutex_lock(&pool->mutexPool);
        while(pool->queueSize == 0 && !pool->shutDown)   //任务队列无任务进入睡眠
        {
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);    //线程阻塞唤醒的信号只有两种：销毁或取出任务工作

            //判断是不是要销毁线程
            if(pool->exitNum > 0)
            {
                pool->exitNum--;
                if(pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);   //线程销毁前释放互斥锁，放置死锁发生
                    threadExit(pool);
                }  
            }

        }
        //判断线程池是否被关闭了
        if(pool->shutDown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }
        
        //从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;


        //移动任务队列头节点
        pool->queueFront = (pool->queueFront + 1 ) % pool->queueCapacity;
        pool->queueSize--;

        
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        //printf("queueSize:%d\tliveNum:%d\tbusyNum:%d\n",pool->queueSize,pool->liveNum,pool->busyNum);
        pthread_mutex_unlock(&pool->mutexBusy);
        task.function(task.arg);  //执行任务
        //此处执行函数的参数不删除，删除出错，主线程继续使用
        // free(task.arg);
        // task.arg=NULL;
        
        //printf("thread %ld end working.....\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}


int threadPoolBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
    
}

int threadPoolDestroy(ThreadPool* pool)
{
    if(pool == NULL)
    {
        return -1;
    }
    //关闭线程池
    pool->shutDown = 1;
    //阻塞回收管理者线程
    pthread_join(pool->managerID,NULL);
    //唤醒阻塞的消费者线程
    int i;
    for(i=0;i<pool->liveNum;i++)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    //释放堆内存
    if(pool->taskQ)
    {
        free(pool->taskQ);
    }
    if(pool->threadIDs)
    {
        free(pool->threadIDs);
    }
    
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_mutex_destroy(&pool->mutexPool);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    free(pool);
    pool = NULL;
    return 0;
}

void* manager(void* arg)
{
    ThreadPool* pool = (ThreadPool*) arg;
    while(!pool->shutDown)
    {
        //每隔3秒检测一次
        sleep(1);

        //取出线程池中任务的数量和当前线程数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        //取出忙的线程数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程
        //当前任务个数大于存活线程个数 && 存活线程数小于最大线程数
        //printf("queueSize:%d   liveNum:%d\n",queueSize,liveNum);
        if(queueSize >= 1 && liveNum < pool->maxNum)
        {
            int counter = 0;    
            pthread_mutex_lock(&pool->mutexPool);   //锁在这里的原因
            int i;
            for(i = 0;i < pool->maxNum && counter < NUMBER && liveNum < pool->maxNum;i++)
            {
                if(pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    counter++;
                    pool->liveNum++;
                    printf("---------------------Alive thread: %d---------------------\n",pool->liveNum);
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        //忙的线程*2 小于存活的线程数 && 存活的线程数大于最小线程数
        if(busyNum*2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //让工作的线程自杀
            int i;
            for(i=0;i<NUMBER;i++)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    int i;
    for(i = 0;i < pool->maxNum;i++)
    {
        if(pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;     
            printf("thread %ld exiting.....\n",tid);
            //printf("---------------------Dead thread: %d---------------------\n",pool->liveNum);
            break;
        }
    }
    pthread_exit(NULL);
}

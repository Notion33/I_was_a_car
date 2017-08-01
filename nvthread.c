/*
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
 * All information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

#include <nvthread.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifndef NVMEDIA_QNX
#include <sys/syscall.h>
#endif
#include <stdio.h>
#include <time.h>

typedef struct tagNvThreadLinux {
    NvU32 (*pFunc)(void*);
    pthread_cond_t      condition;
    pthread_mutex_t     mutex;
    pthread_mutexattr_t mutexattr;
    void*               pParam;
    pthread_t           thread;
    pthread_attr_t      thread_attr;
    pid_t               pid;
    int                 priority;
} NvThreadLinux;

typedef struct tagNvEventLinux {
    pthread_cond_t     condition;
    pthread_mutex_t    mutex;
    int                signaled;
    int                manual;
} NvEventLinux;

typedef struct tagNvMutexDataLinux {
    pthread_mutexattr_t mutexattr;
    pthread_mutex_t     mutex;
} NvMutexLinux;

typedef struct tagNvSemaphoreLinux {
    pthread_cond_t  condition;
    pthread_mutex_t mutex;
    NvU32             maxCount;
    NvU32             count;
} NvSemaphoreLinux;

static void *ThreadFunc(void *pParam);
static int s_iSchedPolicy;
static int s_iSchedPriorityMin;
static int s_iSchedPriorityMax;
static int s_iSchedPriorityBase;
static void CalculateTimeoutTime(NvU32 uTimeoutMs, struct timespec *pTimeOut);

NvResult NvMutexCreate(NvMutex **ppMutexApp)
{
    NvMutexLinux *pMutex = (NvMutexLinux *)malloc(sizeof(NvMutexLinux));
    int iReturnCode = 0;

    *ppMutexApp = 0;
    if(pMutex){
        memset(pMutex, 0, sizeof(NvMutexLinux));

        iReturnCode = pthread_mutexattr_init(&pMutex->mutexattr);
        if (iReturnCode) {
            printf("pthread_mutexattr_init failed (%d)", iReturnCode);
            free(pMutex);
            return RESULT_FAIL;
        }

#ifndef NVMEDIA_QNX
        iReturnCode = pthread_mutexattr_settype(&pMutex->mutexattr, PTHREAD_MUTEX_RECURSIVE_NP);
        if (iReturnCode) {
            printf("pthread_mutexattr_settype failed (%d)", iReturnCode);
            pthread_mutexattr_destroy(&pMutex->mutexattr);
            free(pMutex);
            return RESULT_FAIL;
        }
#endif
        iReturnCode = pthread_mutex_init(&pMutex->mutex, &pMutex->mutexattr);
        if (iReturnCode) {
            pthread_mutexattr_destroy(&pMutex->mutexattr);
            printf("pthread_mutex_init failed (%d)", iReturnCode);
            free(pMutex);
            return RESULT_FAIL;
        }

        *ppMutexApp = (NvMutex *)pMutex;
        return RESULT_OK;
    }
    return RESULT_FAIL;
}

NvResult NvMutexDestroy(NvMutex *pMutexApp)
{
    NvMutexLinux *pMutex = (NvMutexLinux *)pMutexApp;
    int iReturnCode1 = 0, iReturnCode2 = 0;

    if(pMutex){
        iReturnCode1 = pthread_mutex_destroy(&pMutex->mutex);
        if(iReturnCode1){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode1);
        }
        iReturnCode2 = pthread_mutexattr_destroy(&pMutex->mutexattr);
        if(iReturnCode2){
            printf("pthread_mutexattr_destroy failed (%d)", iReturnCode2);
        }
        free(pMutex);
        return (!iReturnCode1 && !iReturnCode2) ? RESULT_OK : RESULT_FAIL;
    }
    return RESULT_FAIL;
}

NvResult NvMutexAcquire(NvMutex *pMutexApp)
{
    NvMutexLinux *pMutex = (NvMutexLinux *)pMutexApp;
    int iReturnCode = 0;

    if(pMutex){
        iReturnCode = pthread_mutex_lock(&pMutex->mutex);
        if (iReturnCode) {
            printf("pthread_mutex_lock failed (%d)", iReturnCode);
        }
        return iReturnCode ? RESULT_FAIL : RESULT_OK;
    }
    printf("Invalid handle failed (%d)", iReturnCode);
    return RESULT_FAIL;
}

NvResult NvMutexRelease(NvMutex *pMutexApp)
{
    NvMutexLinux *pMutex = (NvMutexLinux *)pMutexApp;
    int iReturnCode = 0;

    if(pMutex){
        iReturnCode = pthread_mutex_unlock(&pMutex->mutex);
        if (iReturnCode) {
            printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        }
        return iReturnCode ? RESULT_FAIL : RESULT_OK;
    }
    printf("Invalid handle failed (%d)", iReturnCode);
    return RESULT_FAIL;
}

#ifndef NVMEDIA_QNX
static pid_t gettid(void)
{
    return (pid_t)syscall(__NR_gettid);
}
#endif

static void *ThreadFunc(void *pParam)
{
    NvU32 uResult;
    NvThreadLinux *pThread = (NvThreadLinux *)(pParam);
    int iReturnCode = 0, iReturnCode2 = 0;

    if (pThread->pid == 0){
        iReturnCode = pthread_mutex_lock(&pThread->mutex);
        if(iReturnCode){
            printf("pthread_mutex_lock failed (%d)", iReturnCode);
            return (void*)iReturnCode;
        }
        pThread->pid = gettid();
        iReturnCode = pthread_cond_signal(&pThread->condition);
        if(iReturnCode){
            printf("pthread_cond_signal failed (%d)", iReturnCode);
            iReturnCode2 = pthread_mutex_unlock(&pThread->mutex);
            if(iReturnCode2){
                printf("pthread_mutex_unlock failed (%d)", iReturnCode2);
                return (void*)iReturnCode2;
            }
            return (void*)iReturnCode;
        }
        iReturnCode = pthread_mutex_unlock(&pThread->mutex);
        if(iReturnCode){
            printf("pthread_mutex_unlock failed (%d)", iReturnCode);
            return (void*)iReturnCode;
        }
    }
    uResult = (*pThread->pFunc)(pThread->pParam);
    return (void*)uResult;
}

int NvThreadGetPid(NvThread *pThreadApp)
{
    NvThreadLinux *pThread = (NvThreadLinux *)pThreadApp;
    return (int)pThread->pid;
}

NvResult NvThreadCreate(NvThread **ppThreadApp, NvU32 (*pFunc)(void *pParam), void *pParam, int sPriority)
{
    struct sched_param oSched;
    NvThreadLinux *pThread = (NvThreadLinux *)calloc(1, sizeof(NvThreadLinux));
    int iReturnCode = 0;

    if(pThread){
        pThread->pFunc = pFunc;
        pThread->pParam = pParam;
        pThread->pid = 0;
        iReturnCode = pthread_getschedparam(pthread_self(), &s_iSchedPolicy, &oSched);
        if (iReturnCode == 0){
            if (s_iSchedPolicy == SCHED_OTHER) {
                s_iSchedPriorityBase = getpriority(PRIO_PROCESS, 0);
                s_iSchedPriorityMin = -20;
                s_iSchedPriorityMax =  19;
            } else {
                s_iSchedPriorityBase = oSched.sched_priority;
                s_iSchedPriorityMin = sched_get_priority_min(s_iSchedPolicy);
                s_iSchedPriorityMax = sched_get_priority_max(s_iSchedPolicy);
            }
        } else {
            printf("pthread_getschedparam failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_attr_init(&pThread->thread_attr);

        if(iReturnCode){
            printf("pthread_attr_init failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_attr_setinheritsched(&pThread->thread_attr, PTHREAD_INHERIT_SCHED);

        if(iReturnCode){
            printf("pthread_attr_setinheritsched failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_attr_setstacksize(&pThread->thread_attr, (PTHREAD_STACK_MIN + 0x4000));
        if(iReturnCode){
            printf("pthread_attr_setstacksize failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_mutex_init(&pThread->mutex, 0);
        if(iReturnCode){
            printf("pthread_mutex_init failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_cond_init(&pThread->condition, 0);
        if(iReturnCode){
            printf("pthread_mutex_init failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_create(&pThread->thread, &pThread->thread_attr, ThreadFunc, (void*)pThread);
        if (iReturnCode) {
            printf("pthread_create failed (%d)", iReturnCode);
            free(pThread);
            return RESULT_FAIL;
        }

        iReturnCode = pthread_mutex_lock(&pThread->mutex);
        if(iReturnCode){
            free(pThread);
            printf("pthread_mutex_lock failed (%d)", iReturnCode);
            return RESULT_FAIL;
        }

        while (pThread->pid == 0){
            iReturnCode = pthread_cond_wait(&pThread->condition, &pThread->mutex);
            if(iReturnCode){
                free(pThread);
                printf("pthread_cond_wait failed (%d)", iReturnCode);
                return RESULT_FAIL;
            }
        }
        iReturnCode = pthread_mutex_unlock(&pThread->mutex);
        if(iReturnCode){
            free(pThread);
            printf("pthread_mutex_unlock failed (%d)", iReturnCode);
            return RESULT_FAIL;
        }
        NvThreadPrioritySet(pThread, sPriority);
        *ppThreadApp = (NvThread *)pThread;
        return RESULT_OK;
    }
    return RESULT_FAIL;
}

NvResult NvThreadPriorityGet(NvThread *pThreadApp, int *psPriority)
{
    NvThreadLinux *pThread = (NvThreadLinux *)pThreadApp;
    int iSchedPolicy;
    int iReturnCode;
    struct sched_param oSched;

    if(!psPriority)
        return RESULT_INVALID_POINTER;

    if (pThread->pid == 0)
         return RESULT_FAIL;

    iReturnCode = pthread_getschedparam(pThread->thread, &iSchedPolicy, &oSched);
    if (iReturnCode){
        printf("pthread_getschedparam failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    if (iSchedPolicy == -1) {
        printf("sched_getscheduler failed (errno=%d)", errno);
        return RESULT_FAIL;
    }

    if (iSchedPolicy == SCHED_OTHER) {
        *psPriority = getpriority(PRIO_PROCESS, pThread->pid);
    } else {
        *psPriority = oSched.sched_priority;
    }
    return RESULT_OK;
}

NvResult NvThreadPrioritySet(NvThread *pThreadApp, int sPriority)
{
    NvThreadLinux *pThread = (NvThreadLinux *)pThreadApp;
    int iSchedPolicy = -1;
    int iReturnCode;
    struct sched_param oSched;

    if (pThread->pid == 0)
         return RESULT_FAIL;

    iReturnCode = pthread_getschedparam(pThread->thread, &iSchedPolicy, &oSched);
    if (iReturnCode){
        printf("pthread_getschedparam failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    if (iSchedPolicy == -1) {
        printf("pthread_getschedparam failed (errno=%d)", errno);
        return RESULT_FAIL;
    }

    if (iSchedPolicy == SCHED_OTHER)
        pThread->priority = s_iSchedPriorityBase - sPriority;
    else
        pThread->priority = s_iSchedPriorityBase + sPriority;

    // Ensure priority is within limits
    if (pThread->priority < s_iSchedPriorityMin)
        pThread->priority = s_iSchedPriorityMin;
    else if (pThread->priority > s_iSchedPriorityMax)
        pThread->priority = s_iSchedPriorityMax;

    if (iSchedPolicy == SCHED_OTHER){
        iReturnCode = setpriority(PRIO_PROCESS, pThread->pid, pThread->priority);
        if (iReturnCode){
            printf("setpriority failed (%d)", iReturnCode);
            return RESULT_FAIL;
        }
    } else {
        struct sched_param oSched;
        oSched.sched_priority = pThread->priority;
        iReturnCode = pthread_setschedparam(pThread->thread, iSchedPolicy, &oSched);
        if (iReturnCode){
            printf("pthread_setschedparam failed (%d)", iReturnCode);
            return RESULT_FAIL;
        }
    }
    return RESULT_OK;
}

NvResult NvThreadDestroy(NvThread *pThreadApp)
{
    NvThreadLinux *pThread = (NvThreadLinux *)pThreadApp;
    int iReturnCode1 = 0, iReturnCode2 = 0, iReturnCode3 = 0;

    if (pthread_join(pThread->thread, 0)) {
        return RESULT_FAIL;
    }
    iReturnCode1 = pthread_attr_destroy(&pThread->thread_attr);
    if(iReturnCode1){
        printf("pthread_attr_destroy failed (%d)", iReturnCode1);
    }
    iReturnCode2 = pthread_cond_destroy(&pThread->condition);
    if(iReturnCode2){
        printf("pthread_cond_destroy failed (%d)", iReturnCode2);
    }
    iReturnCode3 = pthread_mutex_destroy(&pThread->mutex);
    if(iReturnCode3){
        printf("pthread_mutex_destroy failed (%d)", iReturnCode3);
    }
    free(pThread);
    return (!iReturnCode1 && !iReturnCode2 && !iReturnCode3) ? RESULT_OK : RESULT_FAIL;
}

NvResult NvThreadYield(void)
{
    int iReturnCode;

    iReturnCode = sched_yield();
    if(iReturnCode) {
        printf("sched_yield: %d\n", errno);
        perror("sched_yield");
    }
    return (!iReturnCode) ? RESULT_OK : RESULT_FAIL;
}

NvResult NvEventCreate(NvEvent **ppEventApp, int bManual, int bSet)
{
    int iReturnCode = 0;
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    pthread_condattr_t oCondAttr;
#endif
    pthread_condattr_t *pCondAttr = 0;
    *ppEventApp = 0;

    NvEventLinux *pEvent = (NvEventLinux *)malloc(sizeof(NvEventLinux));
    if(!pEvent) {
        return RESULT_FAIL;
    }

    pEvent->manual = bManual;

    iReturnCode = pthread_mutex_init(&pEvent->mutex, 0);
    if (iReturnCode) {
        printf("pthread_mutex_init failed (%d)", iReturnCode);
        free(pEvent);
        return RESULT_FAIL;
    }

// If monotonic clock and clock selection are supported
// then create condattr and set the clock type to CLOCK_MONOTONIC
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    iReturnCode = pthread_condattr_init(&oCondAttr);
    if (iReturnCode) {
        printf("pthread_condattr_init failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_destroy(&pEvent->mutex);
        if(iReturnCode){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        free(pEvent);
        return RESULT_FAIL;
    }
    iReturnCode = pthread_condattr_setclock(&oCondAttr, CLOCK_MONOTONIC);
    if (iReturnCode) {
        printf("pthread_condattr_setclock failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_destroy(&pEvent->mutex);
        if(iReturnCode){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        free(pEvent);
        return RESULT_FAIL;
    }
    pCondAttr = &oCondAttr;
#endif

    iReturnCode = pthread_cond_init(&pEvent->condition, pCondAttr);
    if (iReturnCode) {
        printf("pthread_cond_init failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_destroy(&pEvent->mutex);
        if(iReturnCode){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        free(pEvent);
        return RESULT_FAIL;
    }

    *ppEventApp = (NvEvent *)pEvent;

    if(bSet) {
        NvEventSet(pEvent);
    } else {
        NvEventReset(pEvent);
    }
    return RESULT_OK;
}

static NvU64 GetClock(void)
{
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0
    struct timespec tv;
#else
    struct timeval tv;
#endif

    // Get current time
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (NvU64)tv.tv_sec * (NvU64)1000000 + (NvU64)(tv.tv_nsec / 1000);
#else
    gettimeofday(&tv, NULL);
    return (NvU64)tv.tv_sec * (NvU64)1000000 + (NvU64)tv.tv_usec;
#endif
}

void CalculateTimeoutTime(NvU32 uTimeoutMs, struct timespec *pTimeOut)
{
    NvU64 currentTimeuSec;

    // Get current time
    currentTimeuSec = GetClock();

    // Add Timeout (in micro seconds)
    currentTimeuSec += (uTimeoutMs * 1000);

    // Split to Seconds and Nano Seconds
    pTimeOut->tv_sec  = currentTimeuSec / 1000000;
    pTimeOut->tv_nsec = (currentTimeuSec % 1000000) * 1000;
}

NvResult NvEventWait(NvEvent *pEventApp, NvU32 uTimeoutMs)
{
    struct timespec timeout;
    NvEventLinux    *pEvent = (NvEventLinux *)pEventApp;
    int iReturnCode = 0;

    CalculateTimeoutTime(uTimeoutMs, &timeout);

    iReturnCode = pthread_mutex_lock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_lock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }

    if (uTimeoutMs == 0) {
        if (pEvent->signaled) {
            if (!pEvent->manual) {
                pEvent->signaled = 0;
            }
        } else {
            iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
            if(iReturnCode){
                printf("pthread_mutex_unlock failed (%d)", iReturnCode);
            }
            return RESULT_FAIL;
        }
    } else if (uTimeoutMs == NV_TIMEOUT_INFINITE) {
        while (!pEvent->signaled) {
            iReturnCode = pthread_cond_wait(&pEvent->condition, &pEvent->mutex);
            if(iReturnCode){
                printf("pthread_cond_wait failed (%d)", iReturnCode);
            }
        }
        if (!pEvent->manual) {
            pEvent->signaled = 0;
        }
    } else {
        while (!pEvent->signaled) {
            iReturnCode = pthread_cond_timedwait(&pEvent->condition, &pEvent->mutex, &timeout);
            if (iReturnCode == ETIMEDOUT) {
                iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
                if(iReturnCode){
                    printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                }
                return RESULT_FAIL;
            } else if(iReturnCode) {
                printf("pthread_cond_timedwait failed (%d)", iReturnCode);
                iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
                if(iReturnCode){
                    printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                }
                return RESULT_FAIL;
            }
        }
        if (!pEvent->manual) {
            pEvent->signaled = 0;
        }
    }
    iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }

    return RESULT_OK;
}

NvResult NvEventSet(NvEvent *pEventApp)
{
    NvEventLinux *pEvent = (NvEventLinux *)pEventApp;
    int iReturnCode = 0;

    iReturnCode = pthread_mutex_lock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_lock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    pEvent->signaled = 1;
    iReturnCode = pthread_cond_signal(&pEvent->condition);
    if(iReturnCode){
        printf("pthread_cond_signal failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
        if(iReturnCode){
            printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        }
        return RESULT_FAIL;
    }
    iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    return RESULT_OK;
}

NvResult NvEventReset(NvEvent *pEventApp)
{
    NvEventLinux *pEvent = (NvEventLinux *)pEventApp;
    int iReturnCode = 0;

    iReturnCode = pthread_mutex_lock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_lock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    pEvent->signaled = 0;
    iReturnCode = pthread_mutex_unlock(&pEvent->mutex);
    if(iReturnCode){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    return RESULT_OK;
}

NvResult NvEventDestroy(NvEvent *pEventApp)
{
    NvEventLinux *pEvent = (NvEventLinux *)pEventApp;
    int iReturnCode1 = 0, iReturnCode2 = 0, iReturnCode3 = 0, iReturnCode4 = 0;

    iReturnCode1 = pthread_mutex_lock(&pEvent->mutex);
    if(iReturnCode1){
        printf("pthread_mutex_lock failed (%d)", iReturnCode1);
    }

    iReturnCode2 = pthread_cond_destroy(&pEvent->condition);
    if(iReturnCode2){
        printf("pthread_cond_destroy failed (%d)", iReturnCode2);
    }

    iReturnCode3 = pthread_mutex_unlock(&pEvent->mutex);
    if(iReturnCode3){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode3);
    }

    iReturnCode4 = pthread_mutex_destroy(&pEvent->mutex);
    if(iReturnCode4){
        printf("pthread_mutex_destroy failed (%d)", iReturnCode4);
    }
    free(pEvent);
    return (!iReturnCode1 && !iReturnCode2 && !iReturnCode3 && !iReturnCode4) ? RESULT_OK : RESULT_FAIL;
}

NvResult NvSemaphoreCreate(NvSemaphore **ppSemaphoreApp, NvU32 uInitCount, NvU32 uMaxCount)
{
    NvSemaphoreLinux *pSem = (NvSemaphoreLinux *)malloc(sizeof(NvSemaphoreLinux));
    int iReturnCode = 0;
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    pthread_condattr_t oCondAttr;
#endif
    pthread_condattr_t *pCondAttr = 0;

    *ppSemaphoreApp = 0;
    if(!pSem) {
        return RESULT_FAIL;
    }

    if (uInitCount > uMaxCount) {
        uInitCount = uMaxCount;
    }

    pSem->maxCount = uMaxCount;
    pSem->count = uInitCount;

    iReturnCode = pthread_mutex_init(&pSem->mutex, 0);
    if (iReturnCode) {
        printf("pthread_mutex_init failed (%d)", iReturnCode);
        free(pSem);
        return RESULT_FAIL;
    }

// If monotonic clock and clock selection are supported
// then create condattr and set the clock type to CLOCK_MONOTONIC
#if defined(USE_MONOTONIC_CLOCK) && defined(CLOCK_MONOTONIC) && defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0
    iReturnCode = pthread_condattr_init(&oCondAttr);
    if (iReturnCode) {
        printf("pthread_condattr_init failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_destroy(&pSem->mutex);
        if(iReturnCode){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        free(pSem);
        return RESULT_FAIL;
    }
    iReturnCode = pthread_condattr_setclock(&oCondAttr, CLOCK_MONOTONIC);
    if (iReturnCode) {
        printf("pthread_condattr_setclock failed (%d)", iReturnCode);
        iReturnCode = pthread_mutex_destroy(&pSem->mutex);
        if(iReturnCode){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        free(pSem);
        return RESULT_FAIL;
    }
    pCondAttr = &oCondAttr;
#endif

    iReturnCode = pthread_cond_init(&pSem->condition, pCondAttr);
    if (iReturnCode) {
        if(pthread_mutex_destroy(&pSem->mutex)){
            printf("pthread_mutex_destroy failed (%d)", iReturnCode);
        }
        printf("pthread_cond_init failed (%d)", iReturnCode);
        free(pSem);
        return RESULT_FAIL;
    }

    *ppSemaphoreApp = (NvSemaphore *)pSem;
    return RESULT_OK;
}

NvResult NvSemaphoreIncrement(NvSemaphore *pSemApp)
{
    NvSemaphoreLinux *pSem = (NvSemaphoreLinux *)pSemApp;
    int iReturnCode = 0;

    iReturnCode = pthread_mutex_lock(&pSem->mutex);
    if(iReturnCode){
        printf("pthread_mutex_lock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    pSem->count++;
    if(pSem->count > pSem->maxCount) {
        pSem->count = pSem->maxCount;
    } else {
        iReturnCode = pthread_cond_broadcast(&pSem->condition);
        if(iReturnCode){
            printf("pthread_cond_broadcast failed (%d)", iReturnCode);
            iReturnCode = pthread_mutex_unlock(&pSem->mutex);
            if(iReturnCode){
                printf("pthread_mutex_unlock failed (%d)", iReturnCode);
            }
            return RESULT_FAIL;
        }
    }
    iReturnCode = pthread_mutex_unlock(&pSem->mutex);
    if(iReturnCode){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        return RESULT_FAIL;
    }
    return RESULT_OK;
}

NvResult NvSemaphoreDecrement(NvSemaphore *pSemApp, NvU32 uTimeoutMs)
{
    struct timespec timeout;
    NvSemaphoreLinux *pSem = (NvSemaphoreLinux *)pSemApp;
    int iReturnCode = 0;

    CalculateTimeoutTime(uTimeoutMs, &timeout);

    while (1) {
        iReturnCode = pthread_mutex_lock(&pSem->mutex);
        if(iReturnCode){
            printf("pthread_mutex_lock failed (%d)", iReturnCode);
            return RESULT_FAIL;
        }
        if (pSem->count > 0) {
           pSem->count--;
           iReturnCode = pthread_mutex_unlock(&pSem->mutex);
           if(iReturnCode){
                printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                return RESULT_FAIL;
           }
           break;
        }

        if (uTimeoutMs == 0) {
            iReturnCode = pthread_mutex_unlock(&pSem->mutex);
            if(iReturnCode){
                printf("pthread_mutex_unlock failed (%d)", iReturnCode);
            }
            return RESULT_FAIL;
        } else if (uTimeoutMs == NV_TIMEOUT_INFINITE) {
            iReturnCode = pthread_cond_wait(&pSem->condition, &pSem->mutex);
            if(iReturnCode){
                iReturnCode = pthread_mutex_unlock(&pSem->mutex);
                if(iReturnCode){
                    printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                }
                printf("pthread_cond_wait failed (%d)", iReturnCode);
                return RESULT_FAIL;
           }
        } else {
            iReturnCode = pthread_cond_timedwait(&pSem->condition, &pSem->mutex, &timeout);
            if (iReturnCode == ETIMEDOUT) {
                iReturnCode = pthread_mutex_unlock(&pSem->mutex);
                if(iReturnCode){
                    printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                }
                return RESULT_FAIL;
            } else if(iReturnCode){
                printf("pthread_cond_timedwait failed (%d)", iReturnCode);
                iReturnCode = pthread_mutex_unlock(&pSem->mutex);
                if(iReturnCode){
                    printf("pthread_mutex_unlock failed (%d)", iReturnCode);
                }
                return RESULT_FAIL;
            }
        }
        iReturnCode = pthread_mutex_unlock(&pSem->mutex);
        if(iReturnCode){
            printf("pthread_mutex_unlock failed (%d)", iReturnCode);
        }
    }

    return RESULT_OK;
}

NvResult NvSemaphoreDestroy(NvSemaphore *pSemApp)
{
    NvSemaphoreLinux *pSem = (NvSemaphoreLinux *)pSemApp;
    int iReturnCode1 = 0, iReturnCode2 = 0, iReturnCode3 = 0, iReturnCode4 = 0;

    iReturnCode1 = pthread_mutex_lock(&pSem->mutex);
    if(iReturnCode1){
        printf("pthread_mutex_lock failed (%d)", iReturnCode1);
    }

    iReturnCode2 = pthread_cond_destroy(&pSem->condition);
    if(iReturnCode2){
        printf("pthread_cond_destroy failed (%d)", iReturnCode2);
    }

    iReturnCode3 = pthread_mutex_unlock(&pSem->mutex);
    if(iReturnCode3){
        printf("pthread_mutex_unlock failed (%d)", iReturnCode3);
    }

    iReturnCode4 = pthread_mutex_destroy(&pSem->mutex);
    if(iReturnCode4){
        printf("pthread_mutex_destroy failed (%d)", iReturnCode4);
    }
    free(pSem);
    return (!iReturnCode1 && !iReturnCode2 && !iReturnCode3 && !iReturnCode4) ? RESULT_OK : RESULT_FAIL;
}

NvResult NvQueueCreate(NvQueue **ppQueue, NvU32 uQueueSize, NvU32 uItemSize)
{
    NvQueue *pQueue =(NvQueue *)malloc(sizeof(NvQueue));
    NvResult nr = RESULT_FAIL;
    *ppQueue = pQueue;
    if(pQueue) {
        memset(pQueue, 0, sizeof(pQueue));
        pQueue->uNextPut        = 0;
        pQueue->uNextGet        = 0;
        pQueue->uQueueSize      = uQueueSize;
        pQueue->uItemSize       = uItemSize;
        pQueue->uItems          = 0;
        pQueue->pQueueData      = malloc(uQueueSize * uItemSize);
        nr = NvSemaphoreCreate(&pQueue->pSemGet, 0, uQueueSize);
        nr = NvSemaphoreCreate(&pQueue->pSemPut, uQueueSize, uQueueSize);
        nr = NvMutexCreate(&pQueue->pMutex);
    }
    return nr;
}

NvResult NvQueueDestroy(NvQueue *pQueue)
{
    if(pQueue) {
        free(pQueue->pQueueData);
        NvSemaphoreDestroy(pQueue->pSemGet);
        NvSemaphoreDestroy(pQueue->pSemPut);
        NvMutexDestroy(pQueue->pMutex);
        free(pQueue);
    }
    return RESULT_OK;
}

NvResult NvQueueGet(NvQueue *pQueue, void *pItem, NvU32 uTimeout)
{
    NvResult nr = RESULT_FAIL;
    if(pQueue) {
        if(RESULT_OK == NvSemaphoreDecrement(pQueue->pSemGet, uTimeout)) {
            nr = NvMutexAcquire(pQueue->pMutex);
            pQueue->uNextGet = (pQueue->uNextGet + 1) % pQueue->uQueueSize;
            pQueue->uItems--;
            memcpy(pItem, pQueue->pQueueData + (pQueue->uNextGet * pQueue->uItemSize), pQueue->uItemSize);
            nr = NvMutexRelease(pQueue->pMutex);
            nr = NvSemaphoreIncrement(pQueue->pSemPut);
        }
    }
    return nr;
}

NvResult NvQueuePut(NvQueue *pQueue, void *pItem, NvU32 uTimeout)
{
    NvResult nr = RESULT_FAIL;
    if(pQueue) {
        if(RESULT_OK == NvSemaphoreDecrement(pQueue->pSemPut, uTimeout)) {
            nr = NvMutexAcquire(pQueue->pMutex);
            pQueue->uNextPut = (pQueue->uNextPut + 1) % pQueue->uQueueSize;
            memcpy(pQueue->pQueueData + (pQueue->uNextPut * pQueue->uItemSize), pItem, pQueue->uItemSize);
            pQueue->uItems++;
            nr = NvMutexRelease(pQueue->pMutex);
            nr = NvSemaphoreIncrement(pQueue->pSemGet);
        }
    }
    return nr;
}

NvResult NvQueueGetSize(NvQueue *pQueue, NvU32 *puSize)
{
    NvResult nr = RESULT_FAIL;
    if(pQueue) {
        nr = NvMutexAcquire(pQueue->pMutex);
        *puSize = pQueue->uItems;
        nr = NvMutexRelease(pQueue->pMutex);
    }
    return nr;
}


NvResult NvQueuePeek(NvQueue *pQueue, void *pItem, NvU32 *puItems)
{
    NvResult nr = RESULT_FAIL;
    NvU32 uNextGet;
    if(pQueue) {
        nr = NvMutexAcquire(pQueue->pMutex);
        *puItems = pQueue->uItems;
        if(pQueue->uItems) {
            uNextGet = (pQueue->uNextGet + 1) % pQueue->uQueueSize;
            memcpy(pItem, pQueue->pQueueData + (uNextGet * pQueue->uItemSize), pQueue->uItemSize);
        }
        nr = NvMutexRelease(pQueue->pMutex);
    }
    return nr;
}

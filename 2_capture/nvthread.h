/*
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
 * All information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

#ifndef __NVTHREAD_H__
#define __NVTHREAD_H__

#define _GNU_SOURCE

#include "nvcommon.h"

#define NV_TIMEOUT_INFINITE 0xFFFFFFFF
#define NV_THREAD_PRIORITY_NORMAL 0

typedef void NvThread;
typedef void NvEvent;
typedef void NvMutex;
typedef void NvSemaphore;

typedef int NvResult;
#define RESULT_OK               0
#define RESULT_FAIL             1
#define RESULT_INVALID_POINTER  2

typedef struct tagNvQueue {
    NvU32             uNextGet;
    NvU32             uNextPut;
    NvU8 *            pQueueData;
    NvSemaphore   * pSemGet;
    NvSemaphore   * pSemPut;
    NvMutex       * pMutex;
    NvU32             uItems;
    NvU32             uItemSize;
    NvU32             uQueueSize;
} NvQueue;

typedef void* NvHandle;

typedef struct tagNvHandleEntry {
    NvHandle   handle;
    int        refs;
    int        remove;
} NvHandleEntry;

typedef struct tagNvHandleList {
    NvHandleEntry* entries;
    NvMutex*       mutex;
    int            count;
    int            size;
    NvResult (*pLockingFunc)(NvHandle, int);
    NvResult (*pDestroyFunc)(NvHandle);
} NvHandleList;

NvResult NvMutexCreate(NvMutex **ppMutex);
NvResult NvMutexDestroy(NvMutex *pMutex);
NvResult NvMutexAcquire(NvMutex *pMutex);
NvResult NvMutexRelease(NvMutex *pMutex);
NvResult NvThreadCreate(NvThread **ppThread, NvU32 (*pFunc)(void* pParam), void* pParam, int sPriority);
int NvThreadGetPid(NvThread *pThread);
NvResult NvThreadPriorityGet(NvThread *pThread, int *psPriority);
NvResult NvThreadPrioritySet(NvThread *pThread, int sPriority);
NvResult NvThreadDestroy(NvThread *pThread);
NvResult NvThreadYield(void);
NvResult NvEventCreate(NvEvent **ppEvent, int bManual, int bSet);
NvResult NvEventWait(NvEvent *pEvent, NvU32 uTimeoutMs);
NvResult NvEventSet(NvEvent *pEvent);
NvResult NvEventReset(NvEvent *pEvent);
NvResult NvEventDestroy(NvEvent *pEvent);
NvResult NvSemaphoreCreate(NvSemaphore** ppSemaphore, NvU32 uInitCount, NvU32 uMaxCount);
NvResult NvSemaphoreIncrement(NvSemaphore *pSem);
NvResult NvSemaphoreDecrement(NvSemaphore *pSem, NvU32 uTimeoutMs);
NvResult NvSemaphoreDestroy(NvSemaphore *pSem);
NvResult NvQueueCreate(NvQueue **ppQueue, NvU32 uQueueSize, NvU32 uItemSize);
NvResult NvQueueDestroy(NvQueue *pQueue);
NvResult NvQueueGet(NvQueue *pQueue, void *pItem, NvU32 uTimeout);
NvResult NvQueuePeek(NvQueue *pQueue, void *pItem, NvU32 *puItems);
NvResult NvQueuePut(NvQueue *pQueue, void *pItem, NvU32 uTimeout);
NvResult NvQueueGetSize(NvQueue *pQueue, NvU32 *puSize);

#endif /* __NVTHREAD_H__ */


#ifndef LWSDK_MTUTIL_H
#define LWSDK_MTUTIL_H

/****
 *
 * LWSDK Header File
 * Copyright 2006, NewTek, Inc.
 *
 * LWMTUTIL.H -- LightWave Multi-threading support
 *
 * This header contains declarations necessary to support LightWave-hosted,
 * cross-platform threading for plug-ins.
 *
 */

typedef void *      LWMTUtilID;
typedef void *      LWMTGroupID;
typedef void *      LWMTThreadID;
typedef void *      LWMTRWLockID;

typedef int (*LWMTThreadFunc)(void *arg);

#define LWMTUTILFUNCS_GLOBAL "MultiThreading Utilities 2"

// Each mutex has an array of 129 critical section locks, organized as one thread
// lock (0) and 128 generic locks (1..128). Use the constants below to access the
// locks. These constants may change in the future.
// For future compatibility, the ID of a generic lock is masked by LWMT_MUTEX_MASK
// to give an actual ID: 1 + ((specific ID - 1) & LWMT_MUTEX_MASK).
#define LWMT_MUTEX_BITS     7
#define LWMT_MUTEX_COUNT    ((1 << LWMT_MUTEX_BITS) + 1) // 129 locks in total
#define LWMT_MUTEX_MASK     ((1 << LWMT_MUTEX_BITS) - 1) // 127

typedef struct st_LWMTUtilFuncs {
    // Mutex-only functions
    LWMTUtilID      (*create)(void);
    void            (*destroy)(LWMTUtilID mtid);
    int             (*lock)(LWMTUtilID mtid, int mutexID);
    int             (*unlock)(LWMTUtilID mtid, int mutexID);

    // Thread Group functions
    LWMTGroupID     (*groupCreate)(int count);
    void            (*groupDestroy)(LWMTGroupID mtgrpid);
    int             (*groupLockMutex)(LWMTGroupID mtgrpid,int mutexID);
    int             (*groupUnlockMutex)(LWMTGroupID mtgrpid,int mutexID);
    LWMTThreadID    (*groupAddThread)(LWMTGroupID mtgrpid, LWMTThreadFunc func,int size,void *arg);
    LWMTThreadID    (*groupGetThreadID)(LWMTGroupID mtgrpid, int in_index);
    int             (*groupGetThreadCount)(LWMTGroupID mtgrpid);

    int             (*groupRun)(LWMTGroupID mtgrpid);
    int             (*groupBegin)(LWMTGroupID mtgrpid);
    void            (*groupSync)(LWMTGroupID mtgrpid);
    void            (*groupAbort)(LWMTGroupID mtgrpid);
    void            (*groupKill)(LWMTGroupID mtgrpid);
    int             (*groupIsDone)(LWMTGroupID mtgrpid);
    int             (*groupIsAborted)(LWMTGroupID mtgrpid);
    int             (*groupThreadResult)(LWMTGroupID mtgrpid,int in_index);

    // Thread functions
    LWMTThreadID    (*threadGetID)(void);
    void            (*threadSetData)(void* ptr);
    void*           (*threadGetData)(void);
    void*           (*threadGetArg)(void);
    void*           (*threadGetArgByID)(LWMTThreadID thrdid);
    int             (*threadGetIndex)(void);
    int             (*threadGetIndexByID)(LWMTThreadID thrdid);
    int             (*threadGetThreadCount)(LWMTThreadID thrdid);
    LWMTGroupID     (*threadGetGroupID)(LWMTThreadID thrdid);
    int             (*threadCheckAbort)(void);
    int             (*threadCheckAbortByID)(LWMTThreadID thrdid);
    void            (*threadSleep)(int delay);
    void            (*threadSetArg)(void* ptr);
    void            (*threadSetIndex)(int in_index);

    // Rev 2 addition functions.
    int             (*groupWait)(LWMTGroupID mtgrpid,unsigned int);

    LWMTRWLockID    (*rwlockCreate)(void);
    void            (*rwlockDestroy)(LWMTRWLockID rwlock);
    void            (*rwlockReadLock)(LWMTRWLockID rwlock);
    int             (*rwlockReadLockTimeout)(LWMTRWLockID rwlock, unsigned int spintries);
    void            (*rwlockReadUnlock)(LWMTRWLockID rwlock);
    void            (*rwlockWriteLock)(LWMTRWLockID rwlock);
    void            (*rwlockWriteUnlock)(LWMTRWLockID rwlock);
    void            (*rwlockWriteToReadLock)(LWMTRWLockID rwlock);
} LWMTUtilFuncs;

#endif

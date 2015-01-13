#ifndef LWSDK_TIMER_H
#define LWSDK_TIMER_H

/*
 * LWSDK Header File
 * Copyright 2008, NewTek, Inc.
 *
 * LWTIMER.H -- LightWave Timer
 *
 * This header provides an interface to LightWave's internal timer mechanism
 */

#define LWTIMER_GLOBAL "LW Timer"

/*
 * Signature for the timer event function.  The function takes a pointer
 * to the data provided when the timer was added (see timerAdd()), and
 * returns a Boolean 1 or 0 to indicate whether or not this particular
 * timer should be removed from the internal timer list.  If a timer
 * implicitly removes itself by returning 1, a subsequent call to
 * timerRemove() is not needed for this instance.
 */

typedef int (*TimerEventProc)(void *);

typedef struct st_LWTimer
{
    /*
     * proc - callback function for timer expiration
     * data - user-defined data pointer
     * interval - timeout value expressed in milliseconds
     */

    void    (*timerAdd)(TimerEventProc proc,void *data,unsigned long interval);

    /*
     * Timers can be explicitly removed by calling the following function and
     * specifying the 'proc' callback as well as an optional interval.  If interval
     * is 0, then the first instance of a timer with 'proc' as its callback will be
     * removed.   If interval is >0, then only the 'proc' instance with that interval
     * will be removed.
     *
     * Unless the timer instance implicitly removes itself by returning 1 from its
     * callback function, each call to timerAdd() MUST HAVE a corresponding call to
     * timerRemove(),  Otherwise you run the risk of leaving a dangling function
     * pointer within Layout, which can (and probably will) eventually lead to
     * a crash of the application.
     *
     * proc - callback function for timer expiration (same used for timerAdd())
     * interval - timeout value expressed in milliseconds (same used for timerAdd())
     */

    void    (*timerRemove)(TimerEventProc proc,unsigned long interval);
} LWTimer;

#endif

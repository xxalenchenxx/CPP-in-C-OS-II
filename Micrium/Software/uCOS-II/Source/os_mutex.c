/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*                                  MUTUAL EXCLUSION SEMAPHORE MANAGEMENT
*
*                           (c) Copyright 1992-2017; Micrium, Inc.; Weston; FL
*                                           All Rights Reserved
*
* File    : OS_MUTEX.C
* By      : Jean J. Labrosse
* Version : V2.92.13
*
* LICENSING TERMS:
* ---------------
*   uC/OS-II is provided in source form for FREE evaluation, for educational use or for peaceful research.
* If you plan on using  uC/OS-II  in a commercial product you need to contact Micrium to properly license
* its use in your product. We provide ALL the source code for your convenience and to help you experience
* uC/OS-II.   The fact that the  source is provided does  NOT  mean that you can use it without  paying a
* licensing fee.
*
* Knowledge of the source code may NOT be used to develop a similar product.
*
* Please help us continue to provide the embedded community with the finest software available.
* Your honesty is greatly appreciated.
*
* You can find our product's user manual, API reference, release notes and
* more information at https://doc.micrium.com.
* You can contact us at www.micrium.com.
*********************************************************************************************************
*/

#define  MICRIUM_SOURCE

#ifndef  OS_MASTER_FILE
#include <ucos_ii.h>
#endif


#if OS_MUTEX_EN > 0u
/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

#define  OS_MUTEX_KEEP_LOWER_8   ((INT16U)0x00FFu)
#define  OS_MUTEX_KEEP_UPPER_8   ((INT16U)0xFF00u)

#define  OS_MUTEX_AVAILABLE      ((INT16U)0x00FFu)

/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/

static  void  OSMutex_RdyAtPrio(OS_TCB *ptcb, INT8U prio);


/*
*********************************************************************************************************
*                                  ACCEPT MUTUAL EXCLUSION SEMAPHORE
*
* Description: This  function checks the mutual exclusion semaphore to see if a resource is available.
*              Unlike OSMutexPend(), OSMutexAccept() does not suspend the calling task if the resource is
*              not available or the event did not occur.
*
* Arguments  : pevent     is a pointer to the event control block
*
*              perr       is a pointer to an error code which will be returned to your application:
*                            OS_ERR_NONE         if the call was successful.
*                            OS_ERR_EVENT_TYPE   if 'pevent' is not a pointer to a mutex
*                            OS_ERR_PEVENT_NULL  'pevent' is a NULL pointer
*                            OS_ERR_PEND_ISR     if you called this function from an ISR
*                            OS_ERR_PCP_LOWER    If the priority of the task that owns the Mutex is
*                                                HIGHER (i.e. a lower number) than the PCP.  This error
*                                                indicates that you did not set the PCP higher (lower
*                                                number) than ALL the tasks that compete for the Mutex.
*                                                Unfortunately, this is something that could not be
*                                                detected when the Mutex is created because we don't know
*                                                what tasks will be using the Mutex.
*
* Returns    : == OS_TRUE    if the resource is available, the mutual exclusion semaphore is acquired
*              == OS_FALSE   a) if the resource is not available
*                            b) you didn't pass a pointer to a mutual exclusion semaphore
*                            c) you called this function from an ISR
*
* Warning(s) : This function CANNOT be called from an ISR because mutual exclusion semaphores are
*              intended to be used by tasks only.
*********************************************************************************************************
*/

#if OS_MUTEX_ACCEPT_EN > 0u
BOOLEAN  OSMutexAccept (OS_EVENT  *pevent,
                        INT8U     *perr)
{
    INT8U      pcp;                                    /* Priority Ceiling Priority (PCP)              */
#if OS_CRITICAL_METHOD == 3u                           /* Allocate storage for CPU status register     */
    OS_CPU_SR  cpu_sr = 0u;
#endif



#ifdef OS_SAFETY_CRITICAL
    if (perr == (INT8U *)0) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        return (OS_FALSE);
    }
#endif

#if OS_ARG_CHK_EN > 0u
    if (pevent == (OS_EVENT *)0) {                     /* Validate 'pevent'                            */
        *perr = OS_ERR_PEVENT_NULL;
        return (OS_FALSE);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {  /* Validate event block type                    */
        *perr = OS_ERR_EVENT_TYPE;
        return (OS_FALSE);
    }
    if (OSIntNesting > 0u) {                           /* Make sure it's not called from an ISR        */
        *perr = OS_ERR_PEND_ISR;
        return (OS_FALSE);
    }
    OS_ENTER_CRITICAL();                               /* Get value (0 or 1) of Mutex                  */
    pcp = (INT8U)(pevent->OSEventCnt >> 8u);           /* Get PCP from mutex                           */
    if ((pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;   /*      Mask off LSByte (Acquire Mutex)         */
        pevent->OSEventCnt |= OSTCBCur->OSTCBPrio;     /*      Save current task priority in LSByte    */
        pevent->OSEventPtr  = (void *)OSTCBCur;        /*      Link TCB of task owning Mutex           */
        if ((pcp != OS_PRIO_MUTEX_CEIL_DIS) &&
            (OSTCBCur->OSTCBPrio <= pcp)) {            /*      PCP 'must' have a SMALLER prio ...      */
             OS_EXIT_CRITICAL();                       /*      ... than current task!                  */
            *perr = OS_ERR_PCP_LOWER;
        } else {
             OS_EXIT_CRITICAL();
            *perr = OS_ERR_NONE;
        }
        return (OS_TRUE);
    }
    OS_EXIT_CRITICAL();
    *perr = OS_ERR_NONE;
    return (OS_FALSE);
}
#endif


/*
*********************************************************************************************************
*                                 CREATE A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function creates a mutual exclusion semaphore.
*
* Arguments  : prio          is the priority to use when accessing the mutual exclusion semaphore.  In
*                            other words, when the semaphore is acquired and a higher priority task
*                            attempts to obtain the semaphore then the priority of the task owning the
*                            semaphore is raised to this priority.  It is assumed that you will specify
*                            a priority that is LOWER in value than ANY of the tasks competing for the
*                            mutex. If the priority is specified as OS_PRIO_MUTEX_CEIL_DIS, then the
*                            priority ceiling promotion is disabled. This way, the tasks accessing the
*                            semaphore do not have their priority promoted.
*
*              perr          is a pointer to an error code which will be returned to your application:
*                               OS_ERR_NONE                     if the call was successful.
*                               OS_ERR_CREATE_ISR               if you attempted to create a MUTEX from an
*                                                               ISR
*                               OS_ERR_ILLEGAL_CREATE_RUN_TIME  if you tried to create a mutex after
*                                                               safety critical operation started.
*                               OS_ERR_PRIO_EXIST               if a task at the priority ceiling priority
*                                                               already exist.
*                               OS_ERR_PEVENT_NULL              No more event control blocks available.
*                               OS_ERR_PRIO_INVALID             if the priority you specify is higher that
*                                                               the maximum allowed (i.e. > OS_LOWEST_PRIO)
*
* Returns    : != (void *)0  is a pointer to the event control clock (OS_EVENT) associated with the
*                            created mutex.
*              == (void *)0  if an error is detected.
*
* Note(s)    : 1) The LEAST significant 8 bits of '.OSEventCnt' hold the priority number of the task
*                 owning the mutex or 0xFF if no task owns the mutex.
*
*              2) The MOST  significant 8 bits of '.OSEventCnt' hold the priority number used to
*                 reduce priority inversion or 0xFF (OS_PRIO_MUTEX_CEIL_DIS) if priority ceiling
*                 promotion is disabled.
*********************************************************************************************************
*/

OS_EVENT  *OSMutexCreate (INT8U   prio,
                          INT8U  *perr)
{
    OS_EVENT  *pevent;
#if OS_CRITICAL_METHOD == 3u                               /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr = 0u;
#endif



#ifdef OS_SAFETY_CRITICAL
    if (perr == (INT8U *)0) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        return ((OS_EVENT *)0);
    }
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
    if (OSSafetyCriticalStartFlag == OS_TRUE) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        *perr = OS_ERR_ILLEGAL_CREATE_RUN_TIME;
        return ((OS_EVENT *)0);
    }
#endif

#if OS_ARG_CHK_EN > 0u
    if (prio != OS_PRIO_MUTEX_CEIL_DIS) {
        if (prio >= OS_LOWEST_PRIO) {                      /* Validate PCP                             */
           *perr = OS_ERR_PRIO_INVALID;
            return ((OS_EVENT *)0);
        }
    }
#endif
    if (OSIntNesting > 0u) {                               /* See if called from ISR ...               */
        *perr = OS_ERR_CREATE_ISR;                         /* ... can't CREATE mutex from an ISR       */
        return ((OS_EVENT *)0);
    }
    OS_ENTER_CRITICAL();
    if (prio != OS_PRIO_MUTEX_CEIL_DIS) {
        if (OSTCBPrioTbl[prio] != (OS_TCB *)0) {           /* Mutex priority must not already exist    */
            OS_EXIT_CRITICAL();                            /* Task already exist at priority ...       */
           *perr = OS_ERR_PRIO_EXIST;                      /* ... ceiling priority                     */
            return ((OS_EVENT *)0);
        }
        OSTCBPrioTbl[prio] = OS_TCB_RESERVED;              /* Reserve the table entry                  */
    }

    pevent = OSEventFreeList;                              /* Get next free event control block        */
    if (pevent == (OS_EVENT *)0) {                         /* See if an ECB was available              */
        if (prio != OS_PRIO_MUTEX_CEIL_DIS) {
            OSTCBPrioTbl[prio] = (OS_TCB *)0;              /* No, Release the table entry              */
        }
        OS_EXIT_CRITICAL();
       *perr = OS_ERR_PEVENT_NULL;                         /* No more event control blocks             */
        return (pevent);
    }
    OSEventFreeList     = (OS_EVENT *)OSEventFreeList->OSEventPtr; /* Adjust the free list             */
    OS_EXIT_CRITICAL();
    pevent->OSEventType = OS_EVENT_TYPE_MUTEX;
    pevent->OSEventCnt  = (INT16U)((INT16U)prio << 8u) | OS_MUTEX_AVAILABLE; /* Resource is avail.     */
    pevent->OSEventPtr  = (void *)0;                       /* No task owning the mutex                 */
#if OS_EVENT_NAME_EN > 0u
    pevent->OSEventName = (INT8U *)(void *)"?";
#endif
    OS_EventWaitListInit(pevent);
    OS_TRACE_MUTEX_CREATE(pevent, pevent->OSEventName);
   *perr = OS_ERR_NONE;
    return (pevent);
}


/*
*********************************************************************************************************
*                                           DELETE A MUTEX
*
* Description: This function deletes a mutual exclusion semaphore and readies all tasks pending on the it.
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired mutex.
*
*              opt           determines delete options as follows:
*                            opt == OS_DEL_NO_PEND   Delete mutex ONLY if no task pending
*                            opt == OS_DEL_ALWAYS    Deletes the mutex even if tasks are waiting.
*                                                    In this case, all the tasks pending will be readied.
*
*              perr          is a pointer to an error code that can contain one of the following values:
*                            OS_ERR_NONE                  The call was successful and the mutex was deleted
*                            OS_ERR_DEL_ISR               If you attempted to delete the MUTEX from an ISR
*                            OS_ERR_INVALID_OPT           An invalid option was specified
*                            OS_ERR_ILLEGAL_DEL_RUN_TIME  If you tried to delete a mutex after safety
*                                                         critical operation started.
*                            OS_ERR_TASK_WAITING          One or more tasks were waiting on the mutex
*                            OS_ERR_EVENT_TYPE            If you didn't pass a pointer to a mutex
*                            OS_ERR_PEVENT_NULL           If 'pevent' is a NULL pointer.
*
* Returns    : pevent        upon error
*              (OS_EVENT *)0 if the mutex was successfully deleted.
*
* Note(s)    : 1) This function must be used with care.  Tasks that would normally expect the presence of
*                 the mutex MUST check the return code of OSMutexPend().
*
*              2) This call can potentially disable interrupts for a long time.  The interrupt disable
*                 time is directly proportional to the number of tasks waiting on the mutex.
*
*              3) Because ALL tasks pending on the mutex will be readied, you MUST be careful because the
*                 resource(s) will no longer be guarded by the mutex.
*
*              4) IMPORTANT: In the 'OS_DEL_ALWAYS' case, we assume that the owner of the Mutex (if there
*                            is one) is ready-to-run and is thus NOT pending on another kernel object or
*                            has delayed itself.  In other words, if a task owns the mutex being deleted,
*                            that task will be made ready-to-run at its original priority.
*********************************************************************************************************
*/

#if OS_MUTEX_DEL_EN > 0u
OS_EVENT  *OSMutexDel (OS_EVENT  *pevent,
                       INT8U      opt,
                       INT8U     *perr)
{
    BOOLEAN    tasks_waiting;
    OS_EVENT  *pevent_return;
    INT8U      pcp;                                        /* Priority ceiling priority                */
    INT8U      prio;
    OS_TCB    *ptcb;
#if OS_CRITICAL_METHOD == 3u                               /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr = 0u;
#endif


#ifdef OS_SAFETY_CRITICAL
    if (perr == (INT8U *)0) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        return ((OS_EVENT *)0);
    }
#endif

#ifdef OS_SAFETY_CRITICAL_IEC61508
    if (OSSafetyCriticalStartFlag == OS_TRUE) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        *perr = OS_ERR_ILLEGAL_DEL_RUN_TIME;
        return ((OS_EVENT *)0);
    }
#endif

#if OS_ARG_CHK_EN > 0u
    if (pevent == (OS_EVENT *)0) {                         /* Validate 'pevent'                        */
        *perr = OS_ERR_PEVENT_NULL;
        return (pevent);
    }
#endif

    OS_TRACE_MUTEX_DEL_ENTER(pevent, opt);

    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {      /* Validate event block type                */
        *perr = OS_ERR_EVENT_TYPE;
        OS_TRACE_MUTEX_DEL_EXIT(*perr);
        return (pevent);
    }
    if (OSIntNesting > 0u) {                               /* See if called from ISR ...               */
        *perr = OS_ERR_DEL_ISR;                            /* ... can't DELETE from an ISR             */
        OS_TRACE_MUTEX_DEL_EXIT(*perr);
        return (pevent);
    }
    OS_ENTER_CRITICAL();
    if (pevent->OSEventGrp != 0u) {                        /* See if any tasks waiting on mutex        */
        tasks_waiting = OS_TRUE;                           /* Yes                                      */
    } else {
        tasks_waiting = OS_FALSE;                          /* No                                       */
    }
    switch (opt) {
        case OS_DEL_NO_PEND:                               /* DELETE MUTEX ONLY IF NO TASK WAITING --- */
             if (tasks_waiting == OS_FALSE) {
#if OS_EVENT_NAME_EN > 0u
                 pevent->OSEventName   = (INT8U *)(void *)"?";
#endif
                 pcp                   = (INT8U)(pevent->OSEventCnt >> 8u);
                 if (pcp != OS_PRIO_MUTEX_CEIL_DIS) {
                     OSTCBPrioTbl[pcp] = (OS_TCB *)0;      /* Free up the PCP                          */
                 }
                 pevent->OSEventType   = OS_EVENT_TYPE_UNUSED;
                 pevent->OSEventPtr    = OSEventFreeList;  /* Return Event Control Block to free list  */
                 pevent->OSEventCnt    = 0u;
                 OSEventFreeList       = pevent;
                 OS_EXIT_CRITICAL();
                 *perr                 = OS_ERR_NONE;
                 pevent_return         = (OS_EVENT *)0;    /* Mutex has been deleted                   */
             } else {
                 OS_EXIT_CRITICAL();
                 *perr                 = OS_ERR_TASK_WAITING;
                 pevent_return         = pevent;
             }
             break;

        case OS_DEL_ALWAYS:                                /* ALWAYS DELETE THE MUTEX ---------------- */
             pcp  = (INT8U)(pevent->OSEventCnt >> 8u);                       /* Get PCP of mutex       */
             if (pcp != OS_PRIO_MUTEX_CEIL_DIS) {
                 prio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8); /* Get owner's orig prio  */
                 ptcb = (OS_TCB *)pevent->OSEventPtr;
                 if (ptcb != (OS_TCB *)0) {                /* See if any task owns the mutex           */
                     if (ptcb->OSTCBPrio == pcp) {         /* See if original prio was changed         */
                         OS_TRACE_MUTEX_TASK_PRIO_DISINHERIT(OSTCBCur, prio);
                         OSMutex_RdyAtPrio(ptcb, prio);    /* Yes, Restore the task's original prio    */
                     }
                 }
             }
             while (pevent->OSEventGrp != 0u) {            /* Ready ALL tasks waiting for mutex        */
                 (void)OS_EventTaskRdy(pevent, (void *)0, OS_STAT_MUTEX, OS_STAT_PEND_ABORT);
             }
#if OS_EVENT_NAME_EN > 0u
             pevent->OSEventName   = (INT8U *)(void *)"?";
#endif
             pcp                   = (INT8U)(pevent->OSEventCnt >> 8u);
             if (pcp != OS_PRIO_MUTEX_CEIL_DIS) {
                 OSTCBPrioTbl[pcp] = (OS_TCB *)0;          /* Free up the PCP                          */
             }
             pevent->OSEventType   = OS_EVENT_TYPE_UNUSED;
             pevent->OSEventPtr    = OSEventFreeList;      /* Return Event Control Block to free list  */
             pevent->OSEventCnt    = 0u;
             OSEventFreeList       = pevent;               /* Get next free event control block        */
             OS_EXIT_CRITICAL();
             if (tasks_waiting == OS_TRUE) {               /* Reschedule only if task(s) were waiting  */
                 OS_Sched();                               /* Find highest priority task ready to run  */
             }
             *perr         = OS_ERR_NONE;
             pevent_return = (OS_EVENT *)0;                /* Mutex has been deleted                   */
             break;

        default:
             OS_EXIT_CRITICAL();
             *perr         = OS_ERR_INVALID_OPT;
             pevent_return = pevent;
             break;
    }

    OS_TRACE_MUTEX_DEL_EXIT(*perr);
    
    return (pevent_return);
}
#endif


/*
*********************************************************************************************************
*                                 PEND ON MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function waits for a mutual exclusion semaphore.
*
* Arguments  : pevent        is a pointer to the event control block associated with the desired
*                            mutex.
*
*              timeout       is an optional timeout period (in clock ticks).  If non-zero, your task will
*                            wait for the resource up to the amount of time specified by this argument.
*                            If you specify 0, however, your task will wait forever at the specified
*                            mutex or, until the resource becomes available.
*
*              perr          is a pointer to where an error message will be deposited.  Possible error
*                            messages are:
*                               OS_ERR_NONE        The call was successful and your task owns the mutex
*                               OS_ERR_TIMEOUT     The mutex was not available within the specified 'timeout'.
*                               OS_ERR_PEND_ABORT  The wait on the mutex was aborted.
*                               OS_ERR_EVENT_TYPE  If you didn't pass a pointer to a mutex
*                               OS_ERR_PEVENT_NULL 'pevent' is a NULL pointer
*                               OS_ERR_PEND_ISR    If you called this function from an ISR and the result
*                                                  would lead to a suspension.
*                               OS_ERR_PCP_LOWER   If the priority of the task that owns the Mutex is
*                                                  HIGHER (i.e. a lower number) than the PCP.  This error
*                                                  indicates that you did not set the PCP higher (lower
*                                                  number) than ALL the tasks that compete for the Mutex.
*                                                  Unfortunately, this is something that could not be
*                                                  detected when the Mutex is created because we don't know
*                                                  what tasks will be using the Mutex.
*                               OS_ERR_PEND_LOCKED If you called this function when the scheduler is locked
*
* Returns    : none
*
* Note(s)    : 1) The task that owns the Mutex MUST NOT pend on any other event while it owns the mutex.
*
*              2) You MUST NOT change the priority of the task that owns the mutex
*********************************************************************************************************
*/

void  OSMutexPend (OS_EVENT  *pevent,
                   INT32U     timeout,
                   INT8U     *perr)
{
    INT8U      pcp;                                        /* Priority Ceiling Priority (PCP)          */
    INT8U      mprio;                                      /* Mutex owner priority                     */
    BOOLEAN    rdy;                                        /* Flag indicating task was ready           */
    OS_TCB    *ptcb;
    OS_EVENT  *pevent2;
    INT8U      y;
#if OS_CRITICAL_METHOD == 3u                               /* Allocate storage for CPU status register */
    OS_CPU_SR  cpu_sr = 0u;
#endif


#ifdef OS_SAFETY_CRITICAL
    if (perr == (INT8U *)0) {
        OS_SAFETY_CRITICAL_EXCEPTION();
        return;
    }
#endif

#if OS_ARG_CHK_EN > 0u
    if (pevent == (OS_EVENT *)0) {                         /* Validate 'pevent'                        */
        *perr = OS_ERR_PEVENT_NULL;
        return;
    }
#endif

    OS_TRACE_MUTEX_PEND_ENTER(pevent, timeout);

    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {      /* Validate event block type                */
        *perr = OS_ERR_EVENT_TYPE;
        OS_TRACE_MUTEX_PEND_EXIT(*perr);
        return;
    }
    if (OSIntNesting > 0u) {                               /* See if called from ISR ...               */
        *perr = OS_ERR_PEND_ISR;                           /* ... can't PEND from an ISR               */
        OS_TRACE_MUTEX_PEND_EXIT(*perr);
        return;
    }
    if (OSLockNesting > 0u) {                              /* See if called with scheduler locked ...  */
        *perr = OS_ERR_PEND_LOCKED;                        /* ... can't PEND when locked               */
        OS_TRACE_MUTEX_PEND_EXIT(*perr);
        return;
    }

    OS_ENTER_CRITICAL();
    pcp = (INT8U)(pevent->OSEventCnt >> 8u);               /* Get PCP from mutex                       */
    //檢查這個R是否是空的
    /* Is Mutex available?                      */
    if ((INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8) == OS_MUTEX_AVAILABLE) {

		//當這個mutex沒人使用，把這個mutex的owner設成當前的task
		//紀錄這個mutex的owner的原始priority..方便之後Pend回去
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;       /* Yes, Acquire the resource                */
        pevent->OSEventCnt |= OSTCBCur->OSTCBOriPrio;      /*      Save priority of owning task        */
        pevent->OSEventPtr  = (void *)OSTCBCur;            /*      Point to owning task's OS_TCB       */


        //if ((pcp != OS_PRIO_MUTEX_CEIL_DIS) &&
        //    (OSTCBCur->OSTCBPrio <= pcp)) {                /*      PCP 'must' have a SMALLER prio ...  */
        //     OS_EXIT_CRITICAL();                           /*      ... than current task!              */
        //    *perr = OS_ERR_PCP_LOWER;
        //} else {
        //     OS_EXIT_CRITICAL();
        //    *perr = OS_ERR_NONE;
        //}
        OS_EXIT_CRITICAL();
        *perr = OS_ERR_NONE;
		//把task的priority設定成該mutex的priority(NPCS)
		//移除原本的task的ready狀態，之後會改成更高的priority
        ptcb = (OS_TCB*)(pevent->OSEventPtr);
        y = ptcb->OSTCBY;
        OSRdyTbl[y] &= (OS_PRIO)~ptcb->OSTCBBitX;     /*     Yes, Remove owner from Rdy ...*/
        if (OSRdyTbl[y] == 0u) {                      /*          ... list at current prio */
            OSRdyGrp &= (OS_PRIO)~ptcb->OSTCBBitY;
        }

		TaskParameter[ptcb->OSTCBOriPrio / T_start - 1].Now_TaskPriority = pcp; //紀錄這個mutex的owner擁有mutex後的priority
        ptcb->OSTCBPrio = pcp;                         /* Change owner task prio to PCP            */
        OS_TRACE_MUTEX_TASK_PRIO_INHERIT(ptcb, pcp);
        //mutex owner的提高的Priority Table設定成ready
        ptcb->OSTCBY = (INT8U)(ptcb->OSTCBPrio >> 3u);
        ptcb->OSTCBX = (INT8U)(ptcb->OSTCBPrio & 0x07u);
        //bitmask也更新
        ptcb->OSTCBBitY = (OS_PRIO)(1uL << ptcb->OSTCBY);
        ptcb->OSTCBBitX = (OS_PRIO)(1uL << ptcb->OSTCBX);
        OSRdyGrp |= ptcb->OSTCBBitY; /* ... make it ready at new priority.   */
        OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
		//PrioTable也更新成mutex owner的TCB
        OSTCBPrioTbl[pcp] = ptcb;

		//要sched? 要，得再看一下context不會被換走
        OS_Sched();



        OS_TRACE_MUTEX_PEND_EXIT(*perr);
        return;
    }
	//當這個R有owner的時候
    if (pcp != OS_PRIO_MUTEX_CEIL_DIS) {
		//當別的task要使用這個mutex的時候，會把這個mutex的owner的priority提升到pcp
        mprio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8); /*  Get priority of mutex owner   */
        ptcb  = (OS_TCB *)(pevent->OSEventPtr);                   /*     Point to TCB of mutex owner   */
        //printf("%d check pcp owner pcp %d and mprio: %d \n",OSTime, pcp, mprio);
        if (ptcb->OSTCBPrio > pcp) {                              /*     Need to promote prio of owner?*/
            if (mprio > OSTCBCur->OSTCBPrio) {
                y = ptcb->OSTCBY;
                if ((OSRdyTbl[y] & ptcb->OSTCBBitX) != 0u) {      /*     See if mutex owner is ready 如果她是準備好的  */
					//清除原本的owner的ready狀態，之後會改成更高的priority
                    //printf("%d mprio: %d  ready\n", OSTime, mprio);
                    OSRdyTbl[y] &= (OS_PRIO)~ptcb->OSTCBBitX;     /*     Yes, Remove owner from Rdy ...*/
                    if (OSRdyTbl[y] == 0u) {                      /*          ... list at current prio */
                        OSRdyGrp &= (OS_PRIO)~ptcb->OSTCBBitY;
                    }
                    rdy = OS_TRUE;
                } else {
                    pevent2 = ptcb->OSTCBEventPtr;
                    if (pevent2 != (OS_EVENT *)0) {               /* Remove from event wait list       */
                        y = ptcb->OSTCBY;
                        pevent2->OSEventTbl[y] &= (OS_PRIO)~ptcb->OSTCBBitX;
                        if (pevent2->OSEventTbl[y] == 0u) {
                            pevent2->OSEventGrp &= (OS_PRIO)~ptcb->OSTCBBitY;
                        }
                    }
                    rdy = OS_FALSE;                        /* No                                       */
                }
				//把他的priority提升到pcp!!!
                //printf("%d owner task: %d prio: %d -> %d\n",OSTime, ptcb->OSTCBId, ptcb->OSTCBPrio, pcp);
                ptcb->OSTCBPrio = pcp;                     /* Change owner task prio to PCP            */
                
                OS_TRACE_MUTEX_TASK_PRIO_INHERIT(ptcb, pcp);
                
#if OS_LOWEST_PRIO <= 63u
				//mutex owner的提高的Priority Table設定成ready
                ptcb->OSTCBY    = (INT8U)( ptcb->OSTCBPrio >> 3u);
                ptcb->OSTCBX    = (INT8U)( ptcb->OSTCBPrio & 0x07u);

                //printf("%d set owner task: %d Y: %d X: %d \n", OSTime, ptcb->OSTCBId, ptcb->OSTCBY, ptcb->OSTCBX);
#else
                ptcb->OSTCBY    = (INT8U)((INT8U)(ptcb->OSTCBPrio >> 4u) & 0xFFu);
                ptcb->OSTCBX    = (INT8U)( ptcb->OSTCBPrio & 0x0Fu);
#endif
                ptcb->OSTCBBitY = (OS_PRIO)(1uL << ptcb->OSTCBY);
                ptcb->OSTCBBitX = (OS_PRIO)(1uL << ptcb->OSTCBX);
                //printf("%d set owner task: %d OSTCBBitY: %d OSTCBBitX: %d \n", OSTime, ptcb->OSTCBId, ptcb->OSTCBBitY, ptcb->OSTCBBitX);

                if (rdy == OS_TRUE) {                      /* If task was ready at owner's priority ...*/
                    OSRdyGrp               |= ptcb->OSTCBBitY; /* ... make it ready at new priority.   */
                    OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
                    //printf("%d set owner task: %d ready ptcb->OSTCBPrio: %d pcp: %d  Y: %d X: %d OSRDY: %d\n", OSTime, ptcb->OSTCBId, ptcb->OSTCBPrio, pcp, ptcb->OSTCBBitY, ptcb->OSTCBBitX,(ptcb->OSTCBBitY<<3)+ ptcb->OSTCBBitX);
                } else {
                    pevent2 = ptcb->OSTCBEventPtr;
                    if (pevent2 != (OS_EVENT *)0) {        /* Add to event wait list                   */
                        pevent2->OSEventGrp               |= ptcb->OSTCBBitY;
                        pevent2->OSEventTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
                    }
                }
                OSTCBPrioTbl[pcp] = ptcb;
            }
        }
    }
    OSTCBCur->OSTCBStat     |= OS_STAT_MUTEX;         /* Mutex not available, pend current task        */
    OSTCBCur->OSTCBStatPend  = OS_STAT_PEND_OK;
    OSTCBCur->OSTCBDly       = timeout;               /* Store timeout in current task's TCB           */
    OS_EventTaskWait(pevent);                         /* Suspend task until event or timeout occurs    */
    OS_EXIT_CRITICAL();
    OS_Sched();                                       /* Find next highest priority task ready         */
    OS_ENTER_CRITICAL();
    switch (OSTCBCur->OSTCBStatPend) {                /* See if we timed-out or aborted                */
        case OS_STAT_PEND_OK:
             *perr = OS_ERR_NONE;
             break;

        case OS_STAT_PEND_ABORT:
             *perr = OS_ERR_PEND_ABORT;               /* Indicate that we aborted getting mutex        */
             break;

        case OS_STAT_PEND_TO:
        default:
             OS_EventTaskRemove(OSTCBCur, pevent);
             *perr = OS_ERR_TIMEOUT;                  /* Indicate that we didn't get mutex within TO   */
             break;
    }
    OSTCBCur->OSTCBStat          =  OS_STAT_RDY;      /* Set   task  status to ready                   */
    OSTCBCur->OSTCBStatPend      =  OS_STAT_PEND_OK;  /* Clear pend  status                            */
    OSTCBCur->OSTCBEventPtr      = (OS_EVENT  *)0;    /* Clear event pointers                          */
#if (OS_EVENT_MULTI_EN > 0u)
    OSTCBCur->OSTCBEventMultiPtr = (OS_EVENT **)0;
#endif
    OS_EXIT_CRITICAL();

    OS_TRACE_MUTEX_PEND_EXIT(*perr);
}


/*
*********************************************************************************************************
*                                POST TO A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function signals a mutual exclusion semaphore
*
* Arguments  : pevent              is a pointer to the event control block associated with the desired
*                                  mutex.
*
* Returns    : OS_ERR_NONE             The call was successful and the mutex was signaled.
*              OS_ERR_EVENT_TYPE       If you didn't pass a pointer to a mutex
*              OS_ERR_PEVENT_NULL      'pevent' is a NULL pointer
*              OS_ERR_POST_ISR         Attempted to post from an ISR (not valid for MUTEXes)
*              OS_ERR_NOT_MUTEX_OWNER  The task that did the post is NOT the owner of the MUTEX.
*              OS_ERR_PCP_LOWER        If the priority of the new task that owns the Mutex is
*                                      HIGHER (i.e. a lower number) than the PCP.  This error
*                                      indicates that you did not set the PCP higher (lower
*                                      number) than ALL the tasks that compete for the Mutex.
*                                      Unfortunately, this is something that could not be
*                                      detected when the Mutex is created because we don't know
*                                      what tasks will be using the Mutex.
*********************************************************************************************************
*/

INT8U  OSMutexPost (OS_EVENT *pevent)
{
    INT8U      pcp;                                   /* Priority ceiling priority                     */
    INT8U      prio;
#if OS_CRITICAL_METHOD == 3u                          /* Allocate storage for CPU status register      */
    OS_CPU_SR  cpu_sr = 0u;
#endif


    if (OSIntNesting > 0u) {                          /* See if called from ISR ...                    */
        return (OS_ERR_POST_ISR);                     /* ... can't POST mutex from an ISR              */
    }
#if OS_ARG_CHK_EN > 0u
    if (pevent == (OS_EVENT *)0) {                    /* Validate 'pevent'                             */
        return (OS_ERR_PEVENT_NULL);
    }
#endif

    OS_TRACE_MUTEX_POST_ENTER(pevent);

    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) { /* Validate event block type                     */
        OS_TRACE_MUTEX_POST_EXIT(OS_ERR_EVENT_TYPE);
        return (OS_ERR_EVENT_TYPE);
    }
    OS_ENTER_CRITICAL();
    pcp  = (INT8U)(pevent->OSEventCnt >> 8u);         /* Get priority ceiling priority of mutex        */
    prio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);  /* Get owner's original priority      */
    if (OSTCBCur != (OS_TCB *)pevent->OSEventPtr) {   /* See if posting task owns the MUTEX            */
        OS_EXIT_CRITICAL();
        OS_TRACE_MUTEX_POST_EXIT(OS_ERR_NOT_MUTEX_OWNER);
        return (OS_ERR_NOT_MUTEX_OWNER);
    }
    if (pcp != OS_PRIO_MUTEX_CEIL_DIS) {
        if (OSTCBCur->OSTCBPrio == pcp) {             /* Did we have to raise current task's priority? */
            OS_TRACE_MUTEX_TASK_PRIO_DISINHERIT(OSTCBCur, prio);
            OSMutex_RdyAtPrio(OSTCBCur, prio);        /* Restore the task's original priority          */
        }
        OSTCBPrioTbl[pcp] = OS_TCB_RESERVED;          /* Reserve table entry                           */		
        
    }
    
    if (pevent->OSEventGrp != 0u) {                   /* Any task waiting for the mutex?               */
                                                      /* Yes, Make HPT waiting for mutex ready         */
        prio                = OS_EventTaskRdy(pevent, (void *)0, OS_STAT_MUTEX, OS_STAT_PEND_OK);
        pevent->OSEventCnt &= OS_MUTEX_KEEP_UPPER_8;  /*      Save priority of mutex's new owner       */
        pevent->OSEventCnt |= prio;
        pevent->OSEventPtr  = OSTCBPrioTbl[prio];     /*      Link to new mutex owner's OS_TCB         */
        if ((pcp  != OS_PRIO_MUTEX_CEIL_DIS) &&
            (prio <= pcp)) {                          /*      PCP 'must' have a SMALLER prio ...       */

            OS_EXIT_CRITICAL();                       /*      ... than current task!                   */
            OS_Sched();                               /*      Find highest priority task ready to run  */
            OS_TRACE_MUTEX_POST_EXIT(OS_ERR_PCP_LOWER);
            return (OS_ERR_PCP_LOWER);
        } else {
            OS_EXIT_CRITICAL();
            OS_Sched();                               /*      Find highest priority task ready to run  */
            OS_TRACE_MUTEX_POST_EXIT(OS_ERR_NONE);
            return (OS_ERR_NONE);
        }
    }
    pevent->OSEventCnt |= OS_MUTEX_AVAILABLE;         /* No,  Mutex is now available                   */
    pevent->OSEventPtr  = (void *)0;

    //Task的priority找看看其他mutex是否擁有這個task；有，nowPrio就設定成該mutex
     //if (OSTCBPrioTbl[R1_ceiling] && OSTCBPrioTbl[R1_ceiling]->OSTCBId == TaskParameter[prio / T_start - 1].TaskID) {
     //    TaskParameter[prio / T_start - 1].Now_TaskPriority = pcp; //紀錄這個mutex的owner擁有mutex後的priority
     //}else if (OSTCBPrioTbl[R2_ceiling] && OSTCBPrioTbl[R2_ceiling]->OSTCBId == TaskParameter[prio / T_start - 1].TaskID){
     //    TaskParameter[prio / T_start - 1].Now_TaskPriority = pcp; //紀錄這個mutex的owner擁有mutex後的priority
     //}else {
     //    TaskParameter[prio / T_start - 1].Now_TaskPriority = prio; //紀錄這個mutex的owner擁有mutex後的priority
     //}
    

    OS_EXIT_CRITICAL();
    OS_TRACE_MUTEX_POST_EXIT(OS_ERR_NONE);
    return (OS_ERR_NONE);
}


/*
*********************************************************************************************************
*                                 QUERY A MUTUAL EXCLUSION SEMAPHORE
*
* Description: This function obtains information about a mutex
*
* Arguments  : pevent          is a pointer to the event control block associated with the desired mutex
*
*              p_mutex_data    is a pointer to a structure that will contain information about the mutex
*
* Returns    : OS_ERR_NONE          The call was successful and the message was sent
*              OS_ERR_QUERY_ISR     If you called this function from an ISR
*              OS_ERR_PEVENT_NULL   If 'pevent'       is a NULL pointer
*              OS_ERR_PDATA_NULL    If 'p_mutex_data' is a NULL pointer
*              OS_ERR_EVENT_TYPE    If you are attempting to obtain data from a non mutex.
*********************************************************************************************************
*/

#if OS_MUTEX_QUERY_EN > 0u
INT8U  OSMutexQuery (OS_EVENT       *pevent,
                     OS_MUTEX_DATA  *p_mutex_data)
{
    INT8U       i;
    OS_PRIO    *psrc;
    OS_PRIO    *pdest;
#if OS_CRITICAL_METHOD == 3u                     /* Allocate storage for CPU status register           */
    OS_CPU_SR   cpu_sr = 0u;
#endif



    if (OSIntNesting > 0u) {                               /* See if called from ISR ...               */
        return (OS_ERR_QUERY_ISR);                         /* ... can't QUERY mutex from an ISR        */
    }
#if OS_ARG_CHK_EN > 0u
    if (pevent == (OS_EVENT *)0) {                         /* Validate 'pevent'                        */
        return (OS_ERR_PEVENT_NULL);
    }
    if (p_mutex_data == (OS_MUTEX_DATA *)0) {              /* Validate 'p_mutex_data'                  */
        return (OS_ERR_PDATA_NULL);
    }
#endif
    if (pevent->OSEventType != OS_EVENT_TYPE_MUTEX) {      /* Validate event block type                */
        return (OS_ERR_EVENT_TYPE);
    }
    OS_ENTER_CRITICAL();
    p_mutex_data->OSMutexPCP  = (INT8U)(pevent->OSEventCnt >> 8u);
    p_mutex_data->OSOwnerPrio = (INT8U)(pevent->OSEventCnt & OS_MUTEX_KEEP_LOWER_8);
    if (p_mutex_data->OSOwnerPrio == 0xFFu) {
        p_mutex_data->OSValue = OS_TRUE;
    } else {
        p_mutex_data->OSValue = OS_FALSE;
    }
    p_mutex_data->OSEventGrp  = pevent->OSEventGrp;        /* Copy wait list                           */
    psrc                      = &pevent->OSEventTbl[0];
    pdest                     = &p_mutex_data->OSEventTbl[0];
    for (i = 0u; i < OS_EVENT_TBL_SIZE; i++) {
        *pdest++ = *psrc++;
    }
    OS_EXIT_CRITICAL();
    return (OS_ERR_NONE);
}
#endif                                                     /* OS_MUTEX_QUERY_EN                        */


/*
*********************************************************************************************************
*                            RESTORE A TASK BACK TO ITS ORIGINAL PRIORITY
*
* Description: This function makes a task ready at the specified priority
*
* Arguments  : ptcb            is a pointer to OS_TCB of the task to make ready
*
*              prio            is the desired priority
*
* Returns    : none
*********************************************************************************************************
*/

static  void  OSMutex_RdyAtPrio (OS_TCB  *ptcb,
                                 INT8U    prio)
{
    INT8U  y;


    y            =  ptcb->OSTCBY;                          /* Remove owner from ready list at 'pcp'    */
    OSRdyTbl[y] &= (OS_PRIO)~ptcb->OSTCBBitX;
    OS_TRACE_TASK_SUSPENDED(ptcb);
    if (OSRdyTbl[y] == 0u) {
        OSRdyGrp &= (OS_PRIO)~ptcb->OSTCBBitY;
    }
    ptcb->OSTCBPrio         = prio;
    OSPrioCur               = prio;                        /* The current task is now at this priority */
#if OS_LOWEST_PRIO <= 63u
    ptcb->OSTCBY            = (INT8U)((INT8U)(prio >> 3u) & 0x07u);
    ptcb->OSTCBX            = (INT8U)(prio & 0x07u);
#else
    ptcb->OSTCBY            = (INT8U)((INT8U)(prio >> 4u) & 0x0Fu);
    ptcb->OSTCBX            = (INT8U) (prio & 0x0Fu);
#endif
    ptcb->OSTCBBitY         = (OS_PRIO)(1uL << ptcb->OSTCBY);
    ptcb->OSTCBBitX         = (OS_PRIO)(1uL << ptcb->OSTCBX);
    OSRdyGrp               |= ptcb->OSTCBBitY;             /* Make task ready at original priority     */
    OSRdyTbl[ptcb->OSTCBY] |= ptcb->OSTCBBitX;
    OSTCBPrioTbl[prio]      = ptcb;
    OS_TRACE_TASK_READY(ptcb);
}


#endif                                                     /* OS_MUTEX_EN                              */

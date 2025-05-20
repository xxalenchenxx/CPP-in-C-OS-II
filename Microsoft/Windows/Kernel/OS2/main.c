/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*
*               You can find our product's user manual, API reference, release notes and
*               more information at https://doc.micrium.com.
*               You can contact us at www.micrium.com.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                              uC/OS-II
*                                            EXAMPLE CODE
*
* Filename : main.c
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <lib_mem.h>
#include  <os.h>

#include  "app_cfg.h"


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#define TASK_STACKSIZE              2048
static  OS_STK  StartupTaskStk[APP_CFG_STARTUP_TASK_STK_SIZE];
/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  StartupTask (void  *p_arg);
static  void  task(void* p_arg);
/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*
* Notes       : none
*********************************************************************************************************
*/
int count1 = 0;
int count2 = 0;
int count = 0;
int idle = 0;

OS_EVENT* R1;
OS_EVENT* R2;
OS_MUTEX_DATA R1_status;
OS_MUTEX_DATA R2_status;
int  main (void)
{
#if OS_TASK_NAME_EN > 0u
    CPU_INT08U  os_err;
#endif



    CPU_IntInit();

    Mem_Init();                                                 /* Initialize Memory Managment Module                   */
    CPU_IntDis();                                               /* Disable all Interrupts                               */
    CPU_Init();                                                 /* Initialize the uC/CPU services                       */

    OSInit();
    OutFileInit();


    InputFile();


    Task_STK = malloc( (TASK_NUMBER+1) * T_start * sizeof(int*));
    // 建立 Mutex
	INT8U err;
    R1 = OSMutexCreate(R1_ceiling, &err);
    R2 = OSMutexCreate(R2_ceiling, &err);

    /*printf("R1_prio: %d\n", R1_PRIO);
	printf("R2_prio: %d\n", R2_PRIO);*/
    int n;
    for (n = 0; n < TASK_NUMBER; n++) {
        Task_STK[n] = malloc(TASK_STACKSIZE * sizeof(int));
    }
    
    
    OSTimeSet(0);

    //PA1-2 read task file
    for (auto i = 0;i < TASK_NUMBER;i++) {
            OSTaskCreateExt(task,                               /* Create the startup task                              */
                &TaskParameter[i],
                &Task_STK[i][TASK_STACKSIZE - 1],
                TaskParameter[i].TaskPriority,
                TaskParameter[i].TaskID,
                &Task_STK[i][0],
                TASK_STACKSIZE,
                &TaskParameter[i],
                (OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));
    }
    
    //Tick0 要delay arrival time
    OS_TCB* p_tcb = OSTCBList;
    while (p_tcb != (OS_TCB*)0) {
		INT8U task_priority = p_tcb->OSTCBOriPrio;
        if ( task_priority != 63u && TaskParameter[task_priority/T_start - 1].TaskArriveTime != 0u) {
            //printf("%d taskID: %d Prio: %d delay %d\n",OSTime, p_tcb->OSTCBId, p_tcb->OSTCBPrio, TaskParameter[task_priority/T_start - 1].TaskArriveTime);
            INT8U y = p_tcb->OSTCBY;        /* Delay current task  */
            OSRdyTbl[y] &= (OS_PRIO)~p_tcb->OSTCBBitX;
            OS_TRACE_TASK_SUSPENDED(p_tcb);
            if (OSRdyTbl[y] == 0u) {
                OSRdyGrp &= (OS_PRIO)~p_tcb->OSTCBBitY;
            }
            p_tcb->OSTCBDly = TaskParameter[task_priority / T_start - 1].TaskArriveTime;              /* Load ticks in TCB                                  */
            OS_TRACE_TASK_DLY(TaskParameter[task_priority / T_start - 1].TaskArriveTime);
        }
        p_tcb = p_tcb->OSTCBNext;
    }


    printf("Tick\tEvent\t\tCurrentTaskID\t\tNextTask ID\tResponse Time\tBlocking Time\tPreemption Time\n");
    
	fclose(Output_fp);
    OSStart();
                                               /* Start multitasking (i.e. give control to uC/OS-II)   */

    while (DEF_ON) {                                            /* Should Never Get Here.                               */
        ;
    }
}


/*
*********************************************************************************************************
*                                            STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'StartupTask()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

//static  void  StartupTask (void *p_arg)
//{
//   (void)p_arg;
//
//    OS_TRACE_INIT();                                            /* Initialize the uC/OS-II Trace recorder               */
//
//#if OS_CFG_STAT_TASK_EN > 0u
//    OSStatTaskCPUUsageInit(&err);                               /* Compute CPU capacity with no task running            */
//#endif
//
//#ifdef CPU_CFG_INT_DIS_MEAS_EN
//    CPU_IntDisMeasMaxCurReset();
//#endif
//    
//    APP_TRACE_DBG(("uCOS-III is Running...\n\r"));
//
//    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
//        OSTimeDlyHMSM(0u, 0u, 0u, 500u);
//		APP_TRACE_DBG(("Time: %2d\n\r", OSTimeGet()));
//    }
//}


void task(void* p_arg) {
    task_para_set* task_data;
    task_data = (task_para_set*)p_arg;
	INT8U err;
    //printf("TICK %d task_data: %d\n",OSTime, task_data->TaskID);
    // 初始到達時間延遲
    /*if (OSTime < task_data->TaskArriveTime) {
        printf("TICK %d task_data: %d delay %d\n", OSTime, task_data->TaskID , task_data->TaskArriveTime - OSTime);
        OSTimeDly(task_data->TaskArriveTime - OSTime);
    }*/

    /*printf("TICK %d task_data: %d TaskArriveTime: %d R1:[%d - %d]  R2: [%d - %d]\n", 
        OSTime, task_data->TaskID, task_data->TaskArriveTime , task_data->R1_start, task_data->R1_end, task_data->R2_start, task_data->R2_end);*/

    INT32U next_period = 0;
	INT16U before_Prio = 0;
    while (1) {
		next_period = task_data->TaskPeriodic * (task_data->TaskNumber+1) + task_data->TaskArriveTime;
 /*       OSMutexQuery(R1, &R1_status);
        OSMutexQuery(R2, &R2_status);*/
        
        int cur_tick = OSTime;
        while (task_data->Task_need_ExecutionTime != 0) {
			if (cur_tick != OSTime) {
				//R1 check Unlock
                if (task_data->R1_start != task_data->R1_end) { //避免鎖了又解鎖 && R2_status.OSValue == OS_TRUE
                    if (task_data->TaskExecutionTime - task_data->Task_need_ExecutionTime == task_data->R1_end) { //&& R2_status.OSValue == OS_FALSE
                        //OS_EXIT_CRITICAL();
                        before_Prio = task_data->Now_TaskPriority;
                        OSMutexPost(R1);
                        task_data->Now_TaskPriority = ((INT8U)(R2->OSEventCnt & 0x00FF) == task_data->TaskPriority) ? R2_ceiling : task_data->TaskPriority;
                        
                        if (err == OS_ERR_NONE && (Output_err = fopen_s(&Output_fp, "./Output.txt", "a")) == 0) {
                            printf("%d\tUnlockResource\ttask( %d)( %d)\tR1 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber,before_Prio,task_data->Now_TaskPriority);
                            fprintf(Output_fp, "%d\tUnlockResource\ttask( %d)( %d)\tR1 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                        }
                        fclose(Output_fp);
                        OS_Sched();
                    }
                }

                //R2 check Unlock
                if (task_data->R2_start != task_data->R2_end) { //避免鎖了又解鎖 && R2_status.OSValue == OS_TRUE
                    if (task_data->TaskExecutionTime - task_data->Task_need_ExecutionTime == task_data->R2_end) { //&& R2_status.OSValue == OS_FALSE
                        //OS_EXIT_CRITICAL();
                        before_Prio = task_data->Now_TaskPriority;
                        OSMutexPost(R2);
                        task_data->Now_TaskPriority = ((INT8U)(R1->OSEventCnt & 0x00FF) == task_data->TaskPriority) ? R1_ceiling : task_data->TaskPriority;
                        if (err == OS_ERR_NONE && (Output_err = fopen_s(&Output_fp, "./Output.txt", "a")) == 0) {
                            printf("%d\tUnlockResource\ttask( %d)( %d)\tR2 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                            fprintf(Output_fp, "%d\tUnlockResource\ttask( %d)( %d)\tR2 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                        }
                        fclose(Output_fp);
                        OS_Sched();
                    }
                }

                //R1 check lock
                if (task_data->R1_start != task_data->R1_end) { //避免鎖了又解鎖 && R2_status.OSValue == OS_TRUE
                    if (task_data->TaskExecutionTime - task_data->Task_need_ExecutionTime == task_data->R1_start) { //該任務做了多少tick後需要lock
						before_Prio = task_data->Now_TaskPriority;
                        OSMutexPend(R1, 0, &err);
                        //OSMutexPend(R2, 0, &err);
                        //OS_ENTER_CRITICAL();
                        if (err == OS_ERR_NONE && (Output_err = fopen_s(&Output_fp, "./Output.txt", "a")) == 0) {
                            printf("%d\tLockResource\ttask( %d)( %d)\tR1 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                            fprintf(Output_fp, "%d\tLockResource\ttask( %d)( %d)\tR1 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                        }
                        fclose(Output_fp);

                    }
                }

                //R2 check lock
                if (task_data->R2_start != task_data->R2_end) { //避免鎖了又解鎖 && R2_status.OSValue == OS_TRUE
                    if (task_data->TaskExecutionTime - task_data->Task_need_ExecutionTime == task_data->R2_start ) { //該任務做了多少tick後需要lock
                        before_Prio = task_data->Now_TaskPriority;
                        OSMutexPend(R2, 0, &err);
                        //OSMutexPend(R2, 0, &err);
                        //OS_ENTER_CRITICAL();
                        if (err == OS_ERR_NONE) {
                            if ((Output_err = fopen_s(&Output_fp, "./Output.txt", "a")) == 0) {
                                printf("%d\tLockResource\ttask( %d)( %d)\tR2 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                                fprintf(Output_fp, "%d\tLockResource\ttask( %d)( %d)\tR2 %d to %d\n", OSTime, task_data->TaskID, task_data->TaskNumber, before_Prio, task_data->Now_TaskPriority);
                            }
                            fclose(Output_fp);
                        }
                    }
                }

                cur_tick = OSTime;
            }
        }

        


        if (task_data->Task_need_ExecutionTime==0) {
            if (next_period - OSTime > 0u) {
                OSTimeDly(next_period - OSTime);
            }
            else {
                OSTaskSwHook();
            }
        }
        
    }
}

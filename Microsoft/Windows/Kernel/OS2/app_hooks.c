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
*                                          Application Hooks
*
* Filename : app_hooks.c
* Version  : V2.92.13
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*/

#include  <os.h>


/*
*********************************************************************************************************
*                                      EXTERN  GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                           LOCAL CONSTANTS
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            LOCAL TABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/


/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/



/*
*********************************************************************************************************
*********************************************************************************************************
**                                         GLOBAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/


void OutFileInit() {
    /*Clear the file*/
    if ((Output_err = fopen_s(&Output_fp, OUTPUT_FILE_NAME, "w")) == 0)
        fclose(Output_fp);
    else
        printf("Error to clear output file");
}

void InputFile(void) {
    errno_t err;
    if ((err = fopen_s(&fp, INPUT_FILE_NAME, "r")) != 0) {
        printf("The file '%s' was not opened\n", INPUT_FILE_NAME);
        return;
    }

    rewind(fp);
    char str[MAX];
    char* ptr, * pTmp = NULL;
    int TaskInfo[INFO];
    int i, j = 0;
    TASK_NUMBER = 0;

    // 讀取每行並解析
    while (fgets(str, sizeof(str), fp) != NULL) {
        ptr = strtok_s(str, " ", &pTmp);
        for (i = 0; i < INFO && ptr != NULL; i++) {
            TaskInfo[i] = atoi(ptr);
            ptr = strtok_s(NULL, " ", &pTmp);
        }
        // 填入結構陣列
        TaskParameter[j].TaskID = TaskInfo[0];
        TaskParameter[j].TaskArriveTime = TaskInfo[1];
        TaskParameter[j].TaskExecutionTime = TaskInfo[2];
        TaskParameter[j].TaskPeriodic = TaskInfo[3];
        TaskParameter[j].R1_start = TaskInfo[4];
        TaskParameter[j].R1_end = TaskInfo[5];
        TaskParameter[j].R2_start = TaskInfo[6];
        TaskParameter[j].R2_end = TaskInfo[7];
        TaskParameter[j].TaskNumber = 0;
        TaskParameter[j].Task_need_ExecutionTime = TaskInfo[2];
        TaskParameter[j].Blocking_T = 0;

        TASK_NUMBER++;
        j++;
    }
    fclose(fp);

    // 使用 Bubble Sort 根據 TaskPeriodic 排序
    for (i = 0; i < TASK_NUMBER - 1; i++) {
        for (int k = 0; k < TASK_NUMBER - i - 1; k++) {
            if (TaskParameter[k].TaskPeriodic > TaskParameter[k + 1].TaskPeriodic) {
                task_para_set tmp = TaskParameter[k];
                TaskParameter[k] = TaskParameter[k + 1];
                TaskParameter[k + 1] = tmp;
            }
        }
    }

    // 排序後依序指定優先權 (index+1)*T_start
    for (i = 0; i < TASK_NUMBER; i++) {
        TaskParameter[i].TaskPriority = (i+1)*T_start;
        TaskParameter[i].Now_TaskPriority = (i + 1) * T_start;
    }

	//新增R1和R2的ceiling
    for (i = 0; i < TASK_NUMBER; i++) {
        if (TaskParameter[i].R1_start != TaskParameter[i].R1_end) {
			R1_ceiling = TaskParameter[i].TaskPriority - R1_PRIO;
            break;
        }
    }

    for (i = 0; i < TASK_NUMBER; i++) {
        if (TaskParameter[i].R2_start != TaskParameter[i].R2_end) {
            R2_ceiling = TaskParameter[i].TaskPriority - R2_PRIO;
            break;
        }
    }
    //printf("%d R1_ceiling: %d\n", OSTime, R1_ceiling);
    //printf("%d R2_ceiling: %d\n", OSTime, R2_ceiling);


}

/*
*********************************************************************************************************
*********************************************************************************************************
**                                        uC/OS-II APP HOOKS
*********************************************************************************************************
*********************************************************************************************************
*/

#if (OS_APP_HOOKS_EN > 0)

/*
*********************************************************************************************************
*                                  TASK CREATION HOOK (APPLICATION)
*
* Description : This function is called when a task is created.
*
* Argument(s) : ptcb   is a pointer to the task control block of the task being created.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*********************************************************************************************************
*/

void  App_TaskCreateHook (OS_TCB *ptcb)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN == DEF_ENABLED) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TaskCreateHook(ptcb);
#endif
}


/*
*********************************************************************************************************
*                                  TASK DELETION HOOK (APPLICATION)
*
* Description : This function is called when a task is deleted.
*
* Argument(s) : ptcb   is a pointer to the task control block of the task being deleted.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*********************************************************************************************************
*/

void  App_TaskDelHook (OS_TCB *ptcb)
{
    (void)ptcb;
}


/*
*********************************************************************************************************
*                                    IDLE TASK HOOK (APPLICATION)
*
* Description : This function is called by OSTaskIdleHook(), which is called by the idle task.  This hook
*               has been added to allow you to do such things as STOP the CPU to conserve power.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts are enabled during this call.
*********************************************************************************************************
*/

#if OS_VERSION >= 251
void  App_TaskIdleHook (void)
{
}
#endif


/*
*********************************************************************************************************
*                                  STATISTIC TASK HOOK (APPLICATION)
*
* Description : This function is called by OSTaskStatHook(), which is called every second by uC/OS-II's
*               statistics task.  This allows your application to add functionality to the statistics task.
*
* Argument(s) : none.
*********************************************************************************************************
*/

void  App_TaskStatHook (void)
{
}


/*
*********************************************************************************************************
*                                   TASK RETURN HOOK (APPLICATION)
*
* Description: This function is called if a task accidentally returns.  In other words, a task should
*              either be an infinite loop or delete itself when done.
*
* Arguments  : ptcb      is a pointer to the task control block of the task that is returning.
*
* Note(s)    : none
*********************************************************************************************************
*/


#if OS_VERSION >= 289
void  App_TaskReturnHook (OS_TCB  *ptcb)
{
    (void)ptcb;
}
#endif


/*
*********************************************************************************************************
*                                   TASK SWITCH HOOK (APPLICATION)
*
* Description : This function is called when a task switch is performed.  This allows you to perform other
*               operations during a context switch.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts are disabled during this call.
*
*               (2) It is assumed that the global pointer 'OSTCBHighRdy' points to the TCB of the task that
*                   will be 'switched in' (i.e. the highest priority task) and, 'OSTCBCur' points to the
*                  task being switched out (i.e. the preempted task).
*********************************************************************************************************
*/

#if OS_TASK_SW_HOOK_EN > 0
void  App_TaskSwHook (void)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN > 0) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TaskSwHook();
#endif
    int CurPrio = OSTCBPrioTbl[OSPrioCur]->OSTCBOriPrio;
    if (CurPrio != OS_TASK_IDLE_PRIO) {
        for (int i = 0; i < TASK_NUMBER; i++) {
            if (CurPrio > TaskParameter[i].TaskPriority) {
                INT32U arrival_time = TaskParameter[i].TaskArriveTime + TaskParameter[i].TaskNumber * TaskParameter[i].TaskPeriodic;
                //printf("%d CurID: %d CheckID: %d Check_arrival_time %d\n", OSTime, OSTCBPrioTbl[OSPrioCur]->OSTCBId, TaskParameter[i].TaskID, arrival_time);
                if (OSTime > arrival_time) {
                    //printf("%d TaskID: %d TaskParameter[i] add %d\n", OSTime, TaskParameter[i].TaskID, (OSTime - arrival_time));
                    TaskParameter[i].Blocking_T += (OSTime - arrival_time); //計算task被block的時間
                }
            }
        }
    }
   


    if ((Output_err = fopen_s(&Output_fp, "./Output.txt", "a")) == 0) {
        if (OSTime) {
            printf("%d", OSTime);
            fprintf(Output_fp, "%d", OSTime);
            int CurPrio = OSTCBPrioTbl[OSPrioCur]->OSTCBOriPrio;
			int HighRdyPrio = OSTCBPrioTbl[OSPrioHighRdy]->OSTCBOriPrio;
            if (TaskParameter[CurPrio/T_start - 1].Task_need_ExecutionTime == 0 && CurPrio != OS_TASK_IDLE_PRIO) { //如果要被切換進來的task是idle task，就print這行
                printf("\tCompletion\t");  //if the task will be done, show this line
                fprintf(Output_fp,"\tCompletion\t");
            }
            else {
                printf("\tPreemption\t");
                fprintf(Output_fp, "\tPreemption\t");
            }

            if (CurPrio == OS_TASK_IDLE_PRIO) { //如果要被切換出去的task是idle task，就print這行
                printf("task(%2d)\t\ttask(%2d)(%2d)", CurPrio, TaskParameter[HighRdyPrio/T_start-1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber);  //if the task being switched out is idle task, show this line
                fprintf(Output_fp, "task(%2d)\ttask(%2d)(%2d)", CurPrio, TaskParameter[HighRdyPrio / T_start - 1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber);
            }
            else if (HighRdyPrio == OS_TASK_IDLE_PRIO) { //如果要被切換進來的task是idle task，就print這行
                printf("task(%2d)(%2d)\t\ttask(%2d)", TaskParameter[CurPrio/T_start-1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, HighRdyPrio);  //if the task will be switched in is idle task, show this line
                fprintf(Output_fp, "task(%2d)(%2d)\ttask(%2d)", TaskParameter[CurPrio / T_start - 1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, HighRdyPrio);
            }
            else { //如果兩個task在做context switch，就print這行
                if (CurPrio == HighRdyPrio) {
                    printf("task(%2d)(%2d)\t\ttask(%2d)(%2d)", TaskParameter[CurPrio / T_start - 1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, TaskParameter[HighRdyPrio / T_start - 1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber+1);  //if the two task doing context switch, show this line
                    fprintf(Output_fp, "task(%2d)(%2d)\ttask(%2d)(%2d)", TaskParameter[CurPrio / T_start - 1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, TaskParameter[HighRdyPrio / T_start - 1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber+1);
                }else {
                    printf("task(%2d)(%2d)\t\ttask(%2d)(%2d)", TaskParameter[CurPrio / T_start - 1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, TaskParameter[HighRdyPrio / T_start - 1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber);  //if the two task doing context switch, show this line
                    fprintf(Output_fp, "task(%2d)(%2d)\ttask(%2d)(%2d)", TaskParameter[CurPrio / T_start - 1].TaskID, TaskParameter[CurPrio / T_start - 1].TaskNumber, TaskParameter[HighRdyPrio / T_start - 1].TaskID, TaskParameter[HighRdyPrio / T_start - 1].TaskNumber);
                }
            }
			//completion time
            if (TaskParameter[CurPrio / T_start - 1].Task_need_ExecutionTime == 0 && CurPrio != OS_TASK_IDLE_PRIO) { //done
                int response_time   = OSTime - TaskParameter[CurPrio / T_start - 1].TaskNumber * TaskParameter[CurPrio / T_start - 1].TaskPeriodic - TaskParameter[CurPrio / T_start - 1].TaskArriveTime;
				int preemption_time = response_time - TaskParameter[CurPrio / T_start - 1].TaskExecutionTime;
				int blocking_time = TaskParameter[CurPrio / T_start - 1].Blocking_T;
                
                printf("\t%d\t\t%d\t\t%d\n", response_time, blocking_time, (preemption_time- blocking_time));
                fprintf(Output_fp, "\t%d\t\t%d\t\t\t%d\n", response_time, blocking_time, (preemption_time - blocking_time));
                
                //記錄這個task做幾次
                TaskParameter[CurPrio / T_start - 1].TaskNumber++;
                TaskParameter[CurPrio / T_start - 1].Task_need_ExecutionTime = TaskParameter[CurPrio / T_start - 1].TaskExecutionTime;
				TaskParameter[CurPrio / T_start - 1].Blocking_T = 0; //reset blocking time
                //檢查Missdeadline
                for (int i = 0; i < TASK_NUMBER; i++) {
                    int response_time_i = OSTime - TaskParameter[i].TaskNumber * TaskParameter[i].TaskPeriodic - TaskParameter[i].TaskArriveTime;
                    int OSTimeDly_i = TaskParameter[i].TaskPeriodic - response_time_i;
                    if (OSTimeDly_i <= 0 && TaskParameter[CurPrio / T_start - 1].Task_need_ExecutionTime != 0) {

                        printf("%d\tMissDeadline\ttask( %d)( %d)\t\t----------------- \n", OSTime, TaskParameter[i].TaskID, TaskParameter[i].TaskNumber);
                        fprintf(Output_fp, "%d\tMissDeadline\ttask( %d)( %d)\t\t----------------- \n", OSTime, TaskParameter[i].TaskID, TaskParameter[i].TaskNumber);
                        OSRunning = OS_FALSE;
                        fclose(Output_fp);
                        exit(0);
                    }
                }


            
            }
            else {
                printf("\n");
				fprintf(Output_fp, "\n");
            }
        
        
        }
    }
	fclose(Output_fp);
    

}
#endif


/*
*********************************************************************************************************
*                                   OS_TCBInit() HOOK (APPLICATION)
*
* Description : This function is called by OSTCBInitHook(), which is called by OS_TCBInit() after setting
*               up most of the TCB.
*
* Argument(s) : ptcb    is a pointer to the TCB of the task being created.
*
* Note(s)     : (1) Interrupts may or may not be ENABLED during this call.
*********************************************************************************************************
*/

#if OS_VERSION >= 204
void  App_TCBInitHook (OS_TCB *ptcb)
{
    (void)ptcb;
}
#endif


/*
*********************************************************************************************************
*                                       TICK HOOK (APPLICATION)
*
* Description : This function is called every tick.
*
* Argument(s) : none.
*
* Note(s)     : (1) Interrupts may or may not be ENABLED during this call.
*********************************************************************************************************
*/

#if OS_TIME_TICK_HOOK_EN > 0
void  App_TimeTickHook (void)
{
#if (APP_CFG_PROBE_OS_PLUGIN_EN == DEF_ENABLED) && (OS_PROBE_HOOKS_EN > 0)
    OSProbe_TickHook();
#endif
}
#endif
#endif

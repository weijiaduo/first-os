/* 多任务切换 */

#include "bootpack.h"

struct TASKCTL *taskctl;
struct TIMER *task_timer;

struct TASK *task_init(struct MEMMAN *memman)
{
    int i;
    struct TASK *task;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
    taskctl = (struct TASKCTL *)memman_alloc(memman, sizeof(struct TASKCTL));
    for (i = 0; i < MAX_TASKS; i++)
    {
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss, AR_TSS32);
    }
    task = task_alloc();
    task->flags = 2;
    task->priority = 2;
    taskctl->runing = 1;
    taskctl->now = 0;
    taskctl->tasks[0] = task;
    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_settime(task_timer, task->priority);
    return task;
}

struct TASK *task_alloc(void)
{
    int i;
    struct TASK *task;
    for (i = 0; i < MAX_TASKS; i++)
    {
        if (taskctl->tasks0[i].flags == 0)
        {
            task = &taskctl->tasks0[i];
            task->flags = 1;               /* 标志为正在使用 */
            task->tss.eflags = 0x00000202; /* IF = 1 */
            task->tss.eax = 0;             /* 这里先置为0 */
            task->tss.ecx = 0;
            task->tss.edx = 0;
            task->tss.ebx = 0;
            task->tss.ebp = 0;
            task->tss.esi = 0;
            task->tss.edi = 0;
            task->tss.es = 0;
            task->tss.ds = 0;
            task->tss.fs = 0;
            task->tss.gs = 0;
            task->tss.ldtr = 0;
            task->tss.iomap = 0x40000000;
            return task;
        }
    }
    return 0; /* 任务已满 */
}

void task_run(struct TASK *task, int priority)
{
    if (priority > 0)
    {
        task->priority = priority;
    }
    if (task->flags != 2)
    {
        task->flags = 2;
        taskctl->tasks[taskctl->runing] = task;
        taskctl->runing++;
    }
    return;
}

void task_switch(void)
{
    struct TASK *task;

    taskctl->now++;
    if (taskctl->now == taskctl->runing)
    {
        taskctl->now = 0;
    }
    task = taskctl->tasks[taskctl->now];
    timer_settime(task_timer, task->priority);

    if (taskctl->runing >= 2)
    {
        farjmp(0, task->sel);
    }
    return;
}

/**
 * 任务休眠
 */
void task_sleep(struct TASK *task)
{
    int i;
    char ts = 0;
    if (task->flags == 2)
    {
        /* 任务task处于唤醒状态 */
        if (task == taskctl->tasks[taskctl->now])
        {
            /* 当前任务正在运行，让自己休眠 */
            ts = 1;
        }

        /* 寻找task所在的位置 */
        for (i = 0; i < taskctl->runing; i++)
        {
            if (task == taskctl->tasks[i])
            {
                break;
            }
        }

        /* 调整任务列表 */
        taskctl->runing--;
        if (i < taskctl->now)
        {
            taskctl->now--;
        }

        /* 移动任务成员 */
        for (; i < taskctl->runing; i++)
        {
            taskctl->tasks[i] = taskctl->tasks[i + 1];
        }

        /* 设置任务休眠 */
        task->flags = 1;
        if (ts != 0)
        {
            /* 休眠自己前，先切换任务 */
            if (taskctl->now >= taskctl->runing)
            {
                taskctl->now = 0;
            }
            farjmp(0, taskctl->tasks[taskctl->now]->sel);
        }
    }
    return;
}
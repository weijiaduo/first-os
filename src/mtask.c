/* 多任务切换 */

#include "bootpack.h"

struct TASKCTL *taskctl;
struct TIMER *task_timer;

struct TASK *task_init(struct MEMMAN *memman)
{
    int i;
    struct TASK *task, *idle;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
    taskctl = (struct TASKCTL *)memman_alloc(memman, sizeof(struct TASKCTL));
    for (i = 0; i < MAX_TASKS; i++)
    {
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss, AR_TSS32);
    }
    
    /* 初始化任务层级 */
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        taskctl->level[i].running = 0;
        taskctl->level[i].now = 0;
    }
    
    task = task_alloc();
    task->flags = 2;
    task->level = 0; /* 最高层级 */
    task->priority = 2;

    task_add(task);   /* 添加任务到层级 */
    task_switchsub(); /* 初始化层级任务 */

    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_settime(task_timer, task->priority);

    /* 闲置任务 */
    idle = task_alloc();
    idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
    idle->tss.eip = (int)&task_idle;
    idle->tss.es = 1 * 8;
    idle->tss.cs = 2 * 8;
    idle->tss.ss = 1 * 8;
    idle->tss.ds = 1 * 8;
    idle->tss.fs = 1 * 8;
    idle->tss.gs = 1 * 8;
    task_run(idle, MAX_TASKLEVELS - 1, 0);

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
            task->tss.ss0 = 0;
            return task;
        }
    }
    return 0; /* 任务已满 */
}

void task_run(struct TASK *task, int level, int priority)
{
    if (level < 0)
    {
        level = task->level; /* 不改变层级 */
    }
    if (priority > 0)
    {
        task->priority = priority; /* 不改变优先级 */
    }

    /* 改变活动中任务的层级 */
    if (task->flags == 2 && task->level != level)
    {
        task_remove(task); /* 从原层级中移除，flag 会变成1，从而执行下面的if语句 */
    }

    if (task->flags != 2)
    {
        /* 从休眠中唤醒 */
        task->level = level;
        task_add(task);
        
    }

    /* 下次任务切换时检查层级 */
    taskctl->lv_change = 1;
    return;
}

/**
 * 任务休眠
 */
void task_sleep(struct TASK *task)
{
    struct TASK *now_task;
    if (task->flags == 2)
    {
        now_task = task_now();

        /* 任务休眠 */
        task_remove(task);
        
        /* 如果休眠的是当前活动任务 */
        if (now_task == task)
        {
            /* 层级切换 */
            task_switchsub();
            /* 重新获取当前任务 */
            now_task = task_now();
            farjmp(0, now_task->sel);
        }
    }
    return;
}

void task_switch(void)
{
    struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    struct TASK *new_task, *now_task = tl->tasks[tl->now];
    
    tl->now++;
    if (tl->now == tl->running)
    {
        tl->now = 0;
    }

    /* 层级发生了变化 */
    if (taskctl->lv_change != 0)
    {
        task_switchsub();
        tl = &taskctl->level[taskctl->now_lv];
    }

    /* 新层级任务 */
    new_task = tl->tasks[tl->now];
    timer_settime(task_timer, new_task->priority);

    /* 切换任务 */
    if (new_task != now_task)
    {
        farjmp(0, new_task->sel);
    }
    return;
}

void task_switchsub(void)
{
    int i;

    /* 从高往低寻找活动的层级 */
    for (i = 0; i < MAX_TASKLEVELS; i++)
    {
        if (taskctl->level[i].running > 0)
        {
            break;
        }
    }
    taskctl->now_lv = i;
    taskctl->lv_change = 0;
    return;
}

struct TASK *task_now(void)
{
    struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    return tl->tasks[tl->now];
}

void task_add(struct TASK *task)
{
    struct TASKLEVEL *tl = &taskctl->level[task->level];
    tl->tasks[tl->running] = task;
    tl->running++;
    task->flags = 2;
    return;
}

void task_remove(struct TASK *task)
{
    int i;
    struct TASKLEVEL *tl = &taskctl->level[task->level];

    /* 寻找task所在位置 */
    for (i = 0; i < tl->running; i++)
    {
        if (tl->tasks[i] == task)
        {
            break;
        }
    }

    tl->running--;
    if (i < tl->now)
    {
        tl->now--; /* 需要移动位置 */
    }
    if (tl->now >= tl->running)
    {
        tl->now = 0; /* 处于异常情况 */
    }

    /* 休眠 */
    task->flags = 1;

    /* 移动 */
    for (; i < tl->running; i++)
    {
        tl->tasks[i] = tl->tasks[i + 1];
    }
}

void task_idle(void)
{
    for (;;)
    {
        io_hlt();
    }
}

#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

#define TIMER_FLAGS_ALLOC 1
#define TIMER_FLAGS_USING 2

struct TIMERCTL timerctl;

void init_pit(void)
{
    int i;
    struct TIMER *t;

    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    timerctl.next_timeout = 0xffffffff;
    timerctl.using = 0;
    for (i = 0; i < MAX_TIMER; i++)
    {
        /* 初始化为未使用 */
        timerctl.timers0[i].flags = 0;
    }

    /* 初始化时添加一个链表哨兵 */
    t = timer_alloc();
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next_timer = 0;
    timerctl.t0 = t;
    timerctl.next_timeout = t->timeout;
    timerctl.using = 1;
    return;
}

/* 分配定时器 */
struct TIMER *timer_alloc(void)
{
    int i;
    for (i = 0; i < MAX_TIMER; i++)
    {
        if (timerctl.timers0[i].flags == 0)
        {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].flags2 = 0;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

/* 释放定时器 */
void timer_free(struct TIMER *timer)
{
    timer->flags = 0;
    return;
}

/* 初始化定时器 */
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

/* 设置定时器超时时间 */
void timer_settime(struct TIMER *timer, unsigned int timeout)
{
    int i, j, e;
    struct TIMER *t, *s;

    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    e = io_load_eflags();
    io_cli();

    timerctl.using ++;

    /* 插入链头 */
    t = timerctl.t0;
    if (timer->timeout <= t->timeout)
    {
        timer->next_timer = t;
        timerctl.t0 = timer;
        timerctl.next_timeout = timer->timeout;
        io_store_eflags(e);
        return;
    }

    /* 插入链内 */
    for (;;)
    {
        s = t;
        t = t->next_timer;
        if (t == 0)
        {
            /* 链表末尾 */
            break;
        }
        if (timer->timeout <= t->timeout)
        {
            /* 按超时大小插入指定位置 */
            timer->next_timer = t;
            s->next_timer = timer;
            io_store_eflags(e);
            return;
        }
    }
    return;
}

/**
 * @brief 取消定时器
 * 
 * @param timer 定时器
 * @return int 取消结果，1表示成功，0表示失败
 */
int timer_cancel(struct TIMER *timer)
{
    struct TIMER *t;
    int e = io_load_eflags();
    io_cli(); /* 在设置过程中禁止改变定时器状态 */
    if (timer->flags == TIMER_FLAGS_USING)
    {
        /* 定时器正在使用中，需要取消掉 */
        if (timer == timerctl.t0)
        {
            /* 第一个定时器的取消处理 */
            t = timer->next_timer;
            timerctl.t0 = t;
            timerctl.next_timeout = t->timeout;
        }
        else
        {
            /* 非第一个定时器的取消处理 */
            /* 找到 timer 前一个定时器 */
            t = timerctl.t0;
            for (;t->next_timer != timer;)
            {
                t = t->next_timer;
            }
            /* 将之前 “timer的下一个” 指向 “timer的下一个” */
            t->next_timer = timer->next_timer;
        }
        timer->flags = TIMER_FLAGS_ALLOC;
        io_store_eflags(e);
        return 1; /* 取消处理成功 */
    }
    io_store_eflags(e);
    return 0; /* 不需要取消处理 */
}

/**
 * @brief 取消所有定时器
 * 
 * @param fifo 定时器的 FIFO 缓冲区
 */
void timer_cancelall(struct FIFO32 *fifo)
{
    int e, i;
    struct TIMER *t;
    e = io_load_eflags();
    io_cli(); /* 在设置过程中禁止改变定时器状态 */
    for (i = 0; i < MAX_TIMER; i++)
    {
        t = &timerctl.timers0[i];
        if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo)
        {
            timer_cancel(t);
            timer_free(t);
        }
    }
    io_store_eflags(e);
    return;
}

void inthandler20(int *esp)
{
    char ts = 0;
    struct TIMER *timer;
    /* 把IRQ-00信号接收完的信息通知给PIC */
    io_out8(PIC0_OCW2, 0x60);
    timerctl.count++;
    if (timerctl.next_timeout > timerctl.count)
    {
        /* 还未到下一个时刻 */
        return;
    }
    timer = timerctl.t0;
    for (;;)
    {
        /* timers 中的定时器都处于动作中，不需要判断flags */
        if (timer->timeout > timerctl.count)
        {
            break;
        }
        /* 超时 */
        timer->flags = TIMER_FLAGS_ALLOC;
        if (timer != task_timer)
        {
            fifo32_put(timer->fifo, timer->data);
        }
        else
        {
            /* 多任务切换定时到了 */
            ts = 1;
        }
        timer = timer->next_timer;
    }
    /* 有i个定时器超时了 */
    timerctl.t0 = timer;
    timerctl.next_timeout = timerctl.t0->timeout;
    if (ts != 0)
    {
        task_switch();
    }
    return;
}
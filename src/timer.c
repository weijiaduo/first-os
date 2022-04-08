#include "bootpack.h"

#define PIT_CTRL    0x0043
#define PIT_CNT0    0x0040

#define TIMER_FLAGS_ALLOC   1
#define TIMER_FLAGS_USING   2

struct TIMERCTL timerctl;

void init_pit(void)
{
  int i;
  io_out8(PIT_CTRL, 0x34);
  io_out8(PIT_CNT0, 0x9c);
  io_out8(PIT_CNT0, 0x2e);
  timerctl.count = 0;
  timerctl.next = 0xffffffff;
  timerctl.using = 0;
  for (i = 0; i < MAX_TIMER; i++)
  {
    /* 初始化为未使用 */
    timerctl.timers0[i].flags = 0;
  }
  return;
}

/* 分配定时器 */
struct TIMER * timer_alloc(void)
{
  int i;
  for (i = 0; i < MAX_TIMER; i++)
  {
    if (timerctl.timers0[i].flags == 0)
    {
      timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
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
  timer->timeout = timeout + timerctl.count;
  timer->flags = TIMER_FLAGS_USING;

  e = io_load_eflags();
  io_cli();
  /* 按时间插入指定的位置 */
  for (i = 0; i < timerctl.using; i++)
  {
    if (timerctl.timers[i]->timeout >= timer->timeout)
    {
      break;
    }
  }
  for (j = timerctl.using; j > i; j--)
  {
    timerctl.timers[j] = timerctl.timers[j - 1];
  }
  timerctl.using++;
  timerctl.timers[i] = timer;
  timerctl.next = timerctl.timers[0]->timeout;
  io_store_eflags(e);
  return;
}

void inthandler20(int *esp)
{
  int i, j;
  /* 把IRQ-00信号接收完的信息通知给PIC */
  io_out8(PIC0_OCW2, 0x60);
  timerctl.count++;
  if (timerctl.next > timerctl.count)
  {
    /* 还未到下一个时刻 */
    return;
  }
  timerctl.next = 0xffffffff;
  for (i = 0; i < timerctl.using; i++)
  {
    /* timers 中的定时器都处于动作中，不需要判断flags */
    if (timerctl.timers[i]->timeout > timerctl.count)
    {
      break;
    }
    /* 超时 */
    timerctl.timers[i]->flags = TIMER_FLAGS_ALLOC;
    fifo32_put(timerctl.timers[i]->fifo, timerctl.timers[i]->data);
  }
  /* 有i个定时器超时了，其他进行移位 */
  timerctl.using -= i;
  for (j = 0; j < timerctl.using; j++)
  {
    timerctl.timers[j] = timerctl.timers[i + j];
  }
  if (timerctl.using > 0)
  {
    timerctl.next = timerctl.timers[0]->timeout;
  }
  else
  {
    timerctl.next = 0xffffffff;
  }
  return;
}
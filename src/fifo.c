#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

/** 初始化FIFO缓冲区 */
void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK *task)
{
  fifo->size = size;
  fifo->buf = buf;
  fifo->free = size;
  fifo->flags = 0;
  fifo->p = 0;
  fifo->q = 0;
  fifo->task = task; /* 有数据写入时需要唤醒的任务 */
  return;
}

/** 向FIFO传送数据并保存 */
int fifo32_put(struct FIFO32 *fifo, int data)
{
  if (fifo->free == 0)
  {
    /* 没有空间，溢出了 */
    fifo->flags |= FLAGS_OVERRUN;
    return -1;
  }
  fifo->buf[fifo->p] = data;
  fifo->p++;
  if (fifo->p == fifo->size)
  {
    fifo->p = 0;
  }
  fifo->free--;

  /* 唤醒休眠任务 */
  if (fifo->task != 0)
  {
    if (fifo->task->flags != 2)
    {
      /* 任务处于休眠状态时才需要唤醒 */
      task_run(fifo->task);
    }
  }

  return 0;
}

/** 从FIFO获取数据 */
int fifo32_get(struct FIFO32 *fifo)
{
  int data;
  if (fifo->free == fifo->size)
  {
    /* 缓冲区为空 */
    return -1;
  }
  data = fifo->buf[fifo->q];
  fifo->q++;
  if (fifo->q == fifo->size)
  {
    fifo->q = 0;
  }
  fifo->free++;
  return data;
}

/** 缓冲区的状态 */
int fifo32_status(struct FIFO32 *fifo)
{
  return fifo->size - fifo->free;
}

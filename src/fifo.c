#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

/** 初始化FIFO缓冲区 */
void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf)
{
  fifo->size = size;
  fifo->buf = buf;
  fifo->free = size;
  fifo->flags = 0;
  fifo->p = 0;
  fifo->q = 0;
  return;
}

/** 向FIFO传送数据并保存 */
int fifo8_put(struct FIFO8 *fifo, unsigned char data)
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
  return 0;
}

/** 从FIFO获取数据 */
int fifo8_get(struct FIFO8 *fifo)
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
int fifo8_status(struct FIFO8 *fifo)
{
  return fifo->size - fifo->free;
}
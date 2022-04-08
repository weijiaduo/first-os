#include "bootpack.h"

#define SHEET_USE   1

/** 初始化图层管理 */
struct SHTCTL * shtctl_init(struct MEMMAN *man, unsigned char *vram, int xsize, int ysize)
{
  struct SHTCTL *ctl;
  int i;
  ctl = (struct SHTCTL *) memman_alloc_4k(man, sizeof(struct SHTCTL));
  if (ctl == 0)
  {
    goto err;
  }
  ctl->vram = vram;
  ctl->xsize = xsize;
  ctl->ysize = ysize;
  ctl->top = -1; /* 一个图层都没有 */
  for (i = 0; i < MAX_SHEETS; i++)
  {
    ctl->sheet0[i].flags = 0; /* 标记为未使用 */
  }

err:
  return ctl;
}

/** 分配一个未使用的图层 */
struct SHEET * sheet_alloc(struct SHTCTL *ctl)
{
  struct SHEET *sht;
  int i;
  for (i = 0; i < MAX_SHEETS; i++)
  {
    if (ctl->sheet0[i].flags == 0)
    {
      sht = &(ctl->sheet0[i]);
      sht->flags = SHEET_USE; /* 标记为正在使用 */
      sht->height = -1;       /* 隐藏 */
      return sht;
    }
  }
  return 0; /* 所有图层都处于正在使用状态 */
}

/** 设置图层的缓冲区大小以及颜色 */
void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv)
{
  sht->buf = buf;
  sht->bxsize = xsize;
  sht->bysize = ysize;
  sht->col_inv = col_inv;
  return;
}

/** 设置图层的层级 */
void sheet_updown(struct SHTCTL *ctl, struct SHEET *sht, int height)
{
  int h;
  int old = sht->height;

  /* 修正层级在指定的范围内 */
  if (height > ctl->top + 1)
  {
    height = ctl->top + 1;
  }
  if (height < -1)
  {
    height = -1;
  }
  sht->height = height; /* 更新层级 */

  if (old > height)
  {
    /* 下移图层 */
    if (height >= 0)
    {
      /* 移动图层 */
      /* 把中间的图层上移 */
      for (h = old; h > height; h--)
      {
        ctl->sheets[h] = ctl->sheets[h - 1];
        ctl->sheets[h]->height = h;
      }
      ctl->sheets[height] = sht;
    }
    else
    {
      /* 隐藏图层 */
      if (ctl->top > old)
      {
        /* 非顶层隐藏 */
        /* 把上面的图层下移 */
        for (h = old; h < ctl->top; h++)
        {
          ctl->sheets[h] = ctl->sheets[h + 1];
          ctl->sheets[h]->height = h;
        }
      }
      ctl->top--; /* 隐藏了一个图层 */
    }
    sheet_refresh(ctl);
  }
  else if (old < height)
  {
    /* 上移图层 */
    if (old >= 0)
    {
      /* 移动图层 */
      /* 把上面的图层下移 */
      for (h = old; h < height; h++)
      {
        ctl->sheets[h] = ctl->sheets[h + 1];
        ctl->sheets[h]->height = h;
      }
      ctl->sheets[height] = sht;
    }
    else
    {
      /* 显示图层 */
      /* 把上面的图层上移 */
      for (h = ctl->top; h >= height; h--)
      {
        ctl->sheets[h + 1] = ctl->sheets[h];
        ctl->sheets[h + 1]->height = h + 1;
      }
      ctl->sheets[height] = sht;
      ctl->top++; /* 增加显示了一个图层 */
    }
    sheet_refresh(ctl);
  }
  return;
}

/** 刷新所有图层 */
void sheet_refresh(struct SHTCTL *ctl)
{
  int h;
  int bx;
  int by;
  int vx;
  int vy;
  unsigned char *buf;
  unsigned char c;
  unsigned char *vram = ctl->vram;

  struct SHEET *sht;
  for (h = 0; h <= ctl->top; h++)
  {
    sht = ctl->sheets[h];
    buf = sht->buf;
    for (by = 0; by < sht->bysize; by++)
    {
      vy = sht->vy0 + by;
      for (bx = 0; bx < sht->bxsize; bx++)
      {
        vx = sht->vx0 + bx;
        c = buf[by * sht->bxsize + bx];
        if (c != sht->col_inv)
        {
          vram[vy * ctl->xsize + vx] = c;
        }
      }
    }
  }
  return;
}

/** 滑动图层窗口 */
void sheet_slide(struct SHTCTL *ctl, struct SHEET *sht, int vx0, int vy0)
{
  sht->vx0 = vx0;
  sht->vy0 = vy0;
  if (sht->height >= 0)
  {
    sheet_refresh(ctl);
  }
  return;
}

/** 释放图层 */
void sheet_free(struct SHTCTL *ctl, struct SHEET *sht)
{
  if (sht->height >= 0)
  {
    /* 如果正在显示，先隐藏 */
    sheet_updown(ctl, sht, -1);
  }
  sht->flags = 0; /* 设置为未使用 */
  return;
}
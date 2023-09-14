#include "bootpack.h"

#define SHEET_USE 1

/** 初始化图层管理 */
struct SHTCTL *shtctl_init(struct MEMMAN *man, unsigned char *vram, int xsize, int ysize)
{
    struct SHTCTL *ctl;
    int i;
    ctl = (struct SHTCTL *)memman_alloc_4k(man, sizeof(struct SHTCTL));
    if (ctl == 0)
    {
        goto err;
    }
    ctl->map = (unsigned char *)memman_alloc_4k(man, xsize * ysize);
    if (ctl->map == 0)
    {
        memman_free_4k(man, (int)ctl, sizeof(struct SHTCTL));
        goto err;
    }
    ctl->vram = vram;
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->top = -1; /* 一个图层都没有 */
    for (i = 0; i < MAX_SHEETS; i++)
    {
        ctl->sheets0[i].flags = 0; /* 标记为未使用 */
        ctl->sheets0[i].ctl = ctl; /* 标记所属 */
    }

err:
    return ctl;
}

/** 分配一个未使用的图层 */
struct SHEET *sheet_alloc(struct SHTCTL *ctl)
{
    struct SHEET *sht;
    int i;
    for (i = 0; i < MAX_SHEETS; i++)
    {
        if (ctl->sheets0[i].flags == 0)
        {
            sht = &(ctl->sheets0[i]);
            sht->flags = SHEET_USE; /* 标记为正在使用 */
            sht->height = -1;       /* 隐藏 */
            sht->task = 0;          /* 不使用自动关闭功能 */
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

/** 刷新图片的缓存映射 */
void sheet_refreshmap(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0)
{
    int h, bx, by, vx, vy;
    unsigned char *buf;
    unsigned char *map = ctl->map;
    unsigned char sid;
    int sid4;
    int *p;
    int bx0, by0, bx1, by1;
    struct SHEET *sht;

    if (vx0 < 0)
    {
        vx0 = 0;
    }
    if (vy0 < 0)
    {
        vy0 = 0;
    }
    if (vx1 > ctl->xsize)
    {
        vx1 = ctl->xsize;
    }
    if (vy1 > ctl->ysize)
    {
        vy1 = ctl->ysize;
    }

    for (h = h0; h <= ctl->top; h++)
    {
        sht = ctl->sheets[h];
        sid = sht - ctl->sheets0; /* 使用图层的偏移地址作为id */
        buf = sht->buf;

        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;
        if (bx0 < 0)
        {
            bx0 = 0;
        }
        if (by0 < 0)
        {
            by0 = 0;
        }
        if (bx1 > sht->bxsize)
        {
            bx1 = sht->bxsize;
        }
        if (by1 > sht->bysize)
        {
            by1 = sht->bysize;
        }

        if (sht->col_inv == -1)
        {
            if ((sht->vx0 & 3) == 0 && (bx0 & 3) == 0 && (bx1 & 3) == 0)
            {
                /* 无透明色图层专用的高速版（4字节型） */
                bx1 = (bx1 - bx0) / 4; /* MOV 次数*/
                sid4 = sid | sid << 8 | sid << 16 | sid << 24;
                for (by = by0; by < by1; by++)
                {
                    /* char->int，改成一次性写入4字节 */
                    vy = sht->vy0 + by;
                    vx = sht->vx0 + bx0;
                    p = (int *) &map[vy * ctl->xsize + vx];
                    for (bx = 0; bx < bx1; bx++)
                    {
                        p[bx] = sid4;
                    }
                }
            }
            else
            {
                /* 无透明色图层专用的高速版（1字节型） */
                for (by = by0; by < by1; by++)
                {
                    vy = sht->vy0 + by;
                    for (bx = bx0; bx < bx1; bx++)
                    {
                        /* 去掉了 if 判断 */
                        vx = sht->vx0 + bx;
                        map[vy * ctl->xsize + vx] = sid;
                    }
                }
            }
        }
        else
        {
            /* 有透明色图层用的普通版 */
            for (by = by0; by < by1; by++)
            {
                vy = sht->vy0 + by;
                for (bx = bx0; bx < bx1; bx++)
                {
                    vx = sht->vx0 + bx;
                    if (buf[by * sht->bxsize + bx] != sht->col_inv)
                    {
                        map[vy * ctl->xsize + vx] = sid;
                    }
                }
            }
        }
    }
    return;
}

/** 刷新指定区域 */
void sheet_refreshsub(struct SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1)
{
    int h, bx, by, vx, vy;
    unsigned char *buf;
    unsigned char *vram = ctl->vram, *map = ctl->map;
    unsigned char sid;
    int sid4, i, i1;
    int *p, *q, *r;
    int bx0, by0, bx1, by1, bx2;
    struct SHEET *sht;

    if (vx0 < 0)
    {
        vx0 = 0;
    }
    if (vy0 < 0)
    {
        vy0 = 0;
    }
    if (vx1 > ctl->xsize)
    {
        vx1 = ctl->xsize;
    }
    if (vy1 > ctl->ysize)
    {
        vy1 = ctl->ysize;
    }

    for (h = h0; h <= h1; h++)
    {
        sht = ctl->sheets[h];
        sid = sht - ctl->sheets0;
        buf = sht->buf;

        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;
        if (bx0 < 0)
        {
            bx0 = 0;
        }
        if (by0 < 0)
        {
            by0 = 0;
        }
        if (bx1 > sht->bxsize)
        {
            bx1 = sht->bxsize;
        }
        if (by1 > sht->bysize)
        {
            by1 = sht->bysize;
        }

        if ((sht->vx0 & 3) == 0)
        {
            /* 4字节型 */
            i  = (bx0 + 3) / 4; /* bx0除以4（小数进位） */
			i1 =  bx1      / 4; /* bx1除以4（小数舍去） */
			i1 = i1 - i;
			sid4 = sid | sid << 8 | sid << 16 | sid << 24;
			for (by = by0; by < by1; by++) {
				vy = sht->vy0 + by;
                /* 前面被4除多余的部分逐个字节写入 */
				for (bx = bx0; bx < bx1 && (bx & 3) != 0; bx++)
                {
					vx = sht->vx0 + bx;
					if (map[vy * ctl->xsize + vx] == sid) {
						vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
					}
				}
				vx = sht->vx0 + bx;
				p = (int *) &map[vy * ctl->xsize + vx];
				q = (int *) &vram[vy * ctl->xsize + vx];
				r = (int *) &buf[by * sht->bxsize + bx];
                /* 4的倍数部分 */
				for (i = 0; i < i1; i++)
                {
					if (p[i] == sid4) {
                        /* 估计大多数会是这种情况，因此速度会变快 */
						q[i] = r[i];
					} else {
						bx2 = bx + i * 4;
						vx = sht->vx0 + bx2;
						if (map[vy * ctl->xsize + vx + 0] == sid) {
							vram[vy * ctl->xsize + vx + 0] = buf[by * sht->bxsize + bx2 + 0];
						}
						if (map[vy * ctl->xsize + vx + 1] == sid) {
							vram[vy * ctl->xsize + vx + 1] = buf[by * sht->bxsize + bx2 + 1];
						}
						if (map[vy * ctl->xsize + vx + 2] == sid) {
							vram[vy * ctl->xsize + vx + 2] = buf[by * sht->bxsize + bx2 + 2];
						}
						if (map[vy * ctl->xsize + vx + 3] == sid) {
							vram[vy * ctl->xsize + vx + 3] = buf[by * sht->bxsize + bx2 + 3];
						}
					}
				}
                /* 后面被4除多余的部分逐个字节写入 */
				for (bx += i1 * 4; bx < bx1; bx++)
                {
					vx = sht->vx0 + bx;
					if (map[vy * ctl->xsize + vx] == sid) {
						vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
					}
				}
			}
        }
        else
        {
            /* 1字节型 */
            for (by = by0; by < by1; by++)
            {
                vy = sht->vy0 + by;
                for (bx = bx0; bx < bx1; bx++)
                {
                    vx = sht->vx0 + bx;
                    if (map[vy * ctl->xsize + vx] == sid)
                    {
                        vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
                    }
                }
            }
        }
    }
    return;
}

/** 设置图层的层级 */
void sheet_updown(struct SHEET *sht, int height)
{
    struct SHTCTL *ctl = sht->ctl;
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
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, sht->height + 1);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, sht->height + 1, old);
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
            sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0, old - 1);
        }
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
        sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, sht->height);
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, sht->height, sht->height);
    }
    return;
}

/** 刷新所有图层 */
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1)
{
    if (sht->height >= 0)
    {
        sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height, sht->height);
    }
    return;
}

/** 滑动图层窗口 */
void sheet_slide(struct SHEET *sht, int vx0, int vy0)
{
    struct SHTCTL *ctl = sht->ctl;
    int old_vx0 = sht->vx0;
    int old_vy0 = sht->vy0;
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if (sht->height >= 0)
    {
        /* 刷新图层的映射关系 */
        sheet_refreshmap(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);
        sheet_refreshmap(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);

        /* 刷新移动前后的位置 */
        sheet_refreshsub(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height - 1);
        sheet_refreshsub(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);
    }
    return;
}

/** 释放图层 */
void sheet_free(struct SHEET *sht)
{
    if (sht->height >= 0)
    {
        /* 如果正在显示，先隐藏 */
        sheet_updown(sht, -1);
    }
    sht->flags = 0; /* 设置为未使用 */
    return;
}
#include <stdio.h>
#include <string.h>

#include "bootpack.h"

/**
 * @brief 应用程序功能调用接口
 */
int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);

	struct TASK *task = task_now();
	/* 代码段基址 */
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;
	struct SHEET *sht;
	struct FILEINFO *finfo;
	struct FILEHANDLE *fh;
	int *reg = &eax + 1;
	/* 强行改写通过 PUSHAD 保存的值 */
	/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
	/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */
	int i;

	if (edx == 1)
	{
		/* 输出单个字符 */
		cons_putchar(cons, eax & 0xff, 1);
	}
	else if (edx == 2)
	{
		/* 输出字符串 */
		cons_putstr0(cons, (char *) ebx + ds_base);
	}
	else if (edx == 3)
	{
		/* 输出字符串 */
		cons_putstr1(cons, (char *) ebx + ds_base, ecx);
	}
	else if (edx == 4)
	{
		/* 结束应用程序 */
		return &(task->tss.esp0);
	}
	else if (edx == 5)
	{
		/* 新窗口 */
		sht = sheet_alloc(shtctl);
		sht->task = task;
		/* 标记为应用程序图层 */
		sht->flags |= 0x10;
		sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
		make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		/* 窗口显示在中央位置，加上 &~3 是为了使得x坐标大小变成4的倍数 */
		sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2);
		/* 将窗口图层高度指定为当前鼠标所在图层的高度，鼠标移到上层 */
		sheet_updown(sht, shtctl->top);
		reg[7] = (int) sht;
	}
	else if (edx == 6)
	{
		/* 显示字符 */
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		putfonts8_str(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		if ((ebx & 1) == 0)
		{
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	}
	else if (edx == 7)
	{
		/* 画方块 */
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0)
		{
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if (edx == 8)
	{
		/* 内存初始化 */
		memman_init((struct MEMMAN *) (ebx + ds_base));
		ecx &= 0xfffffff0; /* 以16字节为单位 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	}
	else if (edx == 9)
	{
		/* 内存分配 */
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 以16字节为单位进位取整 */
		reg[7] = memman_alloc((struct MEMMAN *) (ebx + ds_base), ecx);
	}
	else if (edx == 10)
	{
		/* 内存释放 */
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 以16字节为单位进位取整 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	}
	else if (edx == 11)
	{
		/* 画点 */
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		sht->buf[sht->bxsize * edi + esi] = eax;
		if ((ebx & 1) == 0)
		{
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	}
	else if (edx == 12)
	{
		/* 刷新窗口 */
		sht = (struct SHEET *) ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	}
	else if (edx == 13)
	{
		/* 画直线 */
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0)
		{
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if (edx == 14)
	{
		/* 关闭窗口 */
		sheet_free((struct SHEET *) ebx);
	}
	else if (edx == 15)
	{
		/* 按回车键关闭窗口 */
        return hrb_api_getkey(task, &eax);
	}
	else if (edx == 16)
	{
		/* 获取定时器 */
		reg[7] = (int) timer_alloc();
		((struct TIMER *) reg[7])->flags2 = 1; /* 允许自动取消 */
	}
	else if (edx == 17)
	{
		/* 设置定时器发送数据 */
		timer_init((struct TIMER *) ebx, &task->fifo, eax + 256);
	}
	else if (edx == 18)
	{
		/* 定时器时间设定 */
		timer_settime((struct TIMER *) ebx, eax);
	}
	else if (edx == 19)
	{
		/* 释放定时器 */
		timer_free((struct TIMER *) ebx);
	}
	else if (edx == 20)
	{
		/* 蜂鸣器发声 */
		if (eax == 0)
		{
			/* 蜂鸣器OFF */
			i = io_in8(0x61);
			io_out8(0x61, i & 0x0d);
		}
		else
		{
			/* 蜂鸣器ON */
			i = 1193180000 / eax;
			io_out8(0x43, 0xb6);
			io_out8(0x42, i & 0xff);
			io_out8(0x42, i >> 8);
			i = io_in8(0x61);
			io_out8(0x61, (i | 0x03) & 0x0f);
		}
	}
	else if (edx == 21)
	{
		/* 打开文件 */
		for (i = 0; i < 8; i++)
		{
			if (task->fhandle[i].buf == 0)
			{
				break;
			}
		}
		fh = &task->fhandle[i];
		reg[7] = 0;
		if (i < 8)
		{
			/* 加载文件 */
			finfo = file_search((char *) ebx + ds_base,
				(struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
			if (finfo != 0)
			{
				reg[7] = (int) fh;
				fh->buf = (char *) memman_alloc_4k(memman, finfo->size);
				fh->size = finfo->size;
				fh->pos = 0;
				file_loadfile(finfo->clustno, finfo->size, fh->buf, 
					task->fat, (char *) (ADR_DISKIMG + 0x003e00));
			}
		}
	}
	else if (edx == 22)
	{
		/* 关闭文件 */
		fh = (struct FILEHANDLE *) eax;
		memman_free_4k(memman, (int) fh->buf, fh->size);
		fh->buf = 0;
	}
	else if (edx == 23)
	{
		/* 文件定位 */
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0)
		{
			/* 绝对位置（从文件起点开始） */
			fh->pos = ebx;
		}
		else if (ecx == 1)
		{
			/* 相对位置（从当前位置开始） */
			fh->pos += ebx;
		}
		else if (ecx == 2)
		{
			/* 倒数位置（从文件末尾开始） */
			fh->pos = fh->size + ebx;
		}

		/* 修正区间范围 */
		if (fh->pos < 0)
		{
			fh->pos = 0;
		}
		if (fh->pos > fh->size)
		{
			fh->pos = fh->size;
		}
	}
	else if (edx == 24)
	{
		/* 文件大小 */
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0)
		{
			/* 文件大小 */
			reg[7] = fh->size;
		}
		else if (ecx == 1)
		{
			/* 已读大小 */
			reg[7] = fh->pos;
		}
		else if (ecx == 2)
		{
			/* 剩余大小 */
			reg[7] = fh->pos - fh->size;
		}
	}
	else if (edx == 25)
	{
		/* 文件读取 */
		fh = (struct FILEHANDLE *) eax;
		for (i = 0; i < ecx; i++)
		{
			if (fh->pos == fh->size)
			{
				break;
			}
			*((char *) ebx + ds_base + i) = fh->buf[fh->pos];
			fh->pos++;
		}
		reg[7] = i;
	}
	else if (edx == 26)
	{
		/* 获取命令行 */
		for (i = 0;;i++)
		{
			*((char *) ebx + ds_base + i) = task->cmdline[i];
			if (task->cmdline[i] == 0)
			{
				break;
			}
			if (i >= ecx)
			{
				break;
			}
		}
		reg[7] = i;
	}
	else if (edx == 27)
	{
		/* 获取语言模式 */
		reg[7] = task->langmode;
	}
	return 0;
}

/**
 * @brief 等待输入
 */
int * hrb_api_getkey(struct TASK *task, int *eax)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);
	struct CONSOLE *cons = task->cons;

    int *reg = eax + 1;
    int i;
    for (;;)
    {
        io_cli();

        /* FIFO 为空 */
        if (fifo32_status(&task->fifo) == 0)
        {
            if (*eax != 0)
            {
                /* eax=1时，休眠等待 */
                task_sleep(task);
            }
            else
            {
                /* eax=0时，返回-1 */
                io_sti();
                reg[7] = -1;
                return 0;
            }
        }

        /* FIFO 不为空 */
        i = fifo32_get(&task->fifo);
        io_sti();
        if (i <= 1)
        {
            /* 光标用定时器 */
            /* 应用程序运行时不需要显示关标，因此总是将下次显示用的值设为1 */
            timer_init(cons->timer, &task->fifo, 1); /* 下次置为1 */
            timer_settime(cons->timer, 50);
        }
        else if (i == 2)
        {
            /* 光标ON */
            cons->cur_c = COL8_FFFFFF;
        }
        else if (i == 3)
        {
            /* 光标OFF */
            cons->cur_c = -1;
        }
        else if (i == 4)
        {
            /* 只关闭命令行窗口 */
            timer_cancel(cons->timer);
            io_cli();
            fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024); /* 2024~2279 */
            cons->sht = 0;
            io_sti();
        }
        else if (i >= 256)
        {
            /* 键盘数据（通过任务A） */
            reg[7] = i - 256;
            return 0;
        }
    }
	return 0;
}

/**
 * @brief 画线
 * 
 * @param sht 图层
 * @param x0 起始坐标 (x0, y0)
 * @param y0 起始坐标 (x0, y0)
 * @param x1 终点坐标 (x1, y1)
 * @param y1 终点坐标 (x1, y1)
 * @param col 颜色
 */
void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;
	x = x0 << 10;
	y = y0 << 10;
	dx = x1 >= x0 ? x1 - x0 : x0 - x1;
	dy = y1 >= y0 ? y1 - y0 : y0 - y1;
	if (dx >= dy)
	{
		len = dx + 1;
		dx = x0 > x1 ? -1024 : 1024;
		dy = ((y0 <= y1 ? y1 - y0 + 1 : y1 - y0 - 1) << 10) / len;
	}
	else 
	{
		len = dy + 1;
		dy = y0 > y1 ? -1024 : 1024;
		dx = ((x0 <= x1 ? x1 - x0 + 1 : x1 - x0 - 1) << 10) / len;
	}

	for (i = 0; i < len; i++)
	{
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}
	return;
}

#include <stdio.h>
#include <string.h>

#include "bootpack.h"

/**
 * @brief 命令行窗口任务
 * 
 * @param sheet 命令行窗口图层
 * @param memtotal 内存总量
 */
void console_task(struct SHEET *sheet, unsigned int memtotal)
{
	/* 解压FAT文件分配表 */
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	int *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	
	/* 命令行结构体 */
	struct CONSOLE cons;
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;

	/* 命令行窗口任务 */
	struct TASK *task = task_now();
	task->cons = &cons;

	/* 定时器 */
	if (cons.sht != 0)
	{
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}

	/* 显式提示符 */
	cons_putchar(&cons, '>', 1);

	/* 命令行输入 */
	char cmdline[30];
	int i;
	for(;;)
	{
		io_cli();
		if (fifo32_status(&task->fifo) == 0)
		{
			task_sleep(task);
			io_sti();
		}
		else
		{
			i = fifo32_get(&task->fifo);
			io_sti();

			/* 键盘数据（通过任务A） */
			if (256 <= i && i <= 511)
			{
				if (i == 8 + 256)
				{
					/* 退格键 */
					if (cons.cur_x > 16)
					{
						/* 用空白擦除光标后，将光标前移一位 */
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				}
				else if (i == 10 + 256)
				{
					/* 回车键 */
					/* 加上字符串结尾\0 */
					cmdline[cons.cur_x / 8 - 2] = 0;

					/* 用空格将光标擦除 */
					cons_putchar(&cons, ' ', 1);
					
					/* 将光标移到下一行 */
					cons_newline(&cons);

					/* 执行命令 */
					cons_runcmd(cmdline, &cons, fat, memtotal);

					/* 命令执行完成后，关闭后台任务 */
					if (cons.sht == 0)
					{
						cmd_exit(&cons, fat);
					}

					/* 显示提示符 */
					cons_putchar(&cons, '>', 1);
				}
				else
				{
					/* 一般字符 */
					if (cons.cur_x < 240)
					{
						/* 累积记录已输入字符 */
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}

			/* 关闭窗口 */
			if (i == 4)
			{
				cmd_exit(&cons, fat);
			}

			/* 光标数据 */
			if (cons.sht != 0)
			{
				if (i <= 3)
				{
					cons_cursor(&cons, cons.timer, i);
				}
				
				/* 刷新光标 */
				sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
			}
		}
	}
}

/**
 * @brief 命令行窗口光标
 * 
 * @param timer 定时器
 * @param data 数据
 */
void cons_cursor(struct CONSOLE *cons, struct TIMER *timer, int data)
{
	/* 当前任务，也就是命令行窗口任务 */
	struct TASK *task = task_now();
	struct SHEET *sheet = cons->sht;

	/* 定时器 */
	if (data <= 1)
	{
		if (data != 0)
		{
			timer_init(timer, &task->fifo, 0); /* 下次置0 */
			if (cons->cur_c >= 0)
			{
				cons->cur_c = COL8_FFFFFF;
			}
		}
		else
		{
			timer_init(timer, &task->fifo, 1); /* 下次置1 */
			if (cons->cur_c >= 0)
			{
				cons->cur_c = COL8_000000;
			}
		}
		timer_settime(timer, 50);
	}

	/* Tab 切换，光标 ON */
	if (data == 2)
	{
		cons->cur_c = COL8_FFFFFF;
	}

	/* Tab 切换，光标 OFF */
	if (data == 3)
	{
		boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cons->cur_x, cons->cur_y, cons->cur_x + 7, cons->cur_y + 15);
		cons->cur_c = -1;
	}

	/* 重新显示光标 */
	if (cons->cur_c >= 0)
	{
		boxfill8(sheet->buf, sheet->bxsize, cons->cur_c, cons->cur_x, cons->cur_y, cons->cur_x + 7, cons->cur_y + 15);
	}
	return;
}

/**
 * @brief 控制台窗口输出换行
 * 
 * @param cons 命令行结构体
 */
void cons_newline(struct CONSOLE *cons)
{
	struct SHEET *sheet = cons->sht;
	int x, y;

	if (cons->cur_y < 28 + 112)
	{
		/* 在范围内，下移一行 */
		cons->cur_y += 16;
	}
	else
	{
		/* 超出范围，滚动换行 */
		if (sheet != 0)
		{
			/* 整体上移一行，第一行会丢失 */
			for (y = 28; y < 28 + 112; y++)
			{
				for (x = 8; x < 8 + 240; x++)
				{
					sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
				}
			}
			/* 擦除最后一行，相当于新行 */
			for (y = 28 + 112; y < 28 + 128; y++)
			{
				for (x = 8; x < 8 + 240; x++)
				{
					sheet->buf[x + y * sheet->bxsize] = COL8_000000;
				}
			}
			/* 刷新命令行窗口图层 */
			sheet_refresh(sheet, 8, 28, 2 + 240, 28 + 128);
		}
	}
	/* 回到行头 */
	cons->cur_x = 8;
	return;
}

/**
 * @brief 输出字符到命令行窗口
 * 
 * @param cons 命令行结构体
 * @param chr 字符
 * @param move 是否要移动光标，0表示不后移，1表示后移
 */
void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	struct SHEET *sheet = cons->sht;

	char s[2];
	s[0] = chr;
	s[1] = 0;

	if (s[0] == 0x09)
	{
		/* 制表符 */
		for (;;)
		{
			if (sheet != 0)
			{
				putfonts_asc_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			}
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240)
			{
				/* 到达右端后换行 */
				cons->cur_x = 8;
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0)
			{
				/* 被32整除则break */
				break;
			}
		}
	}
	else if (s[0] == 0x0a)
	{
		/* 换行 */
		cons_newline(cons);
	}
	else if (s[0] == 0x0d)
	{
		/* 回车 */
		/* 暂时不做任何操作 */
	}
	else
	{
		/* 一般字符 */
		if (sheet != 0)
		{
			putfonts_asc_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		}
		if (move != 0)
		{
			/* move 为 0 时，光标不后移 */
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240)
			{
				/* 到达右端后换行 */
				cons_newline(cons);
			}
		}
	}
	return;
}

/**
 * @brief 显示字符串，直到遇到编码0为止
 * 
 * @param cons 命令行结构
 * @param s 字符串
 */
void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++)
	{
		cons_putchar(cons, *s, 1);
	}
	return;
}

/**
 * @brief 显示字符串，指定字符串长度
 * 
 * @param cons 命令行结构
 * @param s 字符串
 * @param l 长度
 */
void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++)
	{
		cons_putchar(cons, s[i], 1);
	}
	return;
}

/**
 * @brief 执行命令
 * 
 * @param cmdline 命令行字符串
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 * @param memtotal 内存总量
 */
void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, unsigned int memtotal)
{
	if (cons->sht != 0 && strcmp(cmdline, "mem") == 0)
	{
		/* mem 命令，打印内存 */
		cmd_mem(cons, memtotal);
	}
	else if (cons->sht != 0 && strcmp(cmdline, "cls") == 0)
	{
		/* cls 命令，清屏 */
		cmd_cls(cons);
	}
	else if (cons->sht != 0 && strcmp(cmdline, "dir") == 0)
	{
		/* dir 命令，打印目录 */
		cmd_dir(cons);
	}
	else if (cons->sht != 0 && strncmp(cmdline, "type ", 5) == 0)
	{
		/* type命令，输出文件内容 */
		cmd_type(cons, fat, cmdline);
	}
	else if (strcmp(cmdline, "exit") == 0)
	{
		cmd_exit(cons, fat);
	}
	else if (strncmp(cmdline, "start ", 6) == 0)
	{
		/* start命令，新窗口执行命令 */
		cmd_start(cons, cmdline, memtotal);
	}
	else if (strncmp(cmdline, "ncst ", 5) == 0)
	{
		/* ncst命令，当前窗口执行命令 */
		cmd_ncst(cons, cmdline, memtotal);
	}
	else if (cmdline[0] != 0)
	{
		/* 应用程序执行 */
		if (cmd_app(cons, fat, cmdline) == 0)
		{
			/* 不是命令，也不是空行 */
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

/**
 * @brief mem 命令，打印内存
 * 
 * @param cons 命令行结构体
 * @param memtotal 内存总量
 */
void cmd_mem(struct CONSOLE *cons, unsigned int memtotal)
{
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
}

/**
 * @brief cls 命令，清屏
 * 
 * @param cons 命令行结构体
 */
void cmd_cls(struct CONSOLE *cons)
{
	struct SHEET *sheet = cons->sht;

	int x, y;
	for (y = 28; y < 28 + 128; y++)
	{
		for (x = 8; x < 8 + 240; x++)
		{
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);

	/* 回到第一行 */
	cons->cur_y = 28;
	return;
}

/**
 * @brief dir 命令，打印目录
 * 
 * @param cons 命令行结构体
 */
void cmd_dir(struct CONSOLE *cons)
{
	/* 文件名的起始地址 */
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);

	char s[30];
	int i, j;
	for (i = 0; i < 224; i++)
	{
		/* 文件名第一个字节为 0x00 时，表示这一段不包含任何文件名信息 */
		if (finfo[i].name[0] == 0x00)
		{
			break;
		}
		/* 文件名第一个字节为 0xE5 时，表示文件已被删除 */
		if (finfo[i].name[0] == 0xE5)
		{
			continue;
		}
		/* 文件类型判断 */
		if ((finfo[i].type & 0x18) == 0)
		{
			sprintf(s, "filename.ext %7d\n", finfo[i].size);
			for (j = 0; j < 8; j++)
			{
				s[j] = finfo[i].name[j];
			}
			s[9] = finfo[i].ext[0];
			s[10] = finfo[i].ext[1];
			s[11] = finfo[i].ext[2];
			cons_putstr0(cons, s);
		}
	}
	cons_newline(cons);
	return;
}

/**
 * @brief type命令，输出文件内容
 * 
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 * @param cmdline 命令行字符串
 */
void cmd_type(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	char *p;

	/* 搜索指定的文件 */
	finfo = file_search(cmdline + 5, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo != 0)
	{
		/* 找到文件的情况 */
		/* 创建缓冲区 */
		p = (char *) memman_alloc_4k(memman, finfo->size);
		/* 加载文件内容到缓冲区 */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		/* 打印缓冲区内容 */
		cons_putstr1(cons, p, finfo->size);
		/* 释放缓冲区 */
		memman_free_4k(memman, (int) p, finfo->size);
	}
	else
	{
		/* 没有找到文件的情况 */
		cons_putstr0(cons, "File not found.\n");
	}

	cons_newline(cons);
	return;
}

/**
 * @brief 关闭命令行窗口
 * 
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 */
void cmd_exit(struct CONSOLE *cons, int *fat)
{
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
	struct TASK *task = task_now();

	/* 取消光标定时器 */
	if (cons->sht != 0)
	{
		timer_cancel(cons->timer);
	}
	/* 释放FAT用的空间 */
	memman_free_4k(memman, (int) fat, 4 * 2880);
	
	/* 通知任务 task_a 帮忙关闭当前任务 */
	io_cli();
	if (cons->sht != 0)
	{
		fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768); /* 768~1023 */
	}
	else
	{
		fifo32_put(fifo, task - taskctl->tasks0 + 1024); /* 1024~2023 */
	}
	io_sti();

	/* 任务休眠直到被关闭 */
	for (;;) { task_sleep(task); }
}

/**
 * @brief 新窗口打开，并执行指定的命令
 * 
 * @param cons 命令行结构体
 * @param cmdline 命令行字符串
 * @param memtotal 内存总大小
 */
void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht = open_console(shtctl, memtotal);
	struct FIFO32 *fifo = &sht->task->fifo;
	int i;

	sheet_slide(sht, 32, 4);
	sheet_updown(sht, shtctl->top);

	/* 将命令行输入的字符串逐字复制到新的命令行窗口中 */
	for (i = 6; cmdline[i] != 0; i++)
	{
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256); /* 回车键 */
	cons_newline(cons);
	return;
}

/**
 * @brief 当前窗口执行指定的命令
 * 
 * @param cons 命令行结构体
 * @param cmdline 命令行字符串
 * @param memtotal 内存总大小
 */
void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct TASK *task = open_constask(0, memtotal);
	struct FIFO32 *fifo = &task->fifo;
	int i;

	/* 将命令行输入的字符串逐字复制到新的命令行窗口中 */
	for (i = 5; cmdline[i] != 0; i++)
	{
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256); /* 回车键 */
	cons_newline(cons);
	return;
}

/**
 * @brief 运行应用程序
 * 
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 * @param cmdline 命令行字符串
 * @return int 
 */
int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	struct FILEINFO *finfo;
	struct SHTCTL *shtctl;
	struct SHEET *sht;

	char name[18];
	char *p, *q;
	int i;
	int segsize, esp, datsiz, dathrb;
	struct TASK *task = task_now();

	/* 根据命令行生成文件名 */
	for (i = 0; i < 13; i++)
	{
		if (cmdline[i] <= ' ')
		{
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /* 将文件名后面置为0 */


	finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i - 1] != '.')
	{
		/* 没有找到文件，在后面加上扩展名.hrb再次寻找 */
		name[i] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';

		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0)
	{
		/* 找到文件的情况 */
		/* 分配代码段缓冲区 */
		p = (char *) memman_alloc_4k(memman, finfo->size);
		/* 加载文件内容到缓冲区 */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		
		/* 识别 Hari 文件标记，判断是否是可执行的应用程序文件 */
		if (finfo->size >= 8 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00)
		{
			/* 应用程序请求的数据段大小 */
			segsize = *((int *) (p + 0x0000));
			/* ESP初始值&数据部分传送的目标地址 */
			esp = *((int *) (p + 0x000c));
			/* 数据部分的大小 */
			datsiz = *((int *) (p + 0x0010));
			/* 数据部分的起始地址 */
			dathrb = *((int *) (p + 0x0014));

			/* 分配数据段缓冲区 */
			q = (char *) memman_alloc_4k(memman, segsize);
			task->ds_base = (int) q;

			/* 创建一个代码段，加上 0x60 表示是应用程序段 */
			set_segmdesc(task->ldt + 0, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			/* 创建一个数据段，加上 0x60 表示是应用程序段 */
			set_segmdesc(task->ldt + 1, segsize - 1, (int) q, AR_DATA32_RW + 0x60);

			/* 复制 .hrb 的数据部分到数据段中 */
			for (i = 0; i < datsiz; i++)
			{
				q[esp + i] = p[dathrb + i];
			}

			/* 0x1b 是 HariMain 函数的地址，即程序执行入口 */
			start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));

			/* 应用程序结束后，关闭遗留的窗口 */
			shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
			for (i = 0; i < MAX_SHEETS; i++)
			{
				sht = &(shtctl->sheets0[i]);
				/* 0x11 表示窗口处于使用中，并且是属于应用程序 */
				if ((sht->flags & 0x11) == 0x11 && sht->task == task)
				{
					sheet_free(sht);
				}
			}
			/* 应用程序结束后，关闭遗留的定时器 */
			timer_cancelall(&task->fifo);

			/* 释放缓冲区 */
			memman_free_4k(memman, (int) q, segsize);
		}
		else
		{
			cons_putstr0(cons, ".hrb file format error.\n");
		}

		/* 释放缓冲区 */
		memman_free_4k(memman, (int) p, finfo->size);

		cons_newline(cons);
		return 1;
	}

	/* 没找到文件的情况 */
	return 0;
}

/**
 * @brief 应用程序功能调用接口
 */
int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	int i;
	struct TASK *task = task_now();
	/* 代码段基址 */
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;
	struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	int *reg = &eax + 1;
	/* 强行改写通过 PUSHAD 保存的值 */
	/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
	/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */

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
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
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
		for (;;)
		{
			io_cli();

			/* FIFO 为空 */
			if (fifo32_status(&task->fifo) == 0)
			{
				if (eax != 0)
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
			if (i == 4)
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

/**
 * @brief 栈异常中断处理函数
 */
int *inthandler0c(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); /* 强制结束程序 */
}

/**
 * @brief 一般保护异常中断处理函数
 */
int *inthandler0d(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); /* 强制结束程序 */
}
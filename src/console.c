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
	/* 内存分配地址 */
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	/* 文件名的起始地址 */
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);
	/* 段表地址 */
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;

	/* 解压FAT文件分配表 */
	int *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	
	/* 命令行结构体 */
	struct CONSOLE cons;
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	*((int *) 0x0fec) = (int) &cons;

	/* 命令行输入 */
	char cmdline[30];

	/* 定时器 */
	struct TIMER *timer;
	struct TASK *task = task_now();

	int i;
	int fifobuf[128];
	fifo32_init(&task->fifo, 128, fifobuf, task);

	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);

	/* 显式提示符 */
	cons_putchar(&cons, '>', 1);

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

			/* 光标数据 */
			if (i <= 3)
			{
				cons_cursor(&cons, timer, i);
			}
			
			/* 刷新光标 */
			sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
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
			putfonts_asc_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
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
		putfonts_asc_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
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
 * @brief 执行命令
 * 
 * @param cmdline 命令行字符串
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 * @param memtotal 内存总量
 */
void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, unsigned int memtotal)
{
	if (strcmp(cmdline, "mem") == 0)
	{
		/* mem 命令，打印内存 */
		cmd_mem(cons, memtotal);
	}
	else if (strcmp(cmdline, "cls") == 0)
	{
		/* cls 命令，清屏 */
		cmd_cls(cons);
	}
	else if (strcmp(cmdline, "dir") == 0)
	{
		/* dir 命令，打印目录 */
		cmd_dir(cons);
	}
	else if (strncmp(cmdline, "type ", 5) == 0)
	{
		/* type命令，输出文件内容 */
		cmd_type(cons, fat, cmdline);
	}
	else if (strcmp(cmdline, "hlt") == 0)
	{
		/* hlt 应用程序 */
		cmd_hlt(cons, fat);
	}
	else if (cmdline[0] != 0)
	{
		/* 不是命令，也不是空行 */
		putfonts_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "Bad command.", 12);

		/* 换2行空行 */
		cons_newline(cons);
		cons_newline(cons);
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
	char s[30];

	sprintf(s, "total %dMB", memtotal / (1024 * 1024));
	putfonts_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
	cons_newline(cons);
	sprintf(s, "free %dKB", memman_total(memman) / 1024);
	putfonts_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);

	/* 换2行空行 */
	cons_newline(cons);
	cons_newline(cons);
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
	struct SHEET *sheet = cons->sht;
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600); /* 文件名的起始地址 */

	char s[30];
	int x, y;
	for (x = 0; x < 224; x++)
	{
		/* 文件名第一个字节为 0x00 时，表示这一段不包含任何文件名信息 */
		if (finfo[x].name[0] == 0x00)
		{
			break;
		}
		/* 文件名第一个字节为 0xE5 时，表示文件已被删除 */
		if (finfo[x].name[0] == 0xE5)
		{
			continue;
		}
		/* 文件类型判断 */
		if ((finfo[x].type & 0x18) == 0)
		{
			sprintf(s, "filename.ext %7d", finfo[x].size);
			for (y = 0; y < 8; y++)
			{
				s[y] = finfo[x].name[y];
			}
			s[9] = finfo[x].ext[0];
			s[10] = finfo[x].ext[1];
			s[11] = finfo[x].ext[2];
			putfonts_asc_sht(sheet, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
			cons_newline(cons);
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
	struct SHEET *sheet = cons->sht;
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	char *p;
	int i;

	/* 搜索指定的文件 */
	finfo = file_search(cmdline + 5, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo != 0)
	{
		/* 找到文件的情况 */
		/* 创建缓冲区 */
		p = (char *) memman_alloc_4k(memman, finfo->size);
		/* 加载文件内容到缓冲区 */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		for (i = 0; i < finfo->size; i++)
		{
			cons_putchar(cons, p[i], 1);
		}
		/* 释放缓冲区 */
		memman_free_4k(memman, (int) p, finfo->size);
	}
	else
	{
		/* 没有找到文件的情况 */
		putfonts_asc_sht(sheet, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
		cons_newline(cons);
	}

	cons_newline(cons);
	return;
}

/**
 * @brief hlt应用程序
 * 
 * @param cons 命令行结构体
 * @param fat 解压后的FAT记录
 */
void cmd_hlt(struct CONSOLE *cons, int *fat)
{
	struct SHEET *sheet = cons->sht;
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
	struct FILEINFO *finfo;
	char *p;

	/* 搜索指定的文件 */
	finfo = file_search("HLT.HRB", (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo != 0)
	{
		/* 找到文件的情况 */
		/* 创建缓冲区 */
		p = (char *) memman_alloc_4k(memman, finfo->size);
		/* 加载文件内容到缓冲区 */
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		/* 为文件创建一个代码段 */
		set_segmdesc(gdt + 1003, finfo->size - 1, (int) p, AR_CODE32_ER);
		/* 跳到文件代码段里执行代码 */
		farcall(0, 1003 * 8);
		/* 释放缓冲区 */
		memman_free_4k(memman, (int) p, finfo->size);
	}
	else
	{
		/* 没有找到文件的情况 */
		putfonts_asc_sht(sheet, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
		cons_newline(cons);
	}

	cons_newline(cons);
	return;
}

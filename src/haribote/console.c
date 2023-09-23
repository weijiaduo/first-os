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
	int i;
	/* 命令行输入 */
	char cmdline[30];

	/* 解压FAT文件分配表 */
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	unsigned char *nihongo = (unsigned char*) *((int *) 0x0fe8);
	int *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	
	/* 文件句柄 */
	struct FILEHANDLE fhandle[8];
	for (i = 0; i < 8; i++)
	{
		fhandle[i].buf = 0; /* 未使用标记 */
	}

	/* 命令行结构体 */
	struct CONSOLE cons;
	cons.sht = sheet;
	cons.cur_x = 8;
	cons.cur_y = 28;
	cons.cur_c = -1;

	/* 命令行窗口任务 */
	struct TASK *task = task_now();
	task->cons = &cons;
	task->cmdline = cmdline;
	task->fhandle = fhandle;
	task->fat = fat;
	/* 初始化语言模式 */
	if (nihongo[4096] != 0xff)
	{
		/* 是否载入了日文字库 */
		task->langmode = 1;
	}
	else
	{
		task->langmode = 0;
	}
	task->langbyte1 = 0;

	/* 定时器 */
	if (cons.sht != 0)
	{
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}

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
	struct TASK *task = task_now();
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
	/* 全角字符，需要多偏移8像素 */
	if (task->langmode == 1 && task->langbyte1 != 0)
	{
		cons->cur_x += 8;
	}
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
				putfonts_str_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
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
			putfonts_str_sht(sheet, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
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
	else if (strncmp(cmdline, "langmode ", 9) == 0)
	{
		/* 语言模式 */
		cmd_langmode(cons, cmdline);
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
 * @brief 切换语言模式
 * 
 * @param cons 命令行结构体
 * @param cmdline 命令行字符串
 */
void cmd_langmode(struct CONSOLE *cons, char *cmdline)
{
	struct TASK *task = task_now();
	unsigned char mode = cmdline[9] - '0';
	if (mode <= 3)
	{
		task->langmode = mode;
	}
	else
	{
		cons_putstr0(cons, "mode number error.\n");
	}
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
	struct FILEINFO *finfo;

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

			/* 应用程序结束后的收尾工作 */
			close_app(task);

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
 * @brief 应用程序结束后的收尾工作
 * 
 * @param task 应用程序任务
 */
void close_app(struct TASK *task)
{
	int i;
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	
	/* 销毁窗口 */
	for (i = 0; i < MAX_SHEETS; i++)
	{
		sht = &(shtctl->sheets0[i]);
		/* 0x11 表示窗口处于使用中，并且是属于应用程序 */
		if ((sht->flags & 0x11) == 0x11 && sht->task == task)
		{
			sheet_free(sht);
		}
	}
	
	/* 取消定时器 */
	timer_cancelall(&task->fifo);
	
	/* 关闭文件 */
	for (i = 0; i < 8; i++)
	{
		if (task->fhandle[i].buf != 0)
		{
			memman_free_4k(memman, (int) task->fhandle[i].buf, task->fhandle[i].size);
			task->fhandle[i].buf = 0;
		}
	}

	/* 全角字符 */
	task->langbyte1 = 0;
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

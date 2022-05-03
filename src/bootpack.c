#include <stdio.h>
#include <string.h>

#include "bootpack.h"

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);
void make_wtitle8(unsigned char *buf, int xsize, char *title, char act);
void putfonts_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);
void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *)ADR_BOOTINFO;

	char s[40];
	struct FIFO32 fifo;
	int fifobuf[128];

	struct MOUSE_DEC mdec;
	struct TIMER *timer;

	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
	struct SHTCTL *shtctl;

	struct SHEET *sht_back;
	unsigned char *buf_back;

	struct SHEET *sht_mouse;
	unsigned char buf_mouse[256];

	struct SHEET *sht_win;
	unsigned char *buf_win;

	/* 命令行窗口 */
	struct SHEET *sht_cons;
	unsigned char *buf_cons;

	/* 没按下 Shift 键时 */
	static char keytable0[0x80] = {
		0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0x5c, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x5c, 0, 0};
	/* 按下 Shift 键时 */
	static char keytable1[0x80] = {
		0, 0, '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0, 0, '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, '_', 0, 0, 0, 0, 0, 0, 0, 0, 0, '|', 0, 0};

	/* 多任务 */
	struct TASK *task_a, *task_cons, *task_b[3];

	int mx, my, i;
	int cursor_x, cursor_c;
	int key_to = 0; /* 键盘字符输出的位置 */
	int key_shift = 0; /* 未按下shift键为0，按下左shift键为1，按下右shift键为2，按下左右shift键为3 */
	int key_leds = (binfo->leds >> 4) & 7; /* 键盘灯状态，第4位ScrollLock，第5位NumberLock，第6位CapsLock */
	int keycmd_wait = -1; /* 向键盘控制器发送数据的状态 */

	struct FIFO32 keycmd;
	int keycmd_buf[32];

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 初始化PIC */
	init_pic();

	/* 允许CPU接收来自外部设备的中断 */
	io_sti();

	/* 初始化定时器中断 */
	init_pit();

	/* 启用PIC */
	io_out8(PIC0_IMR, 0xf8); /* 11111000 启用IRQ0（定时器）、IRQ1（键盘）和IRQ2 */
	io_out8(PIC1_IMR, 0xef); /* 11101111 启用IRQ12（鼠标） */

	/* 初始化键盘输入和鼠标输入的缓冲区 */
	fifo32_init(&fifo, 128, fifobuf, 0);
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

	/* 初始化键盘控制电路 */
	init_keyboard(&fifo, 256);

	/* 初始化并激活鼠标 */
	enable_mouse(&fifo, 512, &mdec);

	/* 初始化定时器缓冲区 */
	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);

	/* 内存管理 */
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);

	/* 初始化调色板 */
	init_palette();

	/* 多任务测试 */
	task_a = task_init(memman);
	fifo.task = task_a;

	/* 初始化图层 */
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);

	/* 背景图层 */
	sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

	/* 命令行窗口 */
	sht_cons = sheet_alloc(shtctl);
	buf_cons = (unsigned char *)memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); /* 无透明色 */
	make_window8(buf_cons, 256, 165, "console", 0); /* 窗口范围，包括标题栏 */
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000); /* 输入窗口范围 */
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 12;
	task_cons->tss.eip = (int)&console_task;
	task_cons->tss.es = 1 * 8;
	task_cons->tss.cs = 2 * 8;
	task_cons->tss.ss = 1 * 8;
	task_cons->tss.ds = 1 * 8;
	task_cons->tss.fs = 1 * 8;
	task_cons->tss.gs = 1 * 8;
	*((int *)(task_cons->tss.esp + 4)) = (int)sht_cons;
	*((int *)(task_cons->tss.esp + 8)) = memtotal;
	task_run(task_cons, 2, 2); /* level=2, priority=2 */

	/* 主窗口图层 */
	sht_win = sheet_alloc(shtctl);
	buf_win = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);
	make_window8(buf_win, 144, 52, "task_a", 1);
	/* 初始化输入框 */
	make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;

	/* 鼠标图层 */
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99);
	mx = (binfo->scrnx - 16) / 2;
	my = (binfo->scrny - 28 - 16) / 2;

	/* 设置图层位置和层级 */
	sheet_slide(sht_back, 0, 0);
	sheet_slide(sht_cons, 32, 4);
	sheet_slide(sht_win, 8, 56);
	sheet_slide(sht_mouse, mx, my);
	sheet_updown(sht_back, 0);
	sheet_updown(sht_cons, 1);
	sheet_updown(sht_win, 2);
	sheet_updown(sht_mouse, 3);

	/* 为避免和键盘当前状态冲突，在一开始先进行设置 */
	fifo32_put(&keycmd, KEYCMD_LED);
	fifo32_put(&keycmd, key_leds);

	for (;;)
	{
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0)
		{
			/* 如果存在向键盘控制器发送的数据，则发送它 */
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
		io_cli();
		if (fifo32_status(&fifo) == 0)
		{
			task_sleep(task_a);
			io_sti();
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();

			if (256 <= i && i <= 511)
			{
				/* 键盘数据 */
				if (i < 0x80 + 256)
				{
					if (key_shift == 0)
					{
						s[0] = keytable0[i - 256];
					}
					else
					{
						s[0] = keytable1[i - 256];
					}
				}
				else
				{
					s[0] = 0;
				}

				/* 字母大小写处理 */
				if ('A' <= s[0] && s[0] <= 'Z')
				{
					if (((key_leds & 4) == 0 && key_shift == 0) ||
						((key_leds & 4) != 0 && key_shift != 0))
					{
						/* 大小写转换 */
						s[0] += 0x20;
					}
				}

				/* 一般字符 */
				if (s[0] != 0)
				{
					if (key_to == 0)
					{
						/* 发送给窗口A */
						if (cursor_x < 128)
						{
							/* 一般字符，显示后后移光标 */
							s[1] = 0;
							putfonts_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
							cursor_x += 8;
						}
					}
					else
					{
						/* 发送给命令行窗口 */
						fifo32_put(&task_cons->fifo, s[0] + 256);
					}
				}

				/* 退格键 */
				if (i == 256 + 0x0e)
				{
					if (key_to == 0)
					{
						/* 发送给窗口A */
						if (cursor_x > 8)
						{
							/* 用空格把光标去掉，再前移1次光标 */
							putfonts_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
							cursor_x -= 8;
						}
					}
					else
					{
						/* 发送给命令行窗口 */
						fifo32_put(&task_cons->fifo, 8 + 256);
					}
				}

				/* 回车键 */
				if (i == 256 + 0x1c)
				{
					if (key_to != 0)
					{
						/* 发送至命令行窗口 */
						fifo32_put(&task_cons->fifo, 10 + 256);
					}
				}

				/* Tab 键 */
				if (i == 256 + 0x0f)
				{
					if (key_to == 0)
					{
						key_to = 1;
						make_wtitle8(buf_win, sht_win->bxsize, "task_a", 0);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 1);
						/* 不显示光标 */
						cursor_c = -1;
						boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cursor_x, 28, cursor_x + 7, 43);
						/* 命令行窗口光标 ON */
						fifo32_put(&task_cons->fifo, 2);
					}
					else
					{
						key_to = 0;
						make_wtitle8(buf_win, sht_win->bxsize, "task_a", 1);
						make_wtitle8(buf_cons, sht_cons->bxsize, "console", 0);
						/* 显示光标 */
						cursor_c = COL8_000000;
						/* 命令行窗口光标 OFF */
						fifo32_put(&task_cons->fifo, 3);
					}
					sheet_refresh(sht_win, 0, 0, sht_win->bxsize, 21);
					sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
				}

				/* 左 Shift ON */
				if (i == 256 + 0x2a)
				{
					key_shift |= 1;
				}
				/* 右 Shift ON */
				if (i == 256 + 0x36)
				{
					key_shift |= 2;
				}
				/* 左 Shift OFF */
				if (i == 256 + 0xaa)
				{
					key_shift &= ~1;
				}
				/* 右 Shift OFF */
				if (i == 256 + 0xb6)
				{
					key_shift &= ~2;
				}

				/* CapsLock */
				if (i == 256 + 0x3a)
				{
					key_leds ^= 4;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				/* NumberLock */
				if (i == 256 + 0x45)
				{
					key_leds ^= 2;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}
				/* ScrollLock */
				if (i == 256 + 0x46)
				{
					key_leds ^= 1;
					fifo32_put(&keycmd, KEYCMD_LED);
					fifo32_put(&keycmd, key_leds);
				}

				/* 键盘成功接收到数据 */
				if (i == 256 + 0xfa)
				{
					keycmd_wait = -1;
				}
				/* 键盘没有成功接收到数据 */
				if (i == 256 + 0xfe)
				{
					/* 重新发送数据 */
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
				}

				/* 显示光标 */
				if (cursor_c >= 0)
				{
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				}
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
			else if (512 <= i && i <= 767)
			{
				/* 鼠标数据 */
				if (mouse_decode(&mdec, i - 512) != 0)
				{
					/* 鼠标指针的移动 */
					mx += mdec.x;
					my += mdec.y;
					if (mx < 0)
					{
						mx = 0;
					}
					if (my < 0)
					{
						my = 0;
					}
					if (mx > binfo->scrnx - 1)
					{
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1)
					{
						my = binfo->scrny - 1;
					}

					/* 移动鼠标位置 */
					sheet_slide(sht_mouse, mx, my);
					if ((mdec.btn & 0x01) != 0)
					{
						/* 按下左键，移动win窗口 */
						sheet_slide(sht_win, mx - 80, my - 8);
					}
				}
			}
			else if (i == 1)
			{
				/* 光标定时器（白） */
				timer_init(timer, &fifo, 0);
				timer_settime(timer, 50);
				if (cursor_c >= 0)
				{
					cursor_c = COL8_000000;
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
					sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}
			}
			else if (i == 0)
			{
				/* 光标定时器（黑） */
				timer_init(timer, &fifo, 1);
				timer_settime(timer, 50);
				if (cursor_c >= 0)
				{
					cursor_c = COL8_FFFFFF;
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
					sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}
			}
		}
	}
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act)
{
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         xsize - 1, 0        );
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         xsize - 2, 1        );
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         0,         ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         1,         ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1,         xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0,         xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2,         2,         xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_848484, 1,         ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0,         ysize - 1, xsize - 1, ysize - 1);

	make_wtitle8(buf, xsize, title, act);
	return;
}

void make_wtitle8(unsigned char *buf, int xsize, char *title, char act)
{
	static char closebtn[14][16] = {
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@"
	};

	int x, y;
	char c;
	char tc, tbc;
	if (act != 0)
	{
		tc = COL8_FFFFFF;
		tbc = COL8_000084;
	}
	else
	{
		tc = COL8_C6C6C6;
		tbc = COL8_848484;
	}

	boxfill8(buf, xsize, tbc, 3, 3, xsize - 4, 20);
	putfonts8_asc(buf, xsize, 24, 4, tc, title);
	for (y = 0; y < 14; y++)
	{
		for (x = 0; x < 16; x++)
		{
			c = closebtn[y][x];
			if (c == '@')
			{
				c = COL8_000000;
			}
			else if (c == '$')
			{
				c = COL8_848484;
			}
			else if (c == 'Q')
			{
				c = COL8_C6C6C6;
			}
			else
			{
				c = COL8_FFFFFF;
			}
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}
}

void putfonts_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l)
{
	// 背景更新
	boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15);
	// 文字更新
	putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s);
	// 图层刷新
	sheet_refresh(sht, x, y, x + l * 8, y + 16);
}

void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c)
{
	int x1 = x0 + sx;
	int y1 = y0 + sy;
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, c,           x0 - 1, y0 - 1, x1 + 0, y1 + 0);
	return;
}

void task_b_main(struct SHEET *sht_win_b)
{
	struct FIFO32 fifo;
	struct TIMER *timer_1s;
	int fifobuf[128];
	int i, count = 0, count0 = 0;
	char s[12];

	fifo32_init(&fifo, 128, fifobuf, 0);

	timer_1s = timer_alloc();
	timer_init(timer_1s, &fifo, 100);
	timer_settime(timer_1s, 100);

	for (;;)
	{
		count++;
		io_cli();
		if (fifo32_status(&fifo) == 0)
		{
			io_sti();
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();
			if (i == 100)
			{
				sprintf(s, "%11d", count - count0);
				putfonts_asc_sht(sht_win_b, 24, 28, COL8_000000, COL8_C6C6C6, s, 11);
				count0 = count;
				timer_settime(timer_1s, 100);
			}
		}
	}
}

/* 命令行窗口任务 */
void console_task(struct SHEET *sheet, unsigned int memtotal)
{
	struct TIMER *timer;
	struct TASK *task = task_now();

	int i;
	int x, y;
	int fifobuf[128];
	int cursor_x = 16, cursor_y = 28, cursor_c = -1;
	char s[30], cmdline[30];
	char *p;
	struct MENMAN *menman = (struct MENMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600); /* 文件名的起始地址 */

	fifo32_init(&task->fifo, 128, fifobuf, task);

	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);

	/* 显式提示符 */
	putfonts_asc_sht(sheet, 8, 28, COL8_FFFFFF, COL8_000000, ">", 1);

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

			/* 光标定时器 */
			if (i <= 1)
			{
				if (i != 0)
				{
					timer_init(timer, &task->fifo, 0); /* 下次置0 */
					if (cursor_c >= 0)
					{
						cursor_c = COL8_FFFFFF;
					}
				}
				else
				{
					timer_init(timer, &task->fifo, 1); /* 下次置1 */
					if (cursor_c >= 0)
					{
						cursor_c = COL8_000000;
					}
				}
				timer_settime(timer, 50);
			}

			/* 光标 ON */
			if (i == 2)
			{
				cursor_c = COL8_FFFFFF;
			}
			/* 光标 OFF */
			if (i == 3)
			{
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
				cursor_c = -1;
			}

			/* 键盘数据（通过任务A） */
			if (256 <= i && i <= 511)
			{
				if (i == 8 + 256)
				{
					/* 退格键 */
					if (cursor_x > 16)
					{
						/* 用空白擦除光标后，将光标前移一位 */
						putfonts_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
						cursor_x -= 8;
					}
				}
				else if (i == 10 + 256)
				{
					/* 回车键 */
					/* 用空格将光标擦除 */
					putfonts_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
					
					/* 加上字符串结尾\0 */
					cmdline[cursor_x / 8 - 2] = 0;
					/* 将光标移到下一行 */
					cursor_y = cons_newline(cursor_y, sheet);

					/* 执行命令 */
					if (strcmp(cmdline, "mem") == 0)
					{
						/* mem 命令，打印内存 */
						sprintf(s, "total %dMB", memtotal / (1024 * 1024));
						putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						sprintf(s, "free %dKB", memman_total(menman) / 1024);
						putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);

						/* 换2行空行 */
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strcmp(cmdline, "cls") == 0)
					{
						/* cls 命令，清屏 */
						for (y = 28; y < 28 + 128; y++)
						{
							for (x = 8; x < 8 + 240; x++)
							{
								sheet->buf[x + y * sheet->bxsize] = COL8_000000;
							}
						}
						sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
						/* 回到第一行 */
						cursor_y = 28;
					}
					else if (strcmp(cmdline, "dir") == 0)
					{
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
								putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
								cursor_y = cons_newline(cursor_y, sheet);
							}
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strncmp(cmdline, "type ", 5) == 0)
					{
						/* type命令，输出文件内容 */
						/* 解析出文件名 */
						for (y = 0; y < 11; y++)
						{
							s[y] = ' ';
						}
						y = 0;
						for (x = 5; y < 11 && cmdline[x] != 0; x++)
						{
							if (cmdline[x] == '.' && y <= 8)
							{
								y = 8;
							}
							else
							{
								s[y] = cmdline[x];
								if ('a' <= s[y] && s[y] <= 'z')
								{
									/* 将小写字母转换成大写字母 */
									s[y] -= 0x20;
								}
								y++;
							}
						}
						/* 找到文件 */
						for (x = 0; x < 224;)
						{
							/* 文件名第一个字节为 0x00 时，表示这一段不包含任何文件名信息 */
							if (finfo[x].name[0] == 0x00)
							{
								break;
							}
							/* 文件类型判断 */
							if ((finfo[x].type & 0x18) == 0)
							{
								for (y = 0; y < 11; y++)
								{
									if (finfo[x].name[y] != s[y])
									{
										goto type_next_file;
									}
								}
								/* 找到文件名 */
								break;
							}
type_next_file:
							x++;
						}
						if (x < 24 && finfo[x].name[0] != 0x00)
						{
							/* 找到文件的情况 */
							y = finfo[x].size;
							p = (char *) (finfo[x].clustno * 512 + 0x003e00 + ADR_DISKIMG);
							cursor_x = 8;
							for (x = 0; x < y; x++)
							{
								/* 逐字输出 */
								s[0] = p[x];
								s[1] = 0;
								if (s[0] == 0x09)
								{
									/* 制表符 */
									for (;;)
									{
										putfonts_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
										cursor_x += 8;
										if (cursor_x == 8 + 240)
										{
											/* 到达右端后换行 */
											cursor_x = 8;
											cursor_y = cons_newline(cursor_y, sheet);
										}
										if (((cursor_x - 8) & 0x1f) == 0)
										{
											/* 被32整除则break */
											break;
										}
									}
								}
								else if (s[0] == 0x0a)
								{
									/* 换行 */
									cursor_x = 8;
									cursor_y = cons_newline(cursor_y, sheet);
								}
								else if (s[0] == 0x0d)
								{
									/* 回车 */
									/* 暂时不做任何操作 */
								}
								else
								{
									/* 一般字符 */
									putfonts_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
									cursor_x += 8;
									if (cursor_x == 8 + 240)
									{
										/* 到达右端后换行 */
										cursor_x = 8;
										cursor_y = cons_newline(cursor_y, sheet);
									}
								}
							}
						}
						else
						{
							/* 没有找到文件的情况 */
							putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else
					{
						/* 不是命令，也不是空行 */
						putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "Bad command.", 12);

						/* 换2行空行 */
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}

					/* 显示提示符 */
					putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, ">", 1);
					/* 提示符宽度 */
					cursor_x = 16;
				}
				else
				{
					/* 一般字符 */
					if (cursor_x < 240)
					{
						/* 显示一个字符后，将光标后移一位 */
						s[0] = i - 256;
						s[1] = 0;
						/* 累积记录已输入字符 */
						cmdline[cursor_x / 8 - 2] = i - 256;
						putfonts_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
						cursor_x += 8;
					}
				}
			}

			/* 重新显示光标 */
			if (cursor_c >= 0)
			{
				boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
			}
			sheet_refresh(sheet, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);
		}
	}
}

int cons_newline(int cursor_y, struct SHEET *sheet)
{
	int x, y;
	if (cursor_y < 28 + 112)
	{
		/* 在范围内，下移一行 */
		cursor_y += 16;
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
	return cursor_y;
}

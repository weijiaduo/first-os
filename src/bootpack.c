#include <stdio.h>

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

	static char keytable[0x54] = {
		0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.'};

	/* 多任务 */
	struct TASK *task_a, *task_cons, *task_b[3];

	int mx, my, i;
	int cursor_x, cursor_c;
	int key_to = 0; /* 键盘字符输出的位置 */

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
	make_window8(buf_cons, 256, 165, "console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
	task_cons->tss.eip = (int)&console_task;
	task_cons->tss.es = 1 * 8;
	task_cons->tss.cs = 2 * 8;
	task_cons->tss.ss = 1 * 8;
	task_cons->tss.ds = 1 * 8;
	task_cons->tss.fs = 1 * 8;
	task_cons->tss.gs = 1 * 8;
	*((int *)(task_cons->tss.esp + 4)) = (int)sht_cons;
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

	/* 打印鼠标位置 */
	sprintf(s, "(%3d, %3d)", mx, my);
	putfonts_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);

	/* 内存检查 */
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024);
	putfonts_asc_sht(sht_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

	for (;;)
	{
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
				sprintf(s, "%02X", i - 256);
				putfonts_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				if (i < 256 + 0x54)
				{
					if (keytable[i - 256] != 0 && cursor_x < 144)
					{
						/* 一般字符，显示后后移光标 */
						s[0] = keytable[i - 256];
						s[1] = 0;
						putfonts_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_C6C6C6, s, 1);
						cursor_x += 8;
					}
					if (i == 256 + 0x0e && cursor_x > 8)
					{
						/* 退格键 */
						/* 用空格把光标去掉，再前移1次光标 */
						putfonts_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
						cursor_x -= 8;
					}
					if (i == 256 + 0x0f)
					{
						/* Tab 键 */
						if (key_to == 0)
						{
							key_to = 1;
							make_wtitle8(buf_win, sht_win->bxsize, "task_a", 0);
							make_wtitle8(buf_cons, sht_cons->bxsize, "console", 1);
						}
						else
						{
							key_to = 0;
							make_wtitle8(buf_win, sht_win->bxsize, "task_a", 1);
							make_wtitle8(buf_cons, sht_cons->bxsize, "console", 0);
						}
						sheet_refresh(sht_win, 0, 0, sht_win->bxsize, 21);
						sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
					}
					/* 光标显示 */
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
					sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}
			}
			else if (512 <= i && i <= 767)
			{
				/* 鼠标数据 */
				if (mouse_decode(&mdec, i - 512) != 0)
				{
					/* 鼠标的3个字节都齐了，显示出来 */
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0)
					{
						s[1] = 'L';
					}
					if ((mdec.btn & 0x02) != 0)
					{
						s[3] = 'R';
					}
					if ((mdec.btn & 0x04) != 0)
					{
						s[2] = 'C';
					}
					putfonts_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);

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
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);

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
				cursor_c = COL8_000000;
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}
			else if (i == 0)
			{
				/* 光标定时器（黑） */
				timer_init(timer, &fifo, 1);
				timer_settime(timer, 50);
				cursor_c = COL8_FFFFFF;
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
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
void console_task(struct SHEET *sheet)
{
	struct FIFO32 fifo;
	struct TIMER *timer;
	struct TASK *task = task_now();

	int i;
	int fifobuf[128];
	int cursor_x = 8;
	int cursor_c = COL8_000000;
	fifo32_init(&fifo, 128, fifobuf, task);

	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 50);

	for(;;)
	{
		io_cli();
		if (fifo32_status(&fifo) == 0)
		{
			task_sleep(task);
			io_sti();
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();
			if (i <= 1) /* 光标定时器 */
			{
				if (i != 0)
				{
					timer_init(timer, &fifo, 0); /* 下次置0 */
					cursor_c = COL8_FFFFFF;
				}
				else
				{
					timer_init(timer, &fifo, 1); /* 下次置1 */
					cursor_c = COL8_000000;
				}
				timer_settime(timer, 50);
				boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sheet, cursor_x, 28, cursor_x + 8, 44);
			}
		}
	}
}

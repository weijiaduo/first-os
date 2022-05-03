#include <stdio.h>
#include <string.h>

#include "bootpack.h"

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
	struct TASK *task_a, *task_cons;

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

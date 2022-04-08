#include <stdio.h>

#include "bootpack.h"

void make_window8(unsigned char *buf, int xsize, int ysize, char *title);
void putfonts_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	
	char s[40];
	struct FIFO32 fifo;
	int fifobuf[128];

	struct MOUSE_DEC mdec;
	struct TIMER *timer, *timer2, *timer3;

	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl;

	struct SHEET *sht_back;
	unsigned char *buf_back;

	struct SHEET *sht_mouse;
	unsigned char buf_mouse[256];

	struct SHEET *sht_win;
	unsigned char *buf_win;

	int mx, my, i, count;

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
	fifo32_init(&fifo, 128, fifobuf);

	/* 初始化键盘控制电路 */
	init_keyboard(&fifo, 256);

	/* 初始化并激活鼠标 */
	enable_mouse(&fifo, 512, &mdec);

	/* 初始化定时器缓冲区 */
	timer = timer_alloc();
	timer_init(timer, &fifo, 10);
	timer_settime(timer, 1000);

	timer2 = timer_alloc();
	timer_init(timer2, &fifo, 3);
	timer_settime(timer2, 300);

	timer3 = timer_alloc();
	timer_init(timer3, &fifo, 1);
	timer_settime(timer3, 50);

	/* 内存管理 */
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);

	/* 初始化调色板 */
  init_palette();

	/* 初始化图层 */
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	sht_back = sheet_alloc(shtctl);
	sht_mouse = sheet_alloc(shtctl);
	sht_win = sheet_alloc(shtctl);
	buf_back = (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	buf_win = (unsigned char *) memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	sheet_setbuf(sht_win, buf_win, 160, 52, -1);
  
	/* 初始化屏幕 */
  init_screen8(buf_back, binfo->scrnx, binfo->scrny);
	sheet_slide(sht_back, 0, 0);

	/* 初始化鼠标 */
	init_mouse_cursor8(buf_mouse, 99);
	mx = (binfo->scrnx - 16) / 2;
	my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(sht_mouse, mx, my);

	/* 初始化窗口 */
	make_window8(buf_win, 160, 52, "counter");
	sheet_slide(sht_win, 80, 72);

	/* 设置背景图层和鼠标图层 */
	sheet_updown(sht_back,  0);
	sheet_updown(sht_win, 1);
	sheet_updown(sht_mouse, 2);

	/* 打印字符串变量值 */
	sprintf(s, "(%d, %d)", mx, my);
	putfonts_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);

	/* 内存检查 */
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024);
	putfonts_asc_sht(sht_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

  for (;;)
  {
		/* 计数 */
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
			if (256 <= i && i <= 511)
			{
				/* 键盘数据 */
				sprintf(s, "%02X", i - 256);
				putfonts_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
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
					if (mx < 0) { mx = 0; }
					if (my < 0) { my = 0; }
					if (mx > binfo->scrnx - 1) { mx = binfo->scrnx - 1; }
					if (my > binfo->scrny - 1) { my = binfo->scrny - 1; }
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
					
					/* 移动鼠标位置 */
					sheet_slide(sht_mouse, mx, my);
				}
			}
			else if (i == 10)
			{
				/* 10秒定时器 */
				putfonts_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);
				/* 打印计数 */
				sprintf(s, "%010d", count);
				putfonts_asc_sht(sht_win, 40, 28, COL8_000000, COL8_C6C6C6, s, 10);
			}
			else if (i == 3)
			{
				/* 3秒定时器 */
				putfonts_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[sec]", 6);
				count = 0; /* 开始计数 */
			}
			else if (i == 1)
			{
				/* 光标定时器 */
				timer_init(timer3, &fifo, 0);
				boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 8, 96, 15, 111);
				timer_settime(timer3, 50);
				sheet_refresh(sht_back, 8, 96, 16, 112);
			}
			else if (i == 0)
			{
				/* 光标定时器 */
				timer_init(timer3, &fifo, 1);
				boxfill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111);
				timer_settime(timer3, 50);
				sheet_refresh(sht_back, 8, 96, 16, 112);
			}
		}
  }
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title)
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
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         xsize - 1, 0        );
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         xsize - 2, 1        );
	boxfill8(buf, xsize, COL8_C6C6C6, 0,         0,         0,         ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1,         1,         1,         ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1,         xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0,         xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2,         2,         xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3,         3,         xsize - 4, 20       );
	boxfill8(buf, xsize, COL8_848484, 1,         ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0,         ysize - 1, xsize - 1, ysize - 1);

	putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);
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

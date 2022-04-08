#include <stdio.h>

#include "bootpack.h"

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	char keybuf[32];
	char mousebuf[128];
	struct MOUSE_DEC mdec;
	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHTCTL *shtctl;
	struct SHEET *sht_back;
	struct SHEET *sht_mouse;
	unsigned char *buf_back;
	unsigned char buf_mouse[256];

	int mx;
	int my;
	int i;

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 初始化PIC */
	init_pic();

	/* 允许CPU接收来自外部设备的中断 */
	io_sti();

	/* 初始化键盘输入和鼠标输入的缓冲区 */
	fifo8_init(&keyfifo, 32, keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);

	/* 启用PIC */
	io_out8(PIC0_IMR, 0xf9); /* 11111001 启用IRQ1（键盘）和IRQ2 */
	io_out8(PIC1_IMR, 0xef); /* 11101111 启用IRQ12（鼠标） */

	/* 初始化键盘控制电路 */
	init_keyboard();

	/* 激活鼠标 */
	enable_mouse(&mdec);

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
	buf_back = (unsigned char *) memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
  
	/* 初始化屏幕 */
  init_screen8(buf_back, binfo->scrnx, binfo->scrny);
	sheet_slide(shtctl, sht_back, 0, 0);

	/* 初始化鼠标 */
	init_mouse_cursor8(buf_mouse, 99);
	mx = (binfo->scrnx - 16) / 2;
	my = (binfo->scrny - 28 - 16) / 2;
	sheet_slide(shtctl, sht_mouse, mx, my);

	/* 设置背景图层和鼠标图层 */
	sheet_updown(shtctl, sht_back,  0);
	sheet_updown(shtctl, sht_mouse, 1);

	/* 打印字符串变量值 */
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	/* 内存检查 */
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024);
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);

	/* 刷新所有图层 */
	sheet_refresh(shtctl);

  for (;;)
  {
		io_cli();
		if (fifo8_status(&keyfifo) != 0)
		{
			/* 键盘数据 */
			i = fifo8_get(&keyfifo);
			io_sti();
			sprintf(s, "%02X", i);
			boxfill8(buf_back, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
			putfonts8_asc(buf_back, binfo->scrnx, 0, 16, COL8_FFFFFF, s);

			sheet_refresh(shtctl);
		}
		else if (fifo8_status(&mousefifo) != 0)
		{
			/* 鼠标数据 */
			i = fifo8_get(&mousefifo);
			io_sti();

			if (mouse_decode(&mdec, i) != 0)
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
				boxfill8(buf_back, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
				putfonts8_asc(buf_back, binfo->scrnx, 32, 16, COL8_FFFFFF, s);

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
				if (mx > binfo->scrnx - 16)
				{
					mx = binfo->scrnx - 16;
				}
				if (my > binfo->scrny - 16)
				{
					my = binfo->scrny - 16;
				}
				sprintf(s, "(%3d, %3d)", mx, my);
				boxfill8(buf_back, binfo->scrnx, COL8_008484, 0, 0, 79, 15); /* 隐藏坐标 */
				putfonts8_asc(buf_back, binfo->scrnx, 0 , 0, COL8_FFFFFF, s); /* 显示坐标 */
				
				/* 移动鼠标位置 */
				sheet_slide(shtctl, sht_mouse, mx, my);
			}
		}
		else
		{
			io_stihlt();
		}
  }
}

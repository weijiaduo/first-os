#include "bootpack.h"
#include <stdio.h>

extern struct KEYBUF keybuf;

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	char mcursor[256];
	int mx;
	int my;
	int i;

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 初始化PIC */
	init_pic();

	/* 允许CPU接收来自外部设备的中断 */
	io_sti();

	/* 初始化调色板 */
  init_palette();
  
	/* 初始化屏幕 */
  init_screen8(binfo->vram, binfo->scrnx, binfo->scrny);

	/* 初始化鼠标 */
	mx = (binfo->scrnx - 16) / 2;
	my = (binfo->scrny - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);

	/* 打印字符串变量值 */
	sprintf(s, "(%d, %d)", mx, my);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	/* 启用PIC */
	io_out8(PIC0_IMR, 0xf9); /* 11111001 启用IRQ1（键盘）和IRQ2 */
	io_out8(PIC1_IMR, 0xef); /* 11101111 启用IRQ12（鼠标） */

  for (;;)
  {
		io_cli();
		if (keybuf.flag == 0)
		{
			io_stihlt();
		}
		else
		{
			i = keybuf.data;
			keybuf.flag = 0;
			io_sti();
			sprintf(s, "%02X", i);
			boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
			putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
		}
    io_hlt();
  }
}

#include "bootpack.h"
#include <stdio.h>

extern struct FIFO8 keyfifo;

void enable_mouse(void);
void init_keyboard(void);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	char mcursor[256];
	char keybuf[32];
	int mx;
	int my;
	int i;
	int j;

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 初始化PIC */
	init_pic();

	/* 允许CPU接收来自外部设备的中断 */
	io_sti();

	/* 初始化键盘输入缓冲区 */
	fifo8_init(&keyfifo, 32, keybuf);

	/* 启用PIC */
	io_out8(PIC0_IMR, 0xf9); /* 11111001 启用IRQ1（键盘）和IRQ2 */
	io_out8(PIC1_IMR, 0xef); /* 11101111 启用IRQ12（鼠标） */

	/* 初始化键盘控制电路 */
	init_keyboard();

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

	/* 激活鼠标 */
	enable_mouse();

  for (;;)
  {
		io_cli();
		if (fifo8_status(&keyfifo) == 0)
		{
			io_stihlt();
		}
		else
		{
			i = fifo8_get(&keyfifo);
			io_sti();
			sprintf(s, "%02X", i);
			boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
			putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
		}
    io_hlt();
  }
}

#define PORT_KEYDAT				0x0060
#define PORT_KEYSTA				0x0064
#define PORT_KEYCMD				0x0064
#define KEYSTA_SEND_NOTREADY	0x02
#define KEYCMD_WRITE_MODE		0x60
#define KBC_MODE				0x47

/** 等待键盘控制电路准备完毕 */
void wait_KBC_sendready(void)
{
	for (;;)
	{
		if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0)
		{
			break;
		}
	}
	return;
}

/** 初始化键盘 */
void init_keyboard(void)
{
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, KBC_MODE);
	return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4

/** 激活鼠标 */
void enable_mouse(void)
{
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	return; /* 顺利的话，键盘控制器会返送ACK（0xfa） */
}

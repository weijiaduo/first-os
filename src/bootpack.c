#include "bootpack.h"
#include <stdio.h>

unsigned int memtest(unsigned int start, unsigned int end);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	char mcursor[256];
	char keybuf[32];
	char mousebuf[128];
	struct MOUSE_DEC mdec;
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

	/* 内存检查 */
	i = memtest(0x00400000, 0xbfffffff) / (1024 * 1024);
	sprintf(s, "memory %dMB", i);
	putfonts8_asc(binfo->vram, binfo->scrnx, 0, 32, COL8_FFFFFF, s);

  for (;;)
  {
		io_cli();
		if (fifo8_status(&keyfifo) != 0)
		{
			/* 键盘数据 */
			i = fifo8_get(&keyfifo);
			io_sti();
			sprintf(s, "%02X", i);
			boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
			putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);
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
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
				putfonts8_asc(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, s);

				/* 鼠标指针的移动 */
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx + 15, my + 15); /* 隐藏当前鼠标 */
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
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 0, 79, 15); /* 隐藏坐标 */
				putfonts8_asc(binfo->vram, binfo->scrnx, 0 , 0, COL8_FFFFFF, s); /* 显示坐标 */
				putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); /* 描画鼠标 */
			}
		}
		else
		{
			io_stihlt();
		}
  }
}

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg;
	unsigned int cr0;
	unsigned i;

	/* 确认CPU是386还是486以上 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0)
	{
		/* 如果是386，即使设定AC=1，AC的值还是会自动回到0 */
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT; /* AC-bit=0 */
	io_store_eflags(eflg);

	if (flg486 != 0)
	{
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0)
	{
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
		store_cr0(cr0);
	}

	return i;
}

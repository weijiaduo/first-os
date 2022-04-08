#include "bootpack.h"
#include <stdio.h>

#define MEMMAN_FREES		4090 /* 大约是32KB */
#define MEMMAN_ADDR			0x003c0000

struct FREEINFO {
	unsigned int addr;
	unsigned int size;
};

struct MEMMAN { /* 内存管理 */
	int frees;
	int maxfrees;
	int lostsize;
	int losts;
	struct FREEINFO free[MEMMAN_FREES];
};

unsigned int memtest(unsigned int start, unsigned int end);
void memman_init(struct MEMMAN *man);
unsigned int memman_total(struct MEMMAN *man);
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size);
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	char s[40];
	char mcursor[256];
	char keybuf[32];
	char mousebuf[128];
	struct MOUSE_DEC mdec;
	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
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
	sprintf(s, "memory %dMB   free : %dKB",
			memtotal / (1024 * 1024), memman_total(memman) / 1024);
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

void memman_init(struct MEMMAN *man)
{
	man->frees = 0;			/* 可用信息数目 */
	man->maxfrees = 0;  /* 用于观察可用状况：frees的最大值 */
	man->lostsize = 0;  /* 释放失败的内存的大小总和 */
	man->losts = 0;     /* 释放失败次数 */
	return;
}

unsigned int memman_total(struct MEMMAN *man)
{
	unsigned int i;
	unsigned int t = 0;
	for (i = 0; i < man->frees; i++)
	{
		t += man->free[i].size;
	}
	return t;
}

/** 分配内存 */
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
{
	unsigned int i;
	unsigned int a;
	for (i = 0; i < man->frees; i++)
	{
		if (man->free[i].size >= size)
		{
			/* 找到了足够大的内存 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0)
			{
				/* 如果free[i]变成了0，就删除掉这一条记录 */
				man->frees--;
				for (; i < man->frees; i++)
				{
					man->free[i] = man->free[i + 1];
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用的空间 */
}

/** 释放内存 */
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
	int i;
	int j;

	/* 为了便于归纳内存，将free[]按照addr的顺序排列 */
	/* 所以，先决定应该放在哪里 */
	for (i = 0; i < man->frees; i++)
	{
		if (man->free[i].addr > addr)
		{
			break;
		}
	}

	if (i > 0)
	{
		/* 前面有可用的内存 */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr)
		{
			/* 可与前面的内存合并在一起 */
			man->free[i - 1].size += size;
			if (i < man->frees)
			{
				/* 查看后面的内存是否也可以合并 */
				if (addr + size == man->free[i].addr)
				{
					/* 也可以和后面的可用内存合并在一起 */
					man->free[i - 1].size += man->free[i].size;
					/* 删除free[i] */
					man->frees--;
					for (; i < man->frees; i++)
					{
						man->free[i] = man->free[i + 1];
					}
				}
			}
			return 0;
		}
	}

	if (i < man->frees)
	{
		/* 不能与前面的可用空间合并到一起 */
		if (addr + size == man->free[i].addr)
		{
			/* 可以与后面的内存合并在一起 */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0;
		}
	}

	/* 既不能与前面的合并，也不能与后面的合并 */
	if (man->frees < MEMMAN_FREES)
	{
		/* free[i]之后的向后移动，腾出空间 */
		for (j = man->frees; j > i; j--)
		{
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees)
		{
			/* 更新最大值 */
			man->maxfrees = man->frees;
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0;
	}

	/* 不能往后移动 */
	man->losts++;
	man->lostsize += size;
	return -1; /* 失败 */
}
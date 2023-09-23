#include <stdio.h>
#include <string.h>

#include "bootpack.h"

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *)ADR_BOOTINFO;

	unsigned int memtotal;
	struct MEMMAN *memman = (struct MEMMAN *)MEMMAN_ADDR;
	struct SHTCTL *shtctl;

	/* 任务A */
	struct TASK *task_a, *task;

	/* 背景图层 */
	struct SHEET *sht_back;
	unsigned char *buf_back;

	/* 鼠标图层 */
	struct SHEET *sht_mouse;
	unsigned char buf_mouse[256];

	/* 当前激活窗口图层 */
	struct SHEET *key_win;
	struct SHEET *sht = 0;

	/* 鼠标/键盘输入缓冲区 */
	struct FIFO32 fifo;
	int fifobuf[128];
	
	/* 键盘状态 */
	struct KEYBOARDINFO kinfo;
	kinfo.key_shift = 0;
	kinfo.key_leds = (binfo->leds >> 4) & 7;
	kinfo.keycmd_wait = -1;
	
	/* 鼠标状态 */
	struct MOUSEINFO minfo;
	minfo.old_mx = -1;
	minfo.old_my = -1;
	minfo.new_mx = -1;
	minfo.new_my = -1;
	minfo.old_wx = 0;
	minfo.old_wy = 0;
	minfo.new_wx = 0x7fffffff;
	minfo.new_wy = 0;
	minfo.wsht = 0;

	/* 字体文件 */
	int *fat;
	unsigned char *nihongo;
	struct FILEINFO *finfo;
	extern char hankaku[4096];
	
	int i, j;
	char s[40];

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 初始化PIC */
	init_pic();

	/* 允许CPU接收来自外部设备的中断 */
	io_sti();

	/* 初始化定时器中断 */
	init_pit();

	/* 启用PIC */
	io_out8(PIC0_IMR, 0xf8); /* 11111000 启用IRQ0（定时器）、IRQ1（键盘）和IRQ2（从PIC占用） */
	io_out8(PIC1_IMR, 0xef); /* 11101111 启用IRQ12（鼠标） */

	/* 初始化键盘输入和鼠标输入的缓冲区 */
	fifo32_init(&fifo, 128, fifobuf, 0);
	fifo32_init(&kinfo.keycmd, 32, kinfo.keycmd_buf, 0);

	/* 初始化键盘控制电路 */
	init_keyboard(&fifo, 256);

	/* 初始化并激活鼠标 */
	enable_mouse(&fifo, 512, &minfo.mdec);

	/* 内存管理 */
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000);

	/* 初始化调色板 */
	init_palette();

	/* 任务A，负责接收键盘鼠标数据 */
	task_a = task_init(memman);
	task_a->langmode = 0;
	fifo.task = task_a;
	task_run(task_a, 1, 2);
	*((int *) 0x0fec) = (int) &fifo;

	/* 初始化图层管理器 */
	shtctl = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	*((int *) 0x0fe4) = (int) shtctl;

	/* 背景图层 */
	sht_back = sheet_alloc(shtctl);
	buf_back = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny);
	sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

	/* 鼠标图层 */
	sht_mouse = sheet_alloc(shtctl);
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99);
	init_mouse_cursor8(buf_mouse, 99);
	minfo.mx = (binfo->scrnx - 16) / 2;
	minfo.my = (binfo->scrny - 28 - 16) / 2;

	/* 命令行窗口 */
	key_win = open_console(shtctl, memtotal);

	/* 设置图层位置和层级 */
	sheet_slide(sht_back, 0, 0);
	sheet_slide(key_win, 32, 4);
	sheet_slide(sht_mouse, minfo.mx, minfo.my);
	sheet_updown(sht_back, 0);
	sheet_updown(key_win, 1);
	sheet_updown(sht_mouse, 2);

	/* 默认激活窗口 */
	keywin_on(key_win);

	/* 为避免和键盘当前状态冲突，在一开始先进行设置 */
	fifo32_put(&kinfo.keycmd, KEYCMD_LED);
	fifo32_put(&kinfo.keycmd, kinfo.key_leds);

	/* 载入 nihongo.fnt 字体文件 */
	nihongo = (unsigned char *) memman_alloc_4k(memman, 16 * 256 + 32 * 94 * 55);
	fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	finfo = file_search("chgb2312.fnt", (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	/* finfo = file_search("nihongo.fnt", (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224); */
	if (finfo != 0)
	{
		file_loadfile(finfo->clustno, finfo->size, nihongo, fat, (char *) (ADR_DISKIMG + 0x003e00));
	}
	else
	{
		for (i = 0; i < 16 * 256; i++)
		{
			/* 没有字库，半角字符直接复制英文字库 */
			nihongo[i] = hankaku[i];
		}
		for (i = 16 * 256; i < 16 * 256 + 32 * 94 * 55; i++)
		{
			/* 没有字库，全角部分以0xff填充 */
			nihongo[i] = 0xff;
		}
	}
	*((int *) 0x0fe8) = (int) nihongo;
	memman_free_4k(memman, (int) fat, 4 * 2880);

	for (;;)
	{
		if (fifo32_status(&kinfo.keycmd) > 0 && kinfo.keycmd_wait < 0)
		{
			/* 如果存在向键盘控制器发送的数据，则发送它 */
			kinfo.keycmd_wait = fifo32_get(&kinfo.keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, kinfo.keycmd_wait);
		}
		io_cli();
		if (fifo32_status(&fifo) == 0)
		{
			/* FIFO为空，当存在搁置的绘图操作时，立即执行 */
			if (minfo.new_mx >= 0)
			{
				/* 鼠标移动 */
				io_sti();
				sheet_slide(sht_mouse, minfo.new_mx, minfo.new_my);
				minfo.new_mx = -1;
			}
			else if (minfo.new_wx != 0x7fffffff && minfo.wsht != 0)
			{
				/* 窗口移动 */
				io_sti();
				sheet_slide(minfo.wsht, minfo.new_wx, minfo.new_wy);
				minfo.new_wx = 0x7fffffff;
			}
			else
			{
				task_sleep(task_a);
				io_sti();
			}
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti();

			/* 输入窗口被关闭了 */
			if (key_win != 0 && key_win->flags == 0)
			{
				if (shtctl->top == 1)
				{
					/* 当界面上只剩下背景和鼠标时 */
					key_win = 0;
				}
				else
				{
					/* 聚焦到最顶上的图层 */
					key_win = shtctl->sheets[shtctl->top - 1];
					keywin_on(key_win);
				}
			}

			if (256 <= i && i <= 511)
			{
				/* 键盘数据 */
				key_win = keyboard_handle(&kinfo, i, key_win, shtctl, memtotal);
			}
			else if (512 <= i && i <= 767)
			{
				/* 鼠标数据 */
				key_win = mouse_handle(&minfo, i, key_win, shtctl);
			}
			else if (768 <= i && i <= 1023)
			{
				/* 命令行关闭（窗口和任务一起关闭） */
				close_console(shtctl->sheets0 + (i - 768));
			}
			else if (1024 <= i && i <= 2023)
			{
				/* 命令行任务关闭（仅关闭任务） */
				close_constask(taskctl->tasks0 + (i - 1024));
			}
			else if (2024 <= i && i <= 2279)
			{
				/* 命令行窗口关闭（仅关闭窗口） */
				sht = shtctl->sheets0 + (i - 2024);
				memman_free_4k(memman, (int) sht->buf, 256 * 165);
				sheet_free(sht);
			}
		}
	}
}

/**
 * @brief 键盘事件处理
 */
struct SHEET * keyboard_handle(struct KEYBOARDINFO *kinfo, int i, struct SHEET *key_win,
	struct SHTCTL *shtctl, unsigned int memtotal)
{
	/* 没按下 Shift 键时 */
	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0x08, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0x0a, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	/* 按下 Shift 键时 */
	static char keytable1[0x80] = {
		0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0x08, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0x0a, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};

	int j;
	char s[40];
	struct TASK *task;

	if (i < 0x80 + 256)
	{
		if (kinfo->key_shift == 0)
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
		if (((kinfo->key_leds & 4) == 0 && kinfo->key_shift == 0) ||
			((kinfo->key_leds & 4) != 0 && kinfo->key_shift != 0))
		{
			/* 大小写转换 */
			s[0] += 0x20;
		}
	}

	/* 一般字符/退格键/回车键 */
	if (key_win != 0 && s[0] != 0)
	{
		/* 发送给命令行窗口 */
		fifo32_put(&key_win->task->fifo, s[0] + 256);
	}

	/* Tab 键 */
	if (key_win != 0 && i == 256 + 0x0f)
	{
		keywin_off(key_win);
		j = key_win->height - 1;
		/* 0层是背景，top层是鼠标 */
		j = j == 0 ? shtctl->top - 1 : j;
		key_win = shtctl->sheets[j];
		keywin_on(key_win);
	}

	/* 左 Shift ON */
	if (i == 256 + 0x2a)
	{
		kinfo->key_shift |= 1;
	}
	/* 右 Shift ON */
	if (i == 256 + 0x36)
	{
		kinfo->key_shift |= 2;
	}
	/* 左 Shift OFF */
	if (i == 256 + 0xaa)
	{
		kinfo->key_shift &= ~1;
	}
	/* 右 Shift OFF */
	if (i == 256 + 0xb6)
	{
		kinfo->key_shift &= ~2;
	}

	/* CapsLock */
	if (i == 256 + 0x3a)
	{
		kinfo->key_leds ^= 4;
		fifo32_put(&kinfo->keycmd, KEYCMD_LED);
		fifo32_put(&kinfo->keycmd, kinfo->key_leds);
	}
	/* NumberLock */
	if (i == 256 + 0x45)
	{
		kinfo->key_leds ^= 2;
		fifo32_put(&kinfo->keycmd, KEYCMD_LED);
		fifo32_put(&kinfo->keycmd, kinfo->key_leds);
	}
	/* ScrollLock */
	if (i == 256 + 0x46)
	{
		kinfo->key_leds ^= 1;
		fifo32_put(&kinfo->keycmd, KEYCMD_LED);
		fifo32_put(&kinfo->keycmd, kinfo->key_leds);
	}

	/* Shift+F1，强制结束应用程序 */
	if (key_win != 0 && i == 256 + 0x3b && kinfo->key_shift != 0)
	{
		task = key_win->task;
		if (task != 0 && task->tss.ss0 != 0)
		{
			cons_putstr0(task->cons, "\nBreak(key):\n");
			io_cli(); /* 不能在改变寄存器值时切换到其他任务 */
			task->tss.eax = (int) &(task->tss.esp0);
			task->tss.eip = (int) asm_end_app;
			io_sti();
			/* 任务处于休眠状态则唤醒，保证结束应用能够执行 */
			task_run(task, -1, 0);
		}
	}

	/* Shift+F2，打开新的命令行窗口 */
	if (i == 256 + 0x3c && kinfo->key_shift != 0)
	{
		/* 自动将输入焦点切换到新打开的窗口 */
		keywin_off(key_win);
		key_win = open_console(shtctl, memtotal);
		sheet_slide(key_win, 32, 4);
		sheet_updown(key_win, shtctl->top);
		keywin_on(key_win);
	}

	/* F11，把最下面的窗口移到最上面 */
	if (i == 256 + 0x57 && shtctl->top > 2)
	{
		/* 0图层是背景，top层是鼠标 */
		sheet_updown(shtctl->sheets[1], shtctl->top - 1);
	}

	/* 键盘成功接收到数据 */
	if (i == 256 + 0xfa)
	{
		kinfo->keycmd_wait = -1;
	}
	/* 键盘没有接收到数据 */
	if (i == 256 + 0xfe)
	{
		/* 重新发送数据 */
		wait_KBC_sendready();
		io_out8(PORT_KEYDAT, kinfo->keycmd_wait);
	}
	return key_win;
}

/**
 * @brief 鼠标事件处理
 */
struct SHEET * mouse_handle(struct MOUSEINFO *minfo, int i, struct SHEET *key_win, struct SHTCTL *shtctl)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *)ADR_BOOTINFO;
	struct SHEET *sht = minfo->wsht;
	struct TASK *task;
	int x, y, j;
	char *s[40];

	if (mouse_decode(&minfo->mdec, i - 512) != 0)
	{
		/* 鼠标指针的移动 */
		minfo->mx += minfo->mdec.x;
		minfo->my += minfo->mdec.y;
		if (minfo->mx < 0)
		{
			minfo->mx = 0;
		}
		if (minfo->my < 0)
		{
			minfo->my = 0;
		}
		if (minfo->mx > binfo->scrnx - 1)
		{
			minfo->mx = binfo->scrnx - 1;
		}
		if (minfo->my > binfo->scrny - 1)
		{
			minfo->my = binfo->scrny - 1;
		}

		/* 记录鼠标位置 */
		minfo->new_mx = minfo->mx;
		minfo->new_my = minfo->my;
		if ((minfo->mdec.btn & 0x01) != 0)
		{
			/* 按下左键 */
			if (minfo->old_mx < 0)
			{
				/* 鼠标通常模式 */
				/* 按照从上到下的顺序寻找鼠标所指向的图层 */
				for (j = shtctl->top - 1; j > 0; j--)
				{
					sht = shtctl->sheets[j];
					x = minfo->mx - sht->vx0;
					y = minfo->my - sht->vy0;
					if (0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize)
					{
						/* 判断鼠标点击的是窗口的可见区域 */
						if (sht->buf[y * sht->bxsize + x] != sht->col_inv)
						{
							/* 激活选中的图层窗口 */
							minfo->wsht = sht;
							sheet_updown(sht, shtctl->top - 1);
							if (sht != key_win)
							{
								keywin_off(key_win);
								key_win = sht;
								keywin_on(key_win);
							}
							
							/* 点击标题栏，则进入窗口移动模式 */
							if (3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21)
							{
								minfo->old_mx = minfo->mx;
								minfo->old_my = minfo->my;
								minfo->old_wx = sht->vx0;
								minfo->new_wy = sht->vy0;
							}
							
							/* 点击关闭按钮，则关闭窗口 */
							if (sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19)
							{
								if ((sht->flags & 0x10) != 0)
								{
									/* 应用程序窗口 */
									task = sht->task;
									cons_putstr0(task->cons, "\nBreak(mouse): \n");
									io_cli(); /* 强制结束处理中，禁止切换任务 */
									task->tss.eax = (int) &(task->tss.esp0);
									task->tss.eip = (int) asm_end_app;
									io_sti();
									/* 任务处于休眠状态则唤醒，保证结束应用能够执行 */
									task_run(task, -1, 0);
								}
								else
								{
									/* 命令行窗口 */
									/* 暂且隐藏该图层 */
									sheet_updown(sht, -1);
									keywin_off(key_win);
									key_win = shtctl->sheets[shtctl->top - 1];
									keywin_on(key_win);
									/* 通知任务窗口关闭 */
									task = sht->task;
									io_cli();
									fifo32_put(&task->fifo, 4);
									io_sti();
								}
							}
							break;
						}
					}
				}
			}
			else
			{
				/* 窗口移动模式 */
				x = minfo->mx - minfo->old_mx;
				y = minfo->my - minfo->old_my;
				/* +2 是为了避免 AND 四舍五入后引起的窗口左移 */
				/* 加上 &~3 是为了使得 x 坐标大小变成 4 的倍数 */
				minfo->new_wx = (minfo->old_wx + x + 2) & ~3;
				minfo->new_wy = minfo->new_wy + y;
				minfo->old_my = minfo->my;
			}
		}
		else
		{
			/* 没有按下左键，返回通常模式 */
			minfo->old_mx = -1;
			minfo->old_my = -1;
			/* 松开左键后，要立即更新窗口的位置 */
			if (minfo->new_wx != 0x7fffffff && sht != 0)
			{
				sheet_slide(sht, minfo->new_wx, minfo->new_wy);
				minfo->new_wx = 0x7fffffff;
			}
		}
	}
	return key_win;
}

/**
 * @brief 失活窗口的标题颜色和光标
 * 
 * @param key_win 失活窗口
 */
void keywin_off(struct SHEET *key_win)
{
	if (key_win == 0) { return; }
	change_wtitle8(key_win, 0);
	if ((key_win->flags & 0x20) != 0)
	{
		/* 命令行窗口光标 OFF */
		fifo32_put(&key_win->task->fifo, 3);
	}
	return;
}

/**
 * @brief 激活窗口的标题颜色和光标
 * 
 * @param key_win 激活窗口
 */
void keywin_on(struct SHEET *key_win)
{
	if (key_win == 0) { return; }
	change_wtitle8(key_win, 1);
	if ((key_win->flags & 0x20) != 0)
	{
		/* 命令行窗口光标 ON */
		fifo32_put(&key_win->task->fifo, 2);
	}
	return;
}

/**
 * @brief 新建命令行窗口
 * 
 * @param shtctl 图层管理器
 * @param memtotal 内存大小
 * @return struct SHEET* 命令行窗口图层
 */
struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal)
{
	/* 命令行窗口 */
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct SHEET *sht = sheet_alloc(shtctl);
	unsigned char *buf = (unsigned char *)memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht, buf, 256, 165, -1); /* 无透明色 */
	make_window8(buf, 256, 165, "console", 0); /* 窗口范围，包括标题栏 */
	make_textbox8(sht, 8, 28, 240, 128, COL8_000000); /* 输入窗口范围 */

	/* 绑定命令行的图层和任务 */
	sht->task = open_constask(sht, memtotal);
	/* 命令行窗口启用光标 */
	sht->flags |= 0x20;
	return sht;
}

/**
 * @brief 新建命令行任务
 * 
 * @param sht 命令行图层
 * @param memtotal 内存总大小
 * @return struct TASK* 命令行任务
 */
struct TASK *open_constask(struct SHEET *sht, unsigned int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_alloc();
	task->cons_stack = memman_alloc_4k(memman, 64 * 1024);
	task->tss.esp = task->cons_stack + 64 * 1024 - 12;
	task->tss.eip = (int)&console_task;
	task->tss.es = 1 * 8;
	task->tss.cs = 2 * 8;
	task->tss.ss = 1 * 8;
	task->tss.ds = 1 * 8;
	task->tss.fs = 1 * 8;
	task->tss.gs = 1 * 8;
	*((int *)(task->tss.esp + 4)) = (int)sht;
	*((int *)(task->tss.esp + 8)) = memtotal;

	task_run(task, 2, 2); /* level=2, priority=2 */

	/* 输入缓冲区 */
	int *fifo = (int *) memman_alloc_4k(memman, 128 * 4);
	fifo32_init(&task->fifo, 128, fifo, task);
	return task;
}

/**
 * @brief 关闭命令行窗口
 * 
 * @param sht 命令行窗口图层
 */
void close_console(struct SHEET *sht)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = sht->task;
	memman_free_4k(memman, sht->buf, 256 * 265);
	sheet_free(sht);
	close_constask(task);
	return;
}

/**
 * @brief 关闭命令行任务
 * 
 * @param task 命令行任务
 */
void close_constask(struct TASK *task)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	task_sleep(task);
	memman_free_4k(memman, task->cons_stack, 64 * 1024);
	memman_free_4k(memman, (int) task->fifo.buf, 128 * 4);
	task->flags = 0; /* 用来代替 task_free(task) */
	return;
}
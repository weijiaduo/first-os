#include <stdio.h>
#include <string.h>

#include "bootpack.h"

/**
 * @brief 命令行窗口任务
 * 
 * @param sheet 命令行窗口图层
 * @param memtotal 内存总量
 */
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
	struct MENMAN *memman = (struct MENMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600); /* 文件名的起始地址 */
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;

	/* 解压FAT文件分配表 */
	int *fat = (int *) memman_alloc_4k(memman, 4 * 2880);
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));

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
						sprintf(s, "free %dKB", memman_total(memman) / 1024);
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
							p = (char *) memman_alloc_4k(memman, finfo[x].size);
							file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
							cursor_x = 8;
							for (y = 0; y < finfo[x].size; y++)
							{
								/* 逐字输出 */
								s[0] = p[y];
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
							/* 释放文件内容内存 */
							memman_free_4k(memman, (int) p, finfo[x].size);
						}
						else
						{
							/* 没有找到文件的情况 */
							putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if (strcmp(cmdline, "hlt") == 0)
					{
						/* hlt应用程序 */
						for (y = 0; y < 11; y++)
						{
							s[y] = ' ';
						}
						s[0] = 'H';
						s[1] = 'L';
						s[2] = 'T';
						s[8] = 'H';
						s[9] = 'R';
						s[10] = 'B';
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
										goto hlt_next_file;
									}
								}
								/* 找到文件名 */
								break;
							}
hlt_next_file:
							x++;
						}
						if (x < 24 && finfo[x].name[0] != 0x00)
						{
							/* 找到文件的情况 */
							p = (char *) memman_alloc_4k(memman, finfo[x].size);
							/* 从磁盘加载文件内容到内存中 */
							file_loadfile(finfo[x].clustno, finfo[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
							/* 为文件创建一个代码段 */
							set_segmdesc(gdt + 1003, finfo[x].size - 1, (int) p, AR_CODE32_ER);
							/* 跳到文件代码段里执行代码 */
							farjmp(0, 1003 * 8);
							/* 释放文件内容内存 */
							memman_free_4k(memman, (int) p, finfo[x].size);
						}
						else
						{
							/* 没有找到文件的情况 */
							putfonts_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = cons_newline(cursor_y, sheet);
						}
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

/**
 * @brief 控制台窗口输出换行
 * 
 * @param cursor_y 当前行坐标
 * @param sheet 当前图层
 * @return int 下一行的坐标
 */
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
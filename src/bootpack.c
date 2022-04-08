#include <stdio.h>

/* 声明函数，函数定义在别的文件里 */
void io_hlt(void);
void io_cli(void);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);

/* 调色板 */
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);

/* 绘制 */
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);

/* 打印 */
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s);

/* 鼠标 */
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buf, int bxsize);

#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15

struct BOOTINFO {
	char cyls;
	char leds;
	char vmode;
	char reserve;
	short scrnx;
	short scrny;
	char *vram;
};

struct SEGMENT_DESCRIPTOR {
	short limit_low;
	short base_low;
	char base_mid;
	char access_right;
	char limit_high;
	char base_high;
};

struct GATE_DESCRIPTOR {
	short offset_low;
	short selector;
	char dw_count;
	char access_right;
	short offset_high;
};

/* 段表、中断记录表 */
void init_gdtidt(void);
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) 0x0ff0;
	char s[40];
	char mcursor[256];
	int mx;
	int my;

	/* 初始化段表和中断记录表 */
	init_gdtidt();

	/* 设定调色板 */
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

  for (;;)
  {
    io_hlt();
  }
}

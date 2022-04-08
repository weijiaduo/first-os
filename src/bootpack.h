/** asmhead.nas */
#define ADR_BOOTINFO 0x00000ff0
struct BOOTINFO {
	char cyls;    /* 启动区磁盘读到何处为止 */
	char leds;    /* 启动时键盘LED的状态 */
	char vmode;   /* 显卡模式为多少位彩色 */
	char reserve;
	short scrnx;  /* 画面分辨率 */
	short scrny;
	char *vram;
};


/** naskfunc.nas */
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
int io_in8(int port);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);

void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);

int load_cr0(void);
void store_cr0(int cr0);

void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);


/** fifo.c */
struct FIFO8 {
	unsigned char *buf;
	int p;
	int q;
	int size;
	int free;
	int flags;
};
void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf);
int fifo8_put(struct FIFO8 *fifo, unsigned char data);
int fifo8_get(struct FIFO8 *fifo);
int fifo8_status(struct FIFO8 *fifo);


/** graphic.c */
/* 调色板 */
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


/** dsctbl.c */
/* 段表、中断记录表 */
#define ADR_IDT			0x0026f800
#define LIMIT_IDT		0x000007ff
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a
#define AR_INTGATE32	0x008e

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

void init_gdtidt(void);
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);


/** int.c */
#define PIC0_ICW1		0x0020
#define PIC0_OCW2		0x0020
#define PIC0_IMR		0x0021
#define PIC0_ICW2		0x0021
#define PIC0_ICW3		0x0021
#define PIC0_ICW4		0x0021
#define PIC1_ICW1		0x00a0
#define PIC1_OCW2		0x00a0
#define PIC1_IMR		0x00a1
#define PIC1_ICW2		0x00a1
#define PIC1_ICW3		0x00a1
#define PIC1_ICW4		0x00a1

void init_pic(void);
void inthandler27(int *esp);


/** keyboard.c */
extern struct FIFO8 keyfifo;
#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064

void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(void);


/** mouse.c */
extern struct FIFO8 mousefifo;
struct MOUSE_DEC {
	unsigned char buf[3];
	unsigned char phase;
	int x;
	int y;
	int btn;
};

void inthandler2c(int *esp);
void enable_mouse(struct MOUSE_DEC *mdec);
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);


/** memory.c */
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
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size);
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size);


/** sheet.c */
#define MAX_SHEETS		256

struct SHEET {
	struct SHTCTL *ctl;
	unsigned char *buf;
	int bxsize;
	int bysize;
	int vx0;
	int vy0;
	int col_inv;
	int height;
	int flags;
};
struct SHTCTL {
	unsigned char *vram;
	unsigned char *map;
	int xsize;
	int ysize;
	int top;
	struct SHEET *sheets[MAX_SHEETS];
	struct SHEET sheets0[MAX_SHEETS];
};

struct SHTCTL * shtctl_init(struct MEMMAN *man, unsigned char *vram, int xsize, int ysize);
struct SHEET * sheet_alloc(struct SHTCTL *ctl);
void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv);
void sheet_updown(struct SHEET *sht, int height);
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1);
void sheet_slide(struct SHEET *sht, int vx0, int vy0);
void sheet_free(struct SHEET *sht);

/* timer.c */
void init_pit(void);
void inthandler20(int *esp);
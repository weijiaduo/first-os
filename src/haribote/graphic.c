#include "bootpack.h"

void init_palette(void)
{
    static unsigned char table_rgb[16 * 3] = {
        0x00, 0x00, 0x00, /*  0: 黑 */
        0xff, 0x00, 0x00, /*  1: 亮红 */
        0x00, 0xff, 0x00, /*  2: 亮绿 */
        0xff, 0xff, 0x00, /*  3: 亮黄 */
        0x00, 0x00, 0xff, /*  4: 亮蓝 */
        0xff, 0x00, 0xff, /*  5: 亮紫 */
        0x00, 0xff, 0xff, /*  6: 浅亮蓝 */
        0xff, 0xff, 0xff, /*  7: 白 */
        0xc6, 0xc6, 0xc6, /*  8: 亮灰 */
        0x84, 0x00, 0x00, /*  9: 暗红 */
        0x00, 0x84, 0x00, /* 10: 暗绿 */
        0x84, 0x84, 0x00, /* 11: 暗黄 */
        0x00, 0x00, 0x84, /* 12: 暗蓝 */
        0x84, 0x00, 0x84, /* 13: 暗紫 */
        0x00, 0x84, 0x84, /* 14: 浅暗蓝 */
        0x84, 0x84, 0x84  /* 15: 暗灰 */
    };
    set_palette(0, 15, table_rgb);

    int r, g, b;
    unsigned char table2[216 * 3];
    for (b = 0; b < 6; b++)
    {
        for (g = 0; g < 6; g++)
        {
            for (r = 0; r < 6; r++)
            {
                table2[(r + g * 6 + b * 36) * 3 + 0] = r * 51;
                table2[(r + g * 6 + b * 36) * 3 + 1] = g * 51;
                table2[(r + g * 6 + b * 36) * 3 + 2] = b * 51;
            }
        }
    }
    set_palette(16, 231, table2);
    return;

    /* C语言中的 static char 语句只能用于数据，相当于汇编中的 DB 指令 */
}

void set_palette(int start, int end, unsigned char *rgb)
{
    int i;
    int eflags;
    eflags = io_load_eflags(); /* 记录中断许可标志的值 */
    io_cli();
    io_out8(0x03c8, start);
    for (i = start; i <= end; i++)
    {
        io_out8(0x03c9, rgb[0] / 4);
        io_out8(0x03c9, rgb[1] / 4);
        io_out8(0x03c9, rgb[2] / 4);
        rgb += 3;
    }
    io_store_eflags(eflags); /* 复原中断许可标志 */
    return;
}

/**
 * 绘制指定的区域范围
 */
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
            vram[y * xsize + x] = c;
    }
    return;
}

void init_screen8(char *vram, int x, int y)
{
    /* 主界面和工具栏 */
    boxfill8(vram, x, COL8_008484, 0, 0, x - 1, y - 29);
    boxfill8(vram, x, COL8_C6C6C6, 0, y - 28, x - 1, y - 28);
    boxfill8(vram, x, COL8_FFFFFF, 0, y - 27, x - 1, y - 27);
    boxfill8(vram, x, COL8_C6C6C6, 0, y - 26, x - 1, y - 1);

    /* 工具栏左下角菜单 */
    boxfill8(vram, x, COL8_FFFFFF, 3, y - 24, 59, y - 24);
    boxfill8(vram, x, COL8_FFFFFF, 2, y - 24, 2, y - 4);
    boxfill8(vram, x, COL8_848484, 3, y - 4, 59, y - 4);
    boxfill8(vram, x, COL8_848484, 59, y - 23, 59, y - 5);
    boxfill8(vram, x, COL8_000000, 2, y - 3, 59, y - 3);
    boxfill8(vram, x, COL8_000000, 60, y - 24, 60, y - 3);

    /* 工具栏右下角图标 */
    boxfill8(vram, x, COL8_848484, x - 47, y - 24, x - 4, y - 24);
    boxfill8(vram, x, COL8_848484, x - 47, y - 23, x - 47, y - 4);
    boxfill8(vram, x, COL8_FFFFFF, x - 47, y - 3, x - 4, y - 3);
    boxfill8(vram, x, COL8_FFFFFF, x - 3, y - 24, x - 3, y - 3);
    return;
}

/**
 * 打印指定的字符
 */
void putfont8(char *vram, int xsize, int x, int y, char c, char *font)
{
    int i, j;
    char *p;
    char d;
    static int key[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    for (i = 0; i < 16; i++)
    {
        p = vram + (y + i) * xsize + x;
        d = font[i];
        for (j = 0; j < 8; j++)
        {
            if ((d & key[j]) != 0)
            {
                p[j] = c;
            }
        }
    }
    return;
}

/**
 * @brief 打印字符串
 */
void putfonts8_str(char *vram, int xsize, int x, int y, char c, unsigned char *s)
{
    struct TASK *task = task_now();
    if (task->langmode == 0)
    {
        /* 英文 */
        putfonts8_asc(vram, xsize, x, y, c, s);
    }
    else if (task->langmode == 1 || task->langmode == 2)
    {
        /* 日文 */
        putfonts8_jp(vram, xsize, x, y, c, s);
    }
    else if (task->langmode == 3)
    {
        /* 中文 */
        putfonts8_ch(vram, xsize, x, y, c, s);
    }
    return;
}

/**
 * 按ASCII码打印字符串
 */
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s)
{
    extern char hankaku[4096];
    for (; *s != 0x00; s++)
    {
        putfont8(vram, xsize, x, y, c, hankaku + (*s) * 16);
        x += 8;
    }
    return;
}

/**
 * @brief 按日文打印字符串
 */
void putfonts8_jp(char *vram, int xsize, int x, int y, char c, unsigned char *s)
{
    struct TASK *task = task_now();
    char *nihongo = (char *) *((int *) 0x0fe8);
    char *font;
    int k, t;

    /* 日文 Shift-JIS */
    if (task->langmode == 1)
    {
        for (; *s != 0x00; s++)
        {
            if (task->langbyte1 == 0)
            {
                if ((0x81 <= *s && *s <= 0x9f) || (0xe0 <= *s && *s <= 0xfc))
                {
                    /* 全角字符（第一字节） */
                    task->langbyte1 = *s;
                }
                else
                {
                    /* 半角字符 */
                    putfont8(vram, xsize, x, y, c, nihongo + (*s) * 16);
                }
            }
            else
            {
                /* 全角字符（第二字节） */
                if (0x81 <= task->langbyte1 && task->langbyte1 <= 0x9f)
                {
                    k = (task->langbyte1 - 0x81) * 2;
                }
                else
                {
                    k = (task->langbyte1 - 0xe0) * 2 + 62;
                }
                if (0x40 <= *s && *s <= 0x7e)
                {
                    t = *s - 0x40;
                }
                else if (0x80 <= *s && *s <= 0x9e)
                {
                    t = *s - 0x80 + 63;
                }
                else
                {
                    t = *s - 0x9f;
                    k++;
                }
                task->langbyte1 = 0;
                font = nihongo + 256 * 16 + (k * 94 + t) * 32;
                putfont8(vram, xsize, x - 8, y, c, font);   /* 左半部分 */
                putfont8(vram, xsize, x, y, c, font + 16);  /* 右半部分 */
            }
            x += 8;
        }
    }
    /* 日文 EUC */
    if (task->langmode == 2)
    {
        for (; *s != 0x00; s++)
        {
            if (task->langbyte1 == 0)
            {
                if (0x81 <= *s && *s <= 0xfe)
                {
                    /* 全角字符（第一字节） */
                    task->langbyte1 = *s;
                }
                else
                {
                    /* 半角字符 */
                    putfont8(vram, xsize, x, y, c, nihongo + (*s) * 16);
                }
            }
            else
            {
                /* 全角字符（第二字节） */
                k = task->langbyte1 - 0xa1;
                t = *s - 0xa1;
                task->langbyte1 = 0;
                font = nihongo + 256 * 16 + (k * 94 + t) * 32;
                putfont8(vram, xsize, x - 8, y, c, font);   /* 左半部分 */
                putfont8(vram, xsize, x, y, c, font + 16);  /* 右半部分 */
            }
            x += 8;
        }
    }
}

/**
 * @brief 按中文GB2312打印字符串
 */
void putfonts8_ch(char *vram, int xsize, int x, int y, char c, unsigned char *s)
{
    struct TASK *task = task_now();
    char *nihongo = (char *) *((int *) 0x0fe8);
    char *font;
    int i, k, t;

    for (; *s != 0x00; s++)
    {
        if (task->langbyte1 == 0)
        {
            if (0x81 <= *s && *s <= 0xfe)
            {
                /* 全角字符（第一字节） */
                task->langbyte1 = *s;
            }
            else
            {
                /* 半角字符 */
                putfont8(vram, xsize, x, y, c, nihongo + (*s) * 16);
            }
        }
        else
        {
            /* 全角字符（第二字节） */
            k = task->langbyte1 - 0xa1;
            t = *s - 0xa1;
            task->langbyte1 = 0;
            font = nihongo + 256 * 16 + (k * 94 + t) * 32;
            /* HZK16字模的字节排列顺序和日文的不一样 */
            char byte1[16], byte2[16];
            for (i = 0; i < 16; i++)
            {
                byte1[i] = *(font + i * 2);
                byte2[i] = *(font + i * 2 + 1);
            }
            putfont8(vram, xsize, x - 8, y, c, byte1);  /* 左半部分 */
            putfont8(vram, xsize, x, y, c, byte2);      /* 右半部分 */
        }
        x += 8;
    }
}

/**
 * 初始化图标
 */
void init_mouse_cursor8(char *mouse, char bc)
{
    static char cursor[16][16] = {
        "**************..",
        "*OOOOOOOOOOO*...",
        "*OOOOOOOOOO*....",
        "*OOOOOOOOO*.....",
        "*OOOOOOOO*......",
        "*OOOOOOO*.......",
        "*OOOOOOO*.......",
        "*OOOOOOOO*......",
        "*OOOO**OOO*.....",
        "*OOO*..*OOO*....",
        "*OO*....*OOO*...",
        "*O*......*OOO*..",
        "**........*OOO*.",
        "*..........*OOO*",
        "............*OO*",
        ".............***"};

    int x;
    int y;
    for (y = 0; y < 16; y++)
    {
        for (x = 0; x < 16; x++)
        {
            if (cursor[y][x] == '*')
            {
                mouse[y * 16 + x] = COL8_000000;
            }
            if (cursor[y][x] == 'O')
            {
                mouse[y * 16 + x] = COL8_FFFFFF;
            }
            if (cursor[y][x] == '.')
            {
                mouse[y * 16 + x] = bc;
            }
        }
    }
    return;
}

void putblock8_8(char *vram, int vxsize, int pxsize,
                 int pysize, int px0, int py0, char *buf, int bxsize)
{
    int x;
    int y;
    for (y = 0; y < pysize; y++)
    {
        for (x = 0; x < pxsize; x++)
        {
            vram[(py0 + y) * vxsize + (px0 + x)] = buf[y * bxsize + x];
        }
    }
    return;
}
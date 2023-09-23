#include "bootpack.h"

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE 0xf4

struct FIFO32 *mousefifo;
int mousedata0;

/**
 * 2c号中断处理程序
 * PS/2 鼠标的中断
 */
void inthandler2c(int *esp)
{
    int data;
    io_out8(PIC1_OCW2, 0x64); /* 通知PIC1 IRQ-12的受理已完成 */
    io_out8(PIC0_OCW2, 0x62); /* 通知PIC0 IRQ-02的受理已完成 */

    data = io_in8(PORT_KEYDAT);
    fifo32_put(mousefifo, data + mousedata0);
    return;
}

/** 激活鼠标 */
void enable_mouse(struct FIFO32 *fifo, int data0, struct MOUSE_DEC *mdec)
{
    /* 把缓存信息保存到全局变量 */
    mousefifo = fifo;
    mousedata0 = data0;
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);

    /* 顺利的话，键盘控制器会返送ACK（0xfa） */
    mdec->phase = 0;
    return;
}

/** 鼠标数据解码 */
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
    if (mdec->phase == 0)
    {
        /* 等待鼠标0xfa的状态 */
        if (dat == 0xfa)
        {
            mdec->phase = 1;
        }
        return 0;
    }
    if (mdec->phase == 1)
    {
        /* 等待鼠标的第一字节 */
        if ((dat & 0xc8) == 0x08)
        {
            /* 如果第一字节正确 */
            mdec->buf[0] = dat;
            mdec->phase = 2;
        }
        return 0;
    }
    if (mdec->phase == 2)
    {
        /* 等待鼠标的第二字节 */
        mdec->buf[1] = dat;
        mdec->phase = 3;
        return 0;
    }
    if (mdec->phase == 3)
    {
        /* 等待鼠标的第三字节 */
        mdec->buf[2] = dat;
        mdec->phase = 1;

        mdec->btn = mdec->buf[0] & 0x07;
        mdec->x = mdec->buf[1];
        mdec->y = mdec->buf[2];

        if ((mdec->buf[0] & 0x10) != 0)
        {
            mdec->x |= 0xffffff00;
        }
        if ((mdec->buf[0] & 0x20) != 0)
        {
            mdec->y |= 0xffffff00;
        }
        /* 鼠标的y方向与画面符号相反 */
        mdec->y = -mdec->y;

        return 1;
    }
    /* 应该不可能到这里来 */
    return -1;
}

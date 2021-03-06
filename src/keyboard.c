#include "bootpack.h"

#define PORT_KEYDAT 0x0060
#define PORT_KEYSTA 0x0064
#define PORT_KEYCMD 0x0064
#define KEYSTA_SEND_NOTREADY 0x02
#define KEYCMD_WRITE_MODE 0x60
#define KBC_MODE 0x47

struct FIFO32 *keyfifo;
int keyboard0;

/**
 * 21号中断处理程序
 * PS/2键盘的中断
 */
void inthandler21(int *esp)
{
    int data;
    io_out8(PIC0_OCW2, 0x61); /* 通知PIC"IRQ-01已经受理完毕" */

    data = io_in8(PORT_KEYDAT); /* 获取键盘中断数据 */
    fifo32_put(keyfifo, data + keyboard0);
    return;
}

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
void init_keyboard(struct FIFO32 *fifo, int data0)
{
    /* 把缓存信息保存到全局变量 */
    keyfifo = fifo;
    keyboard0 = data0;
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
    return;
}
#include "bootpack.h"
#include<stdio.h>

#define PORT_KEYDAT		0x0060

/** PIC的初始化 */
void init_pic(void)
{
  io_out8(PIC0_IMR, 0xff);    /* 禁止所有中断 */
  io_out8(PIC1_IMR, 0xff);    /* 禁止所有中断 */

  io_out8(PIC0_ICW1, 0x11);   /* 边缘触发模式（edge trigger mode） */
  io_out8(PIC0_ICW2, 0x20);   /* IRQ0-7由INT20-27接收 */
  io_out8(PIC0_ICW3, 1 << 2); /* PIC1由IRQ2连接 */
  io_out8(PIC0_ICW4, 0x01);   /* 无缓冲模式 */

  io_out8(PIC1_ICW1, 0x11);   /* 边缘触发模式（edge trigger mode） */
  io_out8(PIC1_ICW2, 0x28);   /* IRQ1-8由INT28-2f接收 */
  io_out8(PIC1_ICW3, 2);      /* PIC1由IRQ2连接 */
  io_out8(PIC1_ICW4, 0x01);   /* 无缓冲模式 */

  io_out8(PIC0_IMR, 0xfb);    /* 11111011 PIC1以外全部禁止 */
  io_out8(PIC1_IMR, 0xff);    /* 11111111 禁止所有中断 */

  return;
}

/**
 * 21号中断处理程序
 * PS/2键盘的中断
 */ 
void inthandler21(int *esp)
{
  struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
  unsigned char data;
  unsigned char s[4];

  io_out8(PIC0_OCW2, 0x61); /* 通知PIC"IRQ-01已经受理完毕" */

  data = io_in8(PORT_KEYDAT); /* 获取键盘中断数据 */

  sprintf(s, "%02X", data);
  boxfill8(binfo->vram, binfo->scrnx, COL8_000000, 0, 16, 15, 31);
  putfonts8_asc(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, s);

  return;
}

/**
 * 2c号中断处理程序
 * PS/2 鼠标的中断
 */ 
void inthandler2c(int *esp)
{
  struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
  boxfill8(binfo->vram, binfo->scrnx, COL8_000000, 0, 0, 32 * 8 - 1, 15);
  putfonts8_asc(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, "INT2C (IRQ-12) : PS/2 mouse");

  for (;;)
  {
    io_hlt();
  }
}

void inthandler27(int *esp)
{
  io_out8(PIC0_OCW2, 0x67);
  return;
}
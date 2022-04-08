/* 声明函数，函数定义在别的文件里 */
void io_hlt(void);

void HariMain(void)
{
fin:
  io_hlt(); /* 执行 naskfunc.nas 里的 _io_hlt */
  goto fin;
}
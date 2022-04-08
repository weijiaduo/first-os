/* 声明函数，函数定义在别的文件里 */
void io_hlt(void);

void HariMain(void)
{
  int i;
  char *p;

  for (i = 0xa0000; i <= 0xaffff; i++)
  {
    p = i;
    *p = i & 0x0f;
  }

  for (;;)
  {
    io_hlt();
  }
}

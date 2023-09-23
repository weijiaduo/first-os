#include "apilib.h"

void HariMain(void)
{
    int fh;
    char c, cmdline[30];
    char *p;

    api_cmdline(cmdline, 30);
    /* 跳过之前的内容，直到遇到空格 */
    for (p = cmdline; *p > ' '; ++p) {}
    /* 跳过空格 */
    for (; *p == ' '; ++p) {}
    
    fh = api_fopen(p);
    if (fh != 0)
    {
        for (;;)
        {
            if (api_fread(&c, 1, fh) == 0)
            {
                break;
            }
            api_putchar(c);
        }
    }
    else
    {
        api_putstr0("File not found.\n");
    }
    api_end();
}

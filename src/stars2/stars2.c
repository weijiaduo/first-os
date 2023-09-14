#include "apilib.h"

int rand(void);     /* 产生0~32767之间的随机数 */

void HariMain(void)
{
    char *buf;
	int win, i, x, y;

    api_initmalloc();
    buf = api_malloc(150 * 100);
	win = api_openwin(buf, 150, 100, -1, "stars2");
	api_boxfilwin(win, 6, 26, 143, 93, 0 /* 黑色 */);

    for (i = 0; i < 50; i++)
    {
        x = (rand() % 137) + 6;
        y = (rand() % 67) + 26;
        /* win +1 是为了让窗口不刷新 */
        api_point(win + 1, x, y, 3 /* 黄色 */);
    }
    api_refreshwin(win, 6, 26, 144, 94);
    for (;;)
    {
        if (api_getkey(1) == 0x0a)
        {
            /* 按下回车键则break */
            break;
        }
    }
    api_closewin(win);
	api_end();
}

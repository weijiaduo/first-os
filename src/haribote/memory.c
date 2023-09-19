#include "bootpack.h"

#define EFLAGS_AC_BIT 0x00040000
#define CR0_CACHE_DISABLE 0x60000000

unsigned int memtest(unsigned int start, unsigned int end)
{
    char flg486 = 0;
    unsigned int eflg;
    unsigned int cr0;
    unsigned i;

    /* 确认CPU是386还是486以上 */
    eflg = io_load_eflags();
    eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
    io_store_eflags(eflg);
    eflg = io_load_eflags();
    if ((eflg & EFLAGS_AC_BIT) != 0)
    {
        /* 如果是386，即使设定AC=1，AC的值还是会自动回到0 */
        flg486 = 1;
    }
    eflg &= ~EFLAGS_AC_BIT; /* AC-bit=0 */
    io_store_eflags(eflg);

    if (flg486 != 0)
    {
        cr0 = load_cr0();
        cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */
        store_cr0(cr0);
    }

    i = memtest_sub(start, end);

    if (flg486 != 0)
    {
        cr0 = load_cr0();
        cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
        store_cr0(cr0);
    }

    return i;
}

void memman_init(struct MEMMAN *man)
{
    man->frees = 0;    /* 可用信息数目 */
    man->maxfrees = 0; /* 用于观察可用状况：frees的最大值 */
    man->lostsize = 0; /* 释放失败的内存的大小总和 */
    man->losts = 0;    /* 释放失败次数 */
    return;
}

unsigned int memman_total(struct MEMMAN *man)
{
    unsigned int i;
    unsigned int t = 0;
    for (i = 0; i < man->frees; i++)
    {
        t += man->free[i].size;
    }
    return t;
}

/** 分配内存 */
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
{
    unsigned int i;
    unsigned int a;
    for (i = 0; i < man->frees; i++)
    {
        if (man->free[i].size >= size)
        {
            /* 找到了足够大的内存 */
            a = man->free[i].addr;
            man->free[i].addr += size;
            man->free[i].size -= size;
            if (man->free[i].size == 0)
            {
                /* 如果free[i]变成了0，就删除掉这一条记录 */
                man->frees--;
                for (; i < man->frees; i++)
                {
                    man->free[i] = man->free[i + 1];
                }
            }
            return a;
        }
    }
    return 0; /* 没有可用的空间 */
}

/** 释放内存 */
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
    int i;
    int j;

    /* 为了便于归纳内存，将free[]按照addr的顺序排列 */
    /* 所以，先决定应该放在哪里 */
    for (i = 0; i < man->frees; i++)
    {
        if (man->free[i].addr > addr)
        {
            break;
        }
    }

    if (i > 0)
    {
        /* 前面有可用的内存 */
        if (man->free[i - 1].addr + man->free[i - 1].size == addr)
        {
            /* 可与前面的内存合并在一起 */
            man->free[i - 1].size += size;
            if (i < man->frees)
            {
                /* 查看后面的内存是否也可以合并 */
                if (addr + size == man->free[i].addr)
                {
                    /* 也可以和后面的可用内存合并在一起 */
                    man->free[i - 1].size += man->free[i].size;
                    /* 删除free[i] */
                    man->frees--;
                    for (; i < man->frees; i++)
                    {
                        man->free[i] = man->free[i + 1];
                    }
                }
            }
            return 0;
        }
    }

    if (i < man->frees)
    {
        /* 不能与前面的可用空间合并到一起 */
        if (addr + size == man->free[i].addr)
        {
            /* 可以与后面的内存合并在一起 */
            man->free[i].addr = addr;
            man->free[i].size += size;
            return 0;
        }
    }

    /* 既不能与前面的合并，也不能与后面的合并 */
    if (man->frees < MEMMAN_FREES)
    {
        /* free[i]之后的向后移动，腾出空间 */
        for (j = man->frees; j > i; j--)
        {
            man->free[j] = man->free[j - 1];
        }
        man->frees++;
        if (man->maxfrees < man->frees)
        {
            /* 更新最大值 */
            man->maxfrees = man->frees;
        }
        man->free[i].addr = addr;
        man->free[i].size = size;
        return 0;
    }

    /* 不能往后移动 */
    man->losts++;
    man->lostsize += size;
    return -1; /* 失败 */
}

/** 以4K为单位分配内存 */
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size)
{
    unsigned int a;
    size = (size + 0xfff) & 0xfffff000; /* 4K向上取整 */
    a = memman_alloc(man, size);
    return a;
}

/** 以4K为单位释放内存 */
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
    int i;
    size = (size + 0xfff) & 0xfffff000; /* 4K向上取整 */
    i = memman_free(man, addr, size);
    return i;
}
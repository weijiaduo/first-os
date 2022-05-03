#include "bootpack.h"

/**
 * @brief 将磁盘映像中的FTA解压缩
 * 
 * @param fat 解压后的地址
 * @param img 解压前的字符串
 */
void file_readfat(int *fat, unsigned char *img)
{
	int i, j;
	for (i = 0; i < 2880; i+= 2)
	{
		fat[i + 0] = (img[j + 0] | img[j + 1] << 8) & 0xfff;
		fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
		j += 3;
	}
	return;
}

/**
 * @brief 读取文件内容到缓冲区
 * 
 * @param clustno 簇号/扇区号
 * @param size 文件大小
 * @param buf 缓冲区
 * @param fat 解压后的FAT文件分配表
 * @param img 磁盘镜像
 */
void file_loadfile(int clustno, int size, char *buf, int *fat, char *img)
{
	int i;
	for (;;)
	{
		if (size <= 512)
		{
			for (i = 0; i < size; i++)
			{
				buf[i] = img[clustno * 512 + i];
			}
			break;
		}
		for (i = 0; i < 512; i++)
		{
			buf[i] = img[clustno * 512 + i];
		}
		size -= 512;
		buf += 512;
		clustno = fat[clustno];
	}
	return;
}

/**
 * @brief 查找指定的文件信息
 * 
 * @param name 文件名
 * @param finfo 文件目录信息
 * @param max 搜索的最大数量
 * @return struct FILEINFO* 文件信息
 */
struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max)
{
	char s[12];
	int i, j;

	/* 先设默认值，空字符串 */
	for (j = 0; j < 11; j++)
	{
		s[j] = ' ';
	}

	/* 复制文件名 */
	j = 0;
	for (i = 0; name[i] != 0; i++)
	{
		if (j >= 11)
		{
			/* 没有找到 */
			return 0;
		}
		if (name[i] == '.' && j <= 8)
		{
			j = 8;
		}
		else
		{
			s[j] = name[i];
			if ('a' <= s[j] && s[j] <= 'z')
			{
				/* 将小写字母转换成大写字母 */
				s[j] -= 0x20;
			}
			j++;
		}
	}

	/* 查找文件名 */
	for (i = 0; i < max;)
	{
		/* 文件名第一个字节为 0x00 时，表示这一段不包含任何文件名信息 */
		if (finfo[i].name[0] == 0x00)
		{
			break;
		}
		/* 文件类型判断 */
		if ((finfo[i].type & 0x18) == 0)
		{
			for (j = 0; j < 11; j++)
			{
				if (finfo[i].name[j] != s[j])
				{
					goto next;
				}
			}
			/* 找到文件名 */
			return finfo + i;
		}
next:
		i++;
	}
	return 0;
}
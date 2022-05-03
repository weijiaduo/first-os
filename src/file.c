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

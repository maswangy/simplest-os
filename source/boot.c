/*
boot.c:
Copyright (C) 2009  david leels <davidontech@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses/.
 */
#include "storage.h"
#include "fs.h"
#include "test.h"
#include "interrupt.h"
#include "mmu.h"
#include "board.h"
#include "serial.h"

typedef void (*init_func)(void);

void *kmalloc(unsigned int size);
int do_fork(int (*f)(void *),void *args);

// ���庯��ָ�����飬������ŵ�����صĳ�ʼ������
static init_func init[]=
{
		board_init,
		serial_init,
		interrupt_init,
		0,
};

void timer_init(void)
{
#define TIMER_BASE  (0xd1000000)
#define TCFG0   ( *(volatile unsigned int *)(TIMER_BASE+0x0))
#define TCFG1   ( *(volatile unsigned int *)(TIMER_BASE+0x4))
#define TCON    ( *(volatile unsigned int *)(TIMER_BASE+0x8))
#define TCONB4  ( *(volatile unsigned int *)(TIMER_BASE+0x3c))

	// һ����Ƶ:8
	TCFG0|=0x800;

	// TCFG1ʹ��Ĭ��ֵ��1/2��Ƶ���жϷ�ʽ����;

	TCON&=(~(7<<20));
	// bit[22]:1, �Զ�װ��
	TCON|=(1<<22);

	TCONB4=10000;

	// bit[21]:1, ��һ����Ҫ�ֶ�װ��TCONB4��TCNT4
	TCON|=(1<<21);
	// bit[21]:0, ����ֶ�װ��λ
	TCON&=~(1<<21);

	// bit[20]:1, ����timer4
	TCON|=(1<<20);

	umask_int(14);
	enable_irq();
}

void delay(void)
{
	volatile unsigned int time=0x1ffff;
	while(time--);
}


void plat_boot(void)
{
	//led_blink();
	int i;
	for(i=0;init[i];i++)			// ������صĳ�ʼ��
	{
		init[i]();
	}

	init_sys_mmu();

	test_mmu();						// ����mmu

	test_printk();					// ����printk

	task_init();

	timer_init();					// ��ʼ��timer0

	init_page_map();				// ��ʼ��buddy
	test_buddy();					// ��ʼ��buddy�㷨

	kmalloc_init();					// ��ʼ��kmalloc
	test_slab_kmalloc();			// ����kmalloc

	ramdisk_driver_init();			// ע��ramdisk
	test_ramdisk();					// ����ramdisk�Ķ�д



	romfs_init();					// ��ʼ��romfs
	test_romfs();					// ��romfs�е�ĳ���ļ��ж�����

	//test_exec();					// ����Ӧ�ó�����ʱ�޷�����

	test_fork();					// ��������
}

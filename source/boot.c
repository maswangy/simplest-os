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

// 定义函数指针数组，用来存放单板相关的初始化函数
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

	// 一级分频:8
	TCFG0|=0x800;

	// TCFG1使用默认值，1/2分频，中断方式工作;

	TCON&=(~(7<<20));
	// bit[22]:1, 自动装载
	TCON|=(1<<22);

	TCONB4=10000;

	// bit[21]:1, 第一次需要手动装载TCONB4到TCNT4
	TCON|=(1<<21);
	// bit[21]:0, 清除手动装载位
	TCON&=~(1<<21);

	// bit[20]:1, 启动timer4
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
	for(i=0;init[i];i++)			// 单板相关的初始化
	{
		init[i]();
	}

	init_sys_mmu();

	test_mmu();						// 测试mmu

	test_printk();					// 测试printk

	task_init();

	timer_init();					// 初始化timer0

	init_page_map();				// 初始化buddy
	test_buddy();					// 初始化buddy算法

	kmalloc_init();					// 初始化kmalloc
	test_slab_kmalloc();			// 测试kmalloc

	ramdisk_driver_init();			// 注册ramdisk
	test_ramdisk();					// 测试ramdisk的读写



	romfs_init();					// 初始化romfs
	test_romfs();					// 从romfs中的某个文件中读数据

	//test_exec();					// 调用应用程序，暂时无法返回

	test_fork();					// 创建进程
}

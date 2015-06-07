/*
interrupt.c:
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




#include "s3c2440.h"
#include "interrupt.h"

// CPSR��Ibit = 0��ʹ���ж�
void enable_irq(void)
{
	asm volatile
	(
		"mrs r4,cpsr\n\t"
		"bic r4,r4,#0x80\n\t"
		"msr cpsr,r4\n\t"
		:::"r4"
	);
}

// CPSR��Ibit = 1���ر��ж�
void disable_irq(void)
{
	asm volatile
	(
		"mrs r4,cpsr\n\t"
		"orr r4,r4,#0x80\n\t"
		"msr cpsr,r4\n\t"
		:::"r4"
	);
}

// ��offset���ж�
void umask_int(unsigned int offset)
{
	S3C24X0_INTERRUPT * const interrupt = S3C24X0_GetBase_INTERRUPT();
	interrupt->INTMSK &= ~(1<<offset);

}

// �жϴ�����
void common_irq_handler(void)
{
	S3C24X0_INTERRUPT * const interrupt = S3C24X0_GetBase_INTERRUPT();

	unsigned int tmp=(1<<(interrupt->INTOFFSET));
	printk("%d\t",interrupt->INTOFFSET);

	interrupt->SRCPND|=tmp;
	interrupt->INTPND|=tmp;

	// �ڴ����ж�ʱ��CPU���Զ���CPSR��Ibit��Fbit��1,����ֹ�жϡ�
	// ���������Ҫʵ���ж�Ƕ�ף��轫�жϴ򿪡�
	enable_irq();
	printk("interrupt occured\n");

}

void interrupt_init()
{

	S3C24X0_INTERRUPT * const interrupt = S3C24X0_GetBase_INTERRUPT();

	rGPBCON = (rGPBCON & ~0x0FF << 10) | 0x55 << 10;
	rGPBUP |= 0xF << 5;

	rGPGCON = (rGPGCON & ~(3 | 3 << 6)) | 2 | 2 << 6;	// 设置为夊部中�?
	rEXTINT1 = (rEXTINT1 & ~(7 | 7 << 12)) | 2 | 2 << 12;	// 下降沿触发模�?

	interrupt->INTMSK &= ~(1 << 5);
	interrupt->INTMSK &= ~(1 << 9);

	rEINTMASK &= ~(1 << 8 | 1 << 11);
	enable_irq();
}

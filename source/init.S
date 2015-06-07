/*
init.s:
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
along with this program.  If not, see http:@www.gnu.org/licenses/.
*/

.equ DISABLE_IRQ,		(1<<7)
.equ DISABLE_FIQ,		(1<<6)
.equ SYS_MOD,			0x1f						@ 系统模式
.equ IRQ_MOD,			0x12						@ 中断模式
.equ FIQ_MOD,			0x11						@ 快中断模式
.equ SVC_MOD,			0x13						@ 管理模式，系统上电后就进入该模式
.equ ABT_MOD,			0x17						@ 数据访问中止模式
.equ UND_MOD,			0x1b						@ 未定义指令模式

.equ MEM_SIZE,			0x800000					@ 8M
.equ TEXT_BASE,			0x30000000					@ 运行地址

.equ _SVC_STACK,		(TEXT_BASE+MEM_SIZE-4)		@ 指向最后
.equ _IRQ_STACK,		(_SVC_STACK-0x400) 			@ 0x400 = 1024,即各个模式下的栈均为1K
.equ _FIQ_STACK,		(_IRQ_STACK-0x400)
.equ _ABT_STACK,		(_FIQ_STACK-0x400)
.equ _UND_STACK,		(_ABT_STACK-0x400)
.equ _SYS_STACK,		(_UND_STACK-0x400)

.text
.code 32
.global __vector_reset

.extern plat_boot
.extern __bss_start__
.extern __bss_end__

__vector_reset:
													@ 设置各模式下的栈
    msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|SVC_MOD)	@ msr：move register to PSR
	ldr	sp,=_SVC_STACK
	msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|IRQ_MOD)
	ldr	sp,=_IRQ_STACK
	msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|FIQ_MOD)
	ldr	sp,=_FIQ_STACK
	msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|ABT_MOD)
	ldr	sp,=_ABT_STACK
	msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|UND_MOD)
	ldr	sp,=_UND_STACK
	msr cpsr_c,#(DISABLE_IRQ|DISABLE_FIQ|SYS_MOD)
	ldr	sp,=_SYS_STACK

#define pWTCON         0x53000000
#define INTMSK         0x4A000008      /* Interupt-Controller base addresses */
#define INTSUBMSK      0x4A00001C
#define CLKDIVN        0x4C000014      /* clock divisor register */
#define UPLLCON        0x4C000008      /* USB PLL controller register */
#define MPLLCON        0x4C000004      /* Main PLL controller register */


	@ turn off the watchdog
	ldr r0, =pWTCON
	mov r1, #0x0
	str r1, [r0]

	@ disable the irq
	mov r1, #0xffffffff
	ldr r0, =INTMSK
	str r1, [r0]
	ldr r1, =0x7fff
	ldr r0, =INTSUBMSK
	str r1, [r0]

	/*
	 * setup the pll configure
	 * the cpu will run at 405MHz
	 * FCLK : HCLK : PCLK = 1 : 4 : 8
	 */
	ldr r0, =CLKDIVN
	mov r1, #5
	str r1, [r0]

	mrc p15, 0, r1, c1, c0, 0
	orr r1, r1, #0xC0000000
	mcr p15, 0, r1, c1, c0, 0
	ldr r0, =UPLLCON
    ldr r1, =0x38022
    str r1, [r0]

	ldr     r0, =MPLLCON
    ldr     r1, =0x7F021
    str     r1, [r0]

	bl	cpu_init_crit


_clear_bss:											@ 清bss
	ldr r1,_bss_start_
	ldr r3,_bss_end_
	mov r2,#0x0
1:
	cmp r1,r3
	beq _main
	str r2,[r1],#0x4								@ [r1] = r2;然后r1+=0x4
	b	1b


_main:

	@ 跳转到平台启动函数
	b plat_boot

_bss_start_:
	.word   __bss_start__
_bss_end_:
	.word   __bss_end__


cpu_init_crit:
	/*
	 * flush v4 I/D caches
	 */
	mov r0, #0
	mcr p15, 0, r0, c7, c7, 0	/* flush v3/v4 cache */
	mcr p15, 0, r0, c8, c7, 0	/* flush v4 TLB */

	/*
	 * disable MMU stuff and caches
	 */
	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #0x00002300	@ clear bits 13, 9:8 (--V- --RS)
	bic r0, r0, #0x00000087	@ clear bits 7, 2:0 (B--- -CAM)
	orr r0, r0, #0x00000002	@ set bit 2 (A) Align
	orr r0, r0, #0x00001000	@ set bit 12 (I) I-Cache
	mcr p15, 0, r0, c1, c0, 0

	mov ip, lr

//	bl lowlevel_init

	mov lr, ip
	mov pc, lr

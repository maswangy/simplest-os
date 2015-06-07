/*
abnormal.s:
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

.equ DISABLE_IRQ,0x80
.equ DISABLE_FIQ,0x40
.equ SYS_MOD,0x1f
.equ IRQ_MOD,0x12
.equ FIQ_MOD,0x11
.equ SVC_MOD,0x13
.equ ABT_MOD,0x17
.equ UND_MOD,0x1b
.equ MOD_MASK,0x1f


@ 切换到管理模式
.macro CHANGE_TO_SVC
	msr     cpsr_c,#(DISABLE_FIQ|DISABLE_IRQ|SVC_MOD)
.endm

@ 切换到中断模式
.macro CHANGE_TO_IRQ
	msr     cpsr_c,#(DISABLE_FIQ|DISABLE_IRQ|IRQ_MOD)
.endm

@ 切换到系统模式
.macro CHANGE_TO_SYS
	msr     cpsr_c,#(DISABLE_FIQ|DISABLE_IRQ|SYS_MOD)
.endm



.global	__vector_undefined
.global	__vector_swi
.global	__vector_prefetch_abort
.global	__vector_data_abort
.global	__vector_reserved
.global	__vector_irq
.global	__vector_fiq

.text
.code 32


@ 未定义指令异常
__vector_undefined:
	nop


@ 软中断异常
__vector_swi:

	@ r0
	@ SPSR_svc
	@ LR_svc
	str	r14,[r13,#-0xc]				@ 保存中断的返回地址LR_svc
	mrs	r14,spsr
	str	r14,[r13,#-0x8]				@ 保存中断的状态寄存器SPSR_svc
	str r0,[r13,#-0x4]				@ 利用r0传递SP_svc，所以要保存起来
	mov	r0,r13


	CHANGE_TO_SYS					@ 切换到sys模式

	@ LR_svc
	@ LR_sys
	@ R0
	@ R1
	@ R2
	@ R3
	@ SPSR_svc						SP
	str	r14,[r13,#-8]!				@ 保存LR_sys,因为调用子函数时需要用到LR_sys

	ldr	r14,[r0,#-0xc]
	str	r14,[r13,#4]				@ 保存中断返回地址

	ldr r14,[r0,#-0x8]				@ LR_sys = SPSR_svc
	ldr r0,[r0,#-0x4]				@ 恢复r0

	stmfd r13!,{r0-r3,r14}			@ 保存SPSR_svc，r0~r3到栈中

	ldr r3,[r13,#24]
	ldr r0,[r3,#-4]					@ 取到SWI指令

	bic r0,r0,#0xff000000			@ r0/r1/r2是sys_call_schedule()的参数:系统调用号，参数地址，参数个数
	ldr r1,[r13,#32]
	ldr r2,[r13,#36]

	bl	sys_call_schedule

	str r0,[r13,#28]

	ldmfd r13!,{r0-r3}				@ 恢复r0~r3
	ldmfd r13!,{r14}				@ 恢复CPSR
	msr cpsr,r14
	ldmfd r13!,{r14,pc}				@ 恢复LR_sys和LR_svc


@ 预取址中止异常
__vector_prefetch_abort:	
	nop


@ 数据中止异常
__vector_data_abort:
	nop

@ 保留
__vector_reserved:
	nop

@ 中断异常
__vector_irq:
	sub r14,r14,#4
	stmfd r13!,{r0}
	stmfd r13!,{r1-r3}

	@ 清中断
	mov r2,#0xca000000
	add r1,r2,#0x10
	ldr r0,[r1]
	ldr r3,[r2]
	orr r3,r3,r1
	str r3,[r2]
	str r0,[r1]

	ldmfd r13!,{r1-r3}
	mov r0,r14							@ 利用r0传递LR_irq

	CHANGE_TO_SYS

	stmfd r13!,{r0}						@ push LR_irq
	stmfd r13!,{r14}					@ push LR_sys

	CHANGE_TO_IRQ

	ldmfd r13!,{r0}						@ pop r0

	ldr r14,=__asm_schedule				@ jump to __asm_schedule
	stmfd r13!,{r14}
	ldmfd r13!,{pc}^

__asm_schedule:
	stmfd r13!,{r0-r12}
	mrs	r1, cpsr
	stmfd r13!,{r1}

	mov	r1,sp
	bic	r1,#0xff0
	bic r1,#0xf
	mov r0,sp
	str r0,[r1]

	bl __common_schedule
	ldr sp,[r0]
	ldmfd r13!,{r1}
	msr cpsr_cxsf,r1
	ldmfd r13!,{r0-r12,r14,pc}
	

@ 快中断异常
__vector_fiq:
	nop

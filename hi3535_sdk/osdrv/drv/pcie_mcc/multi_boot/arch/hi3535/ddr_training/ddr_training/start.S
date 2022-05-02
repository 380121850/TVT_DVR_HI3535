@/******************************************************************************
@ *    Copyright (c) 2009-2012 by Hisi.
@ *    All rights reserved.
@ * ***
@******************************************************************************/
#include "config.h"

.section .text
.type _start, %function
.globl _start
_start:
	push	{r0 - r10}
	b	flying

. = 0x20
_TEXT_BASE:
	.word TEXT_BASE

_flying:
	.word flying
_STACK_POINT:
	.word STACK_POINT

.globl _bss_start
_bss_start: .word __bss_start
.globl _bss_end
_bss_end:   .word _end

flying:
	@set the cpu to SVC32 mode
	mrs     r0, cpsr
	bic     r0, r0, #0x1f
	orr     r0, r0, #0xd3
	msr     cpsr,r0

	@Invalidate L1 I/D
	mov     r0, #0
	mcr     p15, 0, r0, c8, c7, 0
	mcr     p15, 0, r0, c7, c5, 0

	@Invalidate L1 D-cache
	mcr     p15, 2, r0, c0, c0, 0
	mrc     p15, 1, r3, c0, c0, 0

	ldr     r1, =0x1ff
	and     r3, r1, r3, LSR #13
	mov     r0, #0

way_loop:
	mov     r1, #0
line_loop:
	mov     r2, r0, LSL #30
	orr     r2, r1, LSL #5
	mcr     p15, 0, r2, c7, c6, 2
	add     r1, r1, #1
	cmp     r1, r3
	ble     line_loop
	add     r0, r0, #1
	cmp     r0, #4
	blt     way_loop

	@disable MMU stuff and caches
	mrc     p15, 0, r0, c1, c0, 0
	bic     r0, r0, #0x00002000
	bic     r0, r0, #0x00000007
	orr     r0, r0, #0x00000002
	orr     r0, r0, #0x00000800
	mcr     p15, 0, r0, c1, c0, 0

	@check cpuid
	mrc     p15, 0, r0, c0, c0, 5
	and     r0, r0, #0xf
	cmp     r0, #0
	bne     core_x_flow
	b       main_core
core_x_flow:
	ldr     r3, =REG_BASE_SYSCTRL
	ldr     r0, [r3, #0x134]
	cmp     r0, #0
	beq     core_x_flow
main_core:
	@reset slave cpu
	ldr     r4, =CRG_REG_BASE
	ldr     r0, [r4, #0x28]
	orr     r0, r0, #(1<<4)
	orr     r0, r0, #(1<<5)
	orr     r0, r0, #(1<<6)
	str     r0, [r4, #0x28]

	ldr	r0, _bss_start
	ldr	r1, _bss_end
	mov	r2, #0x00000000

clear_bss_loop:
	str	r2, [r0]
	cmp	r0, r1
	add	r0, r0, #4
	bne	clear_bss_loop

	ldr	r4, =_lr
	str	lr, [r4]
	pop	{r0 - r10}

	ldr	lr, =_sp
	str	sp, [lr]
	ldr	sp, _STACK_POINT

	@ldr	r3, =0x30000000
	@ldr	r1, =0x12345678
	@str	r1, [r3, #0x0]

	ldr	r0, =REG_BASE_SYSCTRL
	bl	start_ddr_training

	@info host that ddr training is done
	ldr	r3, =REG_BASE_SYSCTRL
	ldr	r1, =0x8080
	str	r1, [r3, #0x154]

jump2ddr:
	ldr	r1, [r3, #0x154]
	cmp	r1, #0
	bne	jump2ddr
	mov	pc, r4

.section .data
_lr:	.word 0
_sp:	.word 0

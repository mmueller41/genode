/*
 * \brief   Startup code for bootstrap
 * \author  Stefan Kalkowski
 * \date    2016-09-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

.set STACK_SIZE, 4 * 16 * 1024

.section ".text.crt0"

	.global _start
	_start:


	/***************************
	 ** Zero-fill BSS segment **
	 ***************************/

	adr r0, _bss_local_start
	adr r1, _bss_local_end
	ldr r0, [r0]
	ldr r1, [r1]
	mov r2, #0
	1:
	cmp r1, r0
	ble 2f
	str r2, [r0]
	add r0, r0, #4
	b   1b
	2:


	/******************************
	 ** Disable alignment checks **
	 ******************************/

	mrc p15, 0, r0, c1, c0, 0
	bic r0, r0, #2              /* clear A bit */
	mcr p15, 0, r0, c1, c0, 0


	/*****************************************************
	 ** Setup multiprocessor-aware kernel stack-pointer **
	 *****************************************************/

	mov sp, #0                  /* for boot cpu use id 0    */
	bl _start_setup_stack
	_boot_cpu_lr:
	nop

	.global _start_setup_stack  /* entrypoint for all cpus */
	_start_setup_stack:

	adr r0, _boot_cpu_lr
	cmp r0, lr                  /* check for boot cpu */
	mrcne p15, 0, sp, c0, c0, 5 /* read multiprocessor affinity register */
	andne sp, sp, #0xff         /* set cpu id for non-boot cpu */

	adr r0, _bootstrap_stack_local      /* load stack address into r0 */
	adr r1, _bootstrap_stack_size_local /* load stack size per cpu into r1 */
	ldr r0, [r0]
	ldr r1, [r1]
	ldr r1, [r1]

	add sp, #1                  /* calculate stack start for CPU */
	mul r1, r1, sp
	add sp, r0, r1


	/****************
	 ** Enable VFP **
	 ****************/

	mov r0, #0xf
	lsl r0, #20
	mcr p15, 0, r0, c1, c0, 2   /* write to CPACR to enable VFP access */
	mcr p15, 0, r0, c7, c5, 4   /* deprecated ISB instruction <= ARMv6 */

	vmrs r0, fpexc              /* enable the VFP by read/write fpexc  */
	mov  r1, #1                 /* enable bit 30                       */
	lsl  r1, #30
	orr  r0, r1
	vmsr fpexc, r0


	/************************************
	 ** Jump to high-level entry point **
	 ************************************/

	b init


	_bss_local_start:
	.long _bss_start

	_bss_local_end:
	.long _bss_end

	_bootstrap_stack_local:
	.long bootstrap_stack

	_bootstrap_stack_size_local:
	.long bootstrap_stack_size

/*
 *  linux/arch/arm/mm/proc-v6.S
 *
 *  Copyright (C) 2001 Deep Blue Solutions Ltd.
 *  Modified by Catalin Marinas for noMMU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This is the "shell" of the ARMv6 processor support.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/elf.h>
#include <asm/pgtable-hwdef.h>
#include <asm/pgtable.h>

#ifdef CONFIG_SMP
#include <asm/hardware/arm_scu.h>
#endif

#include "proc-macros.S"

#define D_CACHE_LINE_SIZE	32

#define TTB_C		(1 << 0)
#define TTB_S		(1 << 1)
#define TTB_IMP		(1 << 2)
#define TTB_RGN_NC	(0 << 3)
#define TTB_RGN_WBWA	(1 << 3)
#define TTB_RGN_WT	(2 << 3)
#define TTB_RGN_WB	(3 << 3)

#ifndef CONFIG_SMP
#define TTB_FLAGS	TTB_RGN_WBWA
#else
#define TTB_FLAGS	TTB_RGN_WBWA|TTB_S
#endif

ENTRY(cpu_v6_proc_init)
	mov	pc, lr

ENTRY(cpu_v6_proc_fin)
	stmfd	sp!, {lr}
	cpsid	if				@ disable interrupts
	bl	v6_flush_kern_cache_all
	mrc	p15, 0, r0, c1, c0, 0		@ ctrl register
	bic	r0, r0, #0x1000			@ ...i............
	bic	r0, r0, #0x0006			@ .............ca.
	mcr	p15, 0, r0, c1, c0, 0		@ disable caches
	ldmfd	sp!, {pc}

/*
 *	cpu_v6_reset(loc)
 *
 *	Perform a soft reset of the system.  Put the CPU into the
 *	same state as it would be if it had been reset, and branch
 *	to what would be the reset vector.
 *
 *	- loc   - location to jump to for soft reset
 *
 *	It is assumed that:
 */
	.align	5
ENTRY(cpu_v6_reset)
	mov	pc, r0

/*
 *	cpu_v6_do_idle()
 *
 *	Idle the processor (eg, wait for interrupt).
 *
 *	IRQs are already disabled.
 */
ENTRY(cpu_v6_do_idle)
	mcr	p15, 0, r1, c7, c0, 4		@ wait for interrupt
	mov	pc, lr

ENTRY(cpu_v6_dcache_clean_area)
#ifndef TLB_CAN_READ_FROM_L1_CACHE
1:	mcr	p15, 0, r0, c7, c10, 1		@ clean D entry
	add	r0, r0, #D_CACHE_LINE_SIZE
	subs	r1, r1, #D_CACHE_LINE_SIZE
	bhi	1b
#endif
	mov	pc, lr

/*
 *	cpu_arm926_switch_mm(pgd_phys, tsk)
 *
 *	Set the translation table base pointer to be pgd_phys
 *
 *	- pgd_phys - physical address of new TTB
 *
 *	It is assumed that:
 *	- we are not using split page tables
 */
ENTRY(cpu_v6_switch_mm)
#ifdef CONFIG_MMU
	mov	r2, #0
	ldr	r1, [r1, #MM_CONTEXT_ID]	@ get mm->context.id
	orr	r0, r0, #TTB_FLAGS
	mcr	p15, 0, r2, c7, c5, 6		@ flush BTAC/BTB
	mcr	p15, 0, r2, c7, c10, 4		@ drain write buffer
	mcr	p15, 0, r0, c2, c0, 0		@ set TTB 0
	mcr	p15, 0, r1, c13, c0, 1		@ set context ID
#endif
	mov	pc, lr

/*
 *	cpu_v6_set_pte_ext(ptep, pte, ext)
 *
 *	Set a level 2 translation table entry.
 *
 *	- ptep  - pointer to level 2 translation table entry
 *		  (hardware version is stored at -1024 bytes)
 *	- pte   - PTE value to store
 *	- ext	- value for extended PTE bits
 *
 *	Permissions:
 *	  YUWD  APX AP1 AP0	SVC	User
 *	  0xxx   0   0   0	no acc	no acc
 *	  100x   1   0   1	r/o	no acc
 *	  10x0   1   0   1	r/o	no acc
 *	  1011   0   0   1	r/w	no acc
 *	  110x   0   1   0	r/w	r/o
 *	  11x0   0   1   0	r/w	r/o
 *	  1111   0   1   1	r/w	r/w
 */
ENTRY(cpu_v6_set_pte_ext)
#ifdef CONFIG_MMU
	str	r1, [r0], #-2048		@ linux version

	bic	r3, r1, #0x000003f0
	bic	r3, r3, #0x00000003
	orr	r3, r3, r2
	orr	r3, r3, #PTE_EXT_AP0 | 2

	tst	r1, #L_PTE_WRITE
	tstne	r1, #L_PTE_DIRTY
	orreq	r3, r3, #PTE_EXT_APX

	tst	r1, #L_PTE_USER
	orrne	r3, r3, #PTE_EXT_AP1
	tstne	r3, #PTE_EXT_APX
	bicne	r3, r3, #PTE_EXT_APX | PTE_EXT_AP0

	tst	r1, #L_PTE_YOUNG
	biceq	r3, r3, #PTE_EXT_APX | PTE_EXT_AP_MASK

	tst	r1, #L_PTE_EXEC
	orreq	r3, r3, #PTE_EXT_XN

	tst	r1, #L_PTE_PRESENT
	moveq	r3, #0

	str	r3, [r0]
	mcr	p15, 0, r0, c7, c10, 1 @ flush_pte
#endif
	mov	pc, lr




cpu_v6_name:
	.asciz	"ARMv6-compatible processor"
	.align

	.section ".text.init", #alloc, #execinstr

/*
 *	__v6_setup
 *
 *	Initialise TLB, Caches, and MMU state ready to switch the MMU
 *	on.  Return in r0 the new CP15 C1 control register setting.
 *
 *	We automatically detect if we have a Harvard cache, and use the
 *	Harvard cache control instructions insead of the unified cache
 *	control instructions.
 *
 *	This should be able to cover all ARMv6 cores.
 *
 *	It is assumed that:
 *	- cache type register is implemented
 */
__v6_setup:
#ifdef CONFIG_SMP
	/* Set up the SCU on core 0 only */
	mrc	p15, 0, r0, c0, c0, 5		@ CPU core number
	ands	r0, r0, #15
	ldreq	r0, =SCU_BASE
	ldreq	r5, [r0, #SCU_CTRL]
	orreq	r5, r5, #1
	streq	r5, [r0, #SCU_CTRL]

#ifndef CONFIG_CPU_DCACHE_DISABLE
	mrc	p15, 0, r0, c1, c0, 1		@ Enable SMP/nAMP mode
	orr	r0, r0, #0x20
	mcr	p15, 0, r0, c1, c0, 1
#endif
#endif

	mov	r0, #0
	mcr	p15, 0, r0, c7, c14, 0		@ clean+invalidate D cache
	mcr	p15, 0, r0, c7, c5, 0		@ invalidate I cache
	mcr	p15, 0, r0, c7, c15, 0		@ clean+invalidate cache
	mcr	p15, 0, r0, c7, c10, 4		@ drain write buffer
#ifdef CONFIG_MMU
	mcr	p15, 0, r0, c8, c7, 0		@ invalidate I + D TLBs
	mcr	p15, 0, r0, c2, c0, 2		@ TTB control register
	orr	r4, r4, #TTB_FLAGS
	mcr	p15, 0, r4, c2, c0, 1		@ load TTB1
#endif /* CONFIG_MMU */
	adr	r5, v6_crval
	ldmia	r5, {r5, r6}
	mrc	p15, 0, r0, c1, c0, 0		@ read control register
	bic	r0, r0, r5			@ clear bits them
	orr	r0, r0, r6			@ set them
	mov	pc, lr				@ return to head.S:__ret

	/*
	 *         V X F   I D LR
	 * .... ...E PUI. .T.T 4RVI ZFRS BLDP WCAM
	 * rrrr rrrx xxx0 0101 xxxx xxxx x111 xxxx < forced
	 *         0 110       0011 1.00 .111 1101 < we want
	 */
	.type	v6_crval, #object
v6_crval:
	crval	clear=0x01e0fb7f, mmuset=0x00c0387d, ucset=0x00c0187c

	.type	v6_processor_functions, #object
ENTRY(v6_processor_functions)
	.word	v6_early_abort
	.word	cpu_v6_proc_init
	.word	cpu_v6_proc_fin
	.word	cpu_v6_reset
	.word	cpu_v6_do_idle
	.word	cpu_v6_dcache_clean_area
	.word	cpu_v6_switch_mm
	.word	cpu_v6_set_pte_ext
	.size	v6_processor_functions, . - v6_processor_functions

	.type	cpu_arch_name, #object
cpu_arch_name:
	.asciz	"armv6"
	.size	cpu_arch_name, . - cpu_arch_name

	.type	cpu_elf_name, #object
cpu_elf_name:
	.asciz	"v6"
	.size	cpu_elf_name, . - cpu_elf_name
	.align

	.section ".proc.info.init", #alloc, #execinstr

	/*
	 * Match any ARMv6 processor core.
	 */
	.type	__v6_proc_info, #object
__v6_proc_info:
	.long	0x0007b000
	.long	0x0007f000
	.long   PMD_TYPE_SECT | \
		PMD_SECT_BUFFERABLE | \
		PMD_SECT_CACHEABLE | \
		PMD_SECT_AP_WRITE | \
		PMD_SECT_AP_READ
	.long   PMD_TYPE_SECT | \
		PMD_SECT_XN | \
		PMD_SECT_AP_WRITE | \
		PMD_SECT_AP_READ
	b	__v6_setup
	.long	cpu_arch_name
	.long	cpu_elf_name
	.long	HWCAP_SWP|HWCAP_HALF|HWCAP_THUMB|HWCAP_FAST_MULT|HWCAP_EDSP|HWCAP_JAVA
	.long	cpu_v6_name
	.long	v6_processor_functions
	.long	v6wbi_tlb_fns
	.long	v6_user_fns
	.long	v6_cache_fns
	.size	__v6_proc_info, . - __v6_proc_info

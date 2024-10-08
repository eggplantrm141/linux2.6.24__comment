/*
 * include/asm-arm/arch-s3c2410/entry-macro.S
 *
 * Low-level IRQ helper macros for S3C2410-based platforms
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
*/

/* We have a problem that the INTOFFSET register does not always
 * show one interrupt. Occasionally we get two interrupts through
 * the prioritiser, and this causes the INTOFFSET register to show
 * what looks like the logical-or of the two interrupt numbers.
 *
 * Thanks to Klaus, Shannon, et al for helping to debug this problem
*/

#define INTPND		(0x10)
#define INTOFFSET	(0x14)

#include <asm/hardware.h>
#include <asm/irq.h>

	.macro  get_irqnr_preamble, base, tmp
	.endm

	.macro  arch_ret_to_user, tmp1, tmp2
	.endm

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp

		mov	\base, #S3C24XX_VA_IRQ

		@@ try the interrupt offset register, since it is there

		ldr	\irqstat, [ \base, #INTPND ]
		teq	\irqstat, #0
		beq	1002f
		ldr	\irqnr, [ \base, #INTOFFSET ]
		mov	\tmp, #1
		tst	\irqstat, \tmp, lsl \irqnr
		bne	1001f

		@@ the number specified is not a valid irq, so try
		@@ and work it out for ourselves

		mov	\irqnr, #0		@@ start here

		@@ work out which irq (if any) we got

		movs	\tmp, \irqstat, lsl#16
		addeq	\irqnr, \irqnr, #16
		moveq	\irqstat, \irqstat, lsr#16
		tst	\irqstat, #0xff
		addeq	\irqnr, \irqnr, #8
		moveq	\irqstat, \irqstat, lsr#8
		tst	\irqstat, #0xf
		addeq	\irqnr, \irqnr, #4
		moveq	\irqstat, \irqstat, lsr#4
		tst	\irqstat, #0x3
		addeq	\irqnr, \irqnr, #2
		moveq	\irqstat, \irqstat, lsr#2
		tst	\irqstat, #0x1
		addeq	\irqnr, \irqnr, #1

		@@ we have the value
1001:
		adds	\irqnr, \irqnr, #IRQ_EINT0
1002:
		@@ exit here, Z flag unset if IRQ

	.endm

		/* currently don't need an disable_fiq macro */

		.macro	disable_fiq
		.endm

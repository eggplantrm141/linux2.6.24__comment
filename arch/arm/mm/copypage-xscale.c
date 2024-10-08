/*
 *  linux/arch/arm/lib/copypage-xscale.S
 *
 *  Copyright (C) 1995-2005 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This handles the mini data cache, as found on SA11x0 and XScale
 * processors.  When we copy a user page page, we map it in such a way
 * that accesses to this page will not touch the main data cache, but
 * will be cached in the mini data cache.  This prevents us thrashing
 * the main data cache on page faults.
 */
#include <linux/init.h>
#include <linux/mm.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>

#include "mm.h"

/*
 * 0xffff8000 to 0xffffffff is reserved for any ARM architecture
 * specific hacks for copying pages efficiently.
 */
#define COPYPAGE_MINICACHE	0xffff8000

#define minicache_pgprot __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | \
				  L_PTE_CACHEABLE)

static DEFINE_SPINLOCK(minicache_lock);

/*
 * XScale mini-dcache optimised copy_user_page
 *
 * We flush the destination cache lines just before we write the data into the
 * corresponding address.  Since the Dcache is read-allocate, this removes the
 * Dcache aliasing issue.  The writes will be forwarded to the write buffer,
 * and merged as appropriate.
 */
static void __attribute__((naked))
mc_copy_user_page(void *from, void *to)
{
	/*
	 * Strangely enough, best performance is achieved
	 * when prefetching destination as well.  (NP)
	 */
	asm volatile(
	"stmfd	sp!, {r4, r5, lr}		\n\
	mov	lr, %2				\n\
	pld	[r0, #0]			\n\
	pld	[r0, #32]			\n\
	pld	[r1, #0]			\n\
	pld	[r1, #32]			\n\
1:	pld	[r0, #64]			\n\
	pld	[r0, #96]			\n\
	pld	[r1, #64]			\n\
	pld	[r1, #96]			\n\
2:	ldrd	r2, [r0], #8			\n\
	ldrd	r4, [r0], #8			\n\
	mov	ip, r1				\n\
	strd	r2, [r1], #8			\n\
	ldrd	r2, [r0], #8			\n\
	strd	r4, [r1], #8			\n\
	ldrd	r4, [r0], #8			\n\
	strd	r2, [r1], #8			\n\
	strd	r4, [r1], #8			\n\
	mcr	p15, 0, ip, c7, c10, 1		@ clean D line\n\
	ldrd	r2, [r0], #8			\n\
	mcr	p15, 0, ip, c7, c6, 1		@ invalidate D line\n\
	ldrd	r4, [r0], #8			\n\
	mov	ip, r1				\n\
	strd	r2, [r1], #8			\n\
	ldrd	r2, [r0], #8			\n\
	strd	r4, [r1], #8			\n\
	ldrd	r4, [r0], #8			\n\
	strd	r2, [r1], #8			\n\
	strd	r4, [r1], #8			\n\
	mcr	p15, 0, ip, c7, c10, 1		@ clean D line\n\
	subs	lr, lr, #1			\n\
	mcr	p15, 0, ip, c7, c6, 1		@ invalidate D line\n\
	bgt	1b				\n\
	beq	2b				\n\
	ldmfd	sp!, {r4, r5, pc}		"
	:
	: "r" (from), "r" (to), "I" (PAGE_SIZE / 64 - 1));
}

void xscale_mc_copy_user_page(void *kto, const void *kfrom, unsigned long vaddr)
{
	struct page *page = virt_to_page(kfrom);

	if (test_and_clear_bit(PG_dcache_dirty, &page->flags))
		__flush_dcache_page(page_mapping(page), page);

	spin_lock(&minicache_lock);

	set_pte_ext(TOP_PTE(COPYPAGE_MINICACHE), pfn_pte(__pa(kfrom) >> PAGE_SHIFT, minicache_pgprot), 0);
	flush_tlb_kernel_page(COPYPAGE_MINICACHE);

	mc_copy_user_page((void *)COPYPAGE_MINICACHE, kto);

	spin_unlock(&minicache_lock);
}

/*
 * XScale optimised clear_user_page
 */
void __attribute__((naked))
xscale_mc_clear_user_page(void *kaddr, unsigned long vaddr)
{
	asm volatile(
	"mov	r1, %0				\n\
	mov	r2, #0				\n\
	mov	r3, #0				\n\
1:	mov	ip, r0				\n\
	strd	r2, [r0], #8			\n\
	strd	r2, [r0], #8			\n\
	strd	r2, [r0], #8			\n\
	strd	r2, [r0], #8			\n\
	mcr	p15, 0, ip, c7, c10, 1		@ clean D line\n\
	subs	r1, r1, #1			\n\
	mcr	p15, 0, ip, c7, c6, 1		@ invalidate D line\n\
	bne	1b				\n\
	mov	pc, lr"
	:
	: "I" (PAGE_SIZE / 32));
}

struct cpu_user_fns xscale_mc_user_fns __initdata = {
	.cpu_clear_user_page	= xscale_mc_clear_user_page, 
	.cpu_copy_user_page	= xscale_mc_copy_user_page,
};

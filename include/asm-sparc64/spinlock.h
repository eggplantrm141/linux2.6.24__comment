/* spinlock.h: 64-bit Sparc spinlock support.
 *
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC64_SPINLOCK_H
#define __SPARC64_SPINLOCK_H

#include <linux/threads.h>	/* For NR_CPUS */

#ifndef __ASSEMBLY__

/* To get debugging spinlocks which detect and catch
 * deadlock situations, set CONFIG_DEBUG_SPINLOCK
 * and rebuild your kernel.
 */

/* All of these locking primitives are expected to work properly
 * even in an RMO memory model, which currently is what the kernel
 * runs in.
 *
 * There is another issue.  Because we play games to save cycles
 * in the non-contention case, we need to be extra careful about
 * branch targets into the "spinning" code.  They live in their
 * own section, but the newer V9 branches have a shorter range
 * than the traditional 32-bit sparc branch variants.  The rule
 * is that the branches that go into and out of the spinner sections
 * must be pre-V9 branches.
 */

#define __raw_spin_is_locked(lp)	((lp)->lock != 0)

#define __raw_spin_unlock_wait(lp)	\
	do {	rmb();			\
	} while((lp)->lock)

static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldstub		[%1], %0\n"
"	membar		#StoreLoad | #StoreStore\n"
"	brnz,pn		%0, 2f\n"
"	 nop\n"
"	.subsection	2\n"
"2:	ldub		[%1], %0\n"
"	membar		#LoadLoad\n"
"	brnz,pt		%0, 2b\n"
"	 nop\n"
"	ba,a,pt		%%xcc, 1b\n"
"	.previous"
	: "=&r" (tmp)
	: "r" (lock)
	: "memory");
}

static inline int __raw_spin_trylock(raw_spinlock_t *lock)
{
	unsigned long result;

	__asm__ __volatile__(
"	ldstub		[%1], %0\n"
"	membar		#StoreLoad | #StoreStore"
	: "=r" (result)
	: "r" (lock)
	: "memory");

	return (result == 0UL);
}

static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
	__asm__ __volatile__(
"	membar		#StoreStore | #LoadStore\n"
"	stb		%%g0, [%0]"
	: /* No outputs */
	: "r" (lock)
	: "memory");
}

static inline void __raw_spin_lock_flags(raw_spinlock_t *lock, unsigned long flags)
{
	unsigned long tmp1, tmp2;

	__asm__ __volatile__(
"1:	ldstub		[%2], %0\n"
"	membar		#StoreLoad | #StoreStore\n"
"	brnz,pn		%0, 2f\n"
"	 nop\n"
"	.subsection	2\n"
"2:	rdpr		%%pil, %1\n"
"	wrpr		%3, %%pil\n"
"3:	ldub		[%2], %0\n"
"	membar		#LoadLoad\n"
"	brnz,pt		%0, 3b\n"
"	 nop\n"
"	ba,pt		%%xcc, 1b\n"
"	 wrpr		%1, %%pil\n"
"	.previous"
	: "=&r" (tmp1), "=&r" (tmp2)
	: "r"(lock), "r"(flags)
	: "memory");
}

/* Multi-reader locks, these are much saner than the 32-bit Sparc ones... */

static void inline __read_lock(raw_rwlock_t *lock)
{
	unsigned long tmp1, tmp2;

	__asm__ __volatile__ (
"1:	ldsw		[%2], %0\n"
"	brlz,pn		%0, 2f\n"
"4:	 add		%0, 1, %1\n"
"	cas		[%2], %0, %1\n"
"	cmp		%0, %1\n"
"	membar		#StoreLoad | #StoreStore\n"
"	bne,pn		%%icc, 1b\n"
"	 nop\n"
"	.subsection	2\n"
"2:	ldsw		[%2], %0\n"
"	membar		#LoadLoad\n"
"	brlz,pt		%0, 2b\n"
"	 nop\n"
"	ba,a,pt		%%xcc, 4b\n"
"	.previous"
	: "=&r" (tmp1), "=&r" (tmp2)
	: "r" (lock)
	: "memory");
}

static int inline __read_trylock(raw_rwlock_t *lock)
{
	int tmp1, tmp2;

	__asm__ __volatile__ (
"1:	ldsw		[%2], %0\n"
"	brlz,a,pn	%0, 2f\n"
"	 mov		0, %0\n"
"	add		%0, 1, %1\n"
"	cas		[%2], %0, %1\n"
"	cmp		%0, %1\n"
"	membar		#StoreLoad | #StoreStore\n"
"	bne,pn		%%icc, 1b\n"
"	 mov		1, %0\n"
"2:"
	: "=&r" (tmp1), "=&r" (tmp2)
	: "r" (lock)
	: "memory");

	return tmp1;
}

static void inline __read_unlock(raw_rwlock_t *lock)
{
	unsigned long tmp1, tmp2;

	__asm__ __volatile__(
"	membar	#StoreLoad | #LoadLoad\n"
"1:	lduw	[%2], %0\n"
"	sub	%0, 1, %1\n"
"	cas	[%2], %0, %1\n"
"	cmp	%0, %1\n"
"	bne,pn	%%xcc, 1b\n"
"	 nop"
	: "=&r" (tmp1), "=&r" (tmp2)
	: "r" (lock)
	: "memory");
}

static void inline __write_lock(raw_rwlock_t *lock)
{
	unsigned long mask, tmp1, tmp2;

	mask = 0x80000000UL;

	__asm__ __volatile__(
"1:	lduw		[%2], %0\n"
"	brnz,pn		%0, 2f\n"
"4:	 or		%0, %3, %1\n"
"	cas		[%2], %0, %1\n"
"	cmp		%0, %1\n"
"	membar		#StoreLoad | #StoreStore\n"
"	bne,pn		%%icc, 1b\n"
"	 nop\n"
"	.subsection	2\n"
"2:	lduw		[%2], %0\n"
"	membar		#LoadLoad\n"
"	brnz,pt		%0, 2b\n"
"	 nop\n"
"	ba,a,pt		%%xcc, 4b\n"
"	.previous"
	: "=&r" (tmp1), "=&r" (tmp2)
	: "r" (lock), "r" (mask)
	: "memory");
}

static void inline __write_unlock(raw_rwlock_t *lock)
{
	__asm__ __volatile__(
"	membar		#LoadStore | #StoreStore\n"
"	stw		%%g0, [%0]"
	: /* no outputs */
	: "r" (lock)
	: "memory");
}

static int inline __write_trylock(raw_rwlock_t *lock)
{
	unsigned long mask, tmp1, tmp2, result;

	mask = 0x80000000UL;

	__asm__ __volatile__(
"	mov		0, %2\n"
"1:	lduw		[%3], %0\n"
"	brnz,pn		%0, 2f\n"
"	 or		%0, %4, %1\n"
"	cas		[%3], %0, %1\n"
"	cmp		%0, %1\n"
"	membar		#StoreLoad | #StoreStore\n"
"	bne,pn		%%icc, 1b\n"
"	 nop\n"
"	mov		1, %2\n"
"2:"
	: "=&r" (tmp1), "=&r" (tmp2), "=&r" (result)
	: "r" (lock), "r" (mask)
	: "memory");

	return result;
}

#define __raw_read_lock(p)	__read_lock(p)
#define __raw_read_trylock(p)	__read_trylock(p)
#define __raw_read_unlock(p)	__read_unlock(p)
#define __raw_write_lock(p)	__write_lock(p)
#define __raw_write_unlock(p)	__write_unlock(p)
#define __raw_write_trylock(p)	__write_trylock(p)

#define __raw_read_can_lock(rw)		(!((rw)->lock & 0x80000000UL))
#define __raw_write_can_lock(rw)	(!(rw)->lock)

#define _raw_spin_relax(lock)	cpu_relax()
#define _raw_read_relax(lock)	cpu_relax()
#define _raw_write_relax(lock)	cpu_relax()

#endif /* !(__ASSEMBLY__) */

#endif /* !(__SPARC64_SPINLOCK_H) */

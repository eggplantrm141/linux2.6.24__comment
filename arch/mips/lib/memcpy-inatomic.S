/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Unified implementation of memcpy, memmove and the __copy_user backend.
 *
 * Copyright (C) 1998, 99, 2000, 01, 2002 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 1999, 2000, 01, 2002 Silicon Graphics, Inc.
 * Copyright (C) 2002 Broadcom, Inc.
 *   memcpy/copy_user author: Mark Vandevoorde
 *
 * Mnemonic names for arguments to memcpy/__copy_user
 */

/*
 * Hack to resolve longstanding prefetch issue
 *
 * Prefetching may be fatal on some systems if we're prefetching beyond the
 * end of memory on some systems.  It's also a seriously bad idea on non
 * dma-coherent systems.
 */
#if !defined(CONFIG_DMA_COHERENT) || !defined(CONFIG_DMA_IP27)
#undef CONFIG_CPU_HAS_PREFETCH
#endif
#ifdef CONFIG_MIPS_MALTA
#undef CONFIG_CPU_HAS_PREFETCH
#endif

#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/regdef.h>

#define dst a0
#define src a1
#define len a2

/*
 * Spec
 *
 * memcpy copies len bytes from src to dst and sets v0 to dst.
 * It assumes that
 *   - src and dst don't overlap
 *   - src is readable
 *   - dst is writable
 * memcpy uses the standard calling convention
 *
 * __copy_user copies up to len bytes from src to dst and sets a2 (len) to
 * the number of uncopied bytes due to an exception caused by a read or write.
 * __copy_user assumes that src and dst don't overlap, and that the call is
 * implementing one of the following:
 *   copy_to_user
 *     - src is readable  (no exceptions when reading src)
 *   copy_from_user
 *     - dst is writable  (no exceptions when writing dst)
 * __copy_user uses a non-standard calling convention; see
 * include/asm-mips/uaccess.h
 *
 * When an exception happens on a load, the handler must
 # ensure that all of the destination buffer is overwritten to prevent
 * leaking information to user mode programs.
 */

/*
 * Implementation
 */

/*
 * The exception handler for loads requires that:
 *  1- AT contain the address of the byte just past the end of the source
 *     of the copy,
 *  2- src_entry <= src < AT, and
 *  3- (dst - src) == (dst_entry - src_entry),
 * The _entry suffix denotes values when __copy_user was called.
 *
 * (1) is set up up by uaccess.h and maintained by not writing AT in copy_user
 * (2) is met by incrementing src by the number of bytes copied
 * (3) is met by not doing loads between a pair of increments of dst and src
 *
 * The exception handlers for stores adjust len (if necessary) and return.
 * These handlers do not need to overwrite any data.
 *
 * For __rmemcpy and memmove an exception is always a kernel bug, therefore
 * they're not protected.
 */

#define EXC(inst_reg,addr,handler)		\
9:	inst_reg, addr;				\
	.section __ex_table,"a";		\
	PTR	9b, handler;			\
	.previous

/*
 * Only on the 64-bit kernel we can made use of 64-bit registers.
 */
#ifdef CONFIG_64BIT
#define USE_DOUBLE
#endif

#ifdef USE_DOUBLE

#define LOAD   ld
#define LOADL  ldl
#define LOADR  ldr
#define STOREL sdl
#define STORER sdr
#define STORE  sd
#define ADD    daddu
#define SUB    dsubu
#define SRL    dsrl
#define SRA    dsra
#define SLL    dsll
#define SLLV   dsllv
#define SRLV   dsrlv
#define NBYTES 8
#define LOG_NBYTES 3

/*
 * As we are sharing code base with the mips32 tree (which use the o32 ABI
 * register definitions). We need to redefine the register definitions from
 * the n64 ABI register naming to the o32 ABI register naming.
 */
#undef t0
#undef t1
#undef t2
#undef t3
#define t0	$8
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define t5	$13
#define t6	$14
#define t7	$15

#else

#define LOAD   lw
#define LOADL  lwl
#define LOADR  lwr
#define STOREL swl
#define STORER swr
#define STORE  sw
#define ADD    addu
#define SUB    subu
#define SRL    srl
#define SLL    sll
#define SRA    sra
#define SLLV   sllv
#define SRLV   srlv
#define NBYTES 4
#define LOG_NBYTES 2

#endif /* USE_DOUBLE */

#ifdef CONFIG_CPU_LITTLE_ENDIAN
#define LDFIRST LOADR
#define LDREST  LOADL
#define STFIRST STORER
#define STREST  STOREL
#define SHIFT_DISCARD SLLV
#else
#define LDFIRST LOADL
#define LDREST  LOADR
#define STFIRST STOREL
#define STREST  STORER
#define SHIFT_DISCARD SRLV
#endif

#define FIRST(unit) ((unit)*NBYTES)
#define REST(unit)  (FIRST(unit)+NBYTES-1)
#define UNIT(unit)  FIRST(unit)

#define ADDRMASK (NBYTES-1)

	.text
	.set	noreorder
	.set	noat

/*
 * A combined memcpy/__copy_user
 * __copy_user sets len to 0 for success; else to an upper bound of
 * the number of uncopied bytes.
 * memcpy sets v0 to dst.
 */
	.align	5
LEAF(__copy_user_inatomic)
	/*
	 * Note: dst & src may be unaligned, len may be 0
	 * Temps
	 */
#define rem t8

	/*
	 * The "issue break"s below are very approximate.
	 * Issue delays for dcache fills will perturb the schedule, as will
	 * load queue full replay traps, etc.
	 *
	 * If len < NBYTES use byte operations.
	 */
	PREF(	0, 0(src) )
	PREF(	1, 0(dst) )
	sltu	t2, len, NBYTES
	and	t1, dst, ADDRMASK
	PREF(	0, 1*32(src) )
	PREF(	1, 1*32(dst) )
	bnez	t2, copy_bytes_checklen
	 and	t0, src, ADDRMASK
	PREF(	0, 2*32(src) )
	PREF(	1, 2*32(dst) )
	bnez	t1, dst_unaligned
	 nop
	bnez	t0, src_unaligned_dst_aligned
	/*
	 * use delay slot for fall-through
	 * src and dst are aligned; need to compute rem
	 */
both_aligned:
	 SRL	t0, len, LOG_NBYTES+3    # +3 for 8 units/iter
	beqz	t0, cleanup_both_aligned # len < 8*NBYTES
	 and	rem, len, (8*NBYTES-1)	 # rem = len % (8*NBYTES)
	PREF(	0, 3*32(src) )
	PREF(	1, 3*32(dst) )
	.align	4
1:
EXC(	LOAD	t0, UNIT(0)(src),	l_exc)
EXC(	LOAD	t1, UNIT(1)(src),	l_exc_copy)
EXC(	LOAD	t2, UNIT(2)(src),	l_exc_copy)
EXC(	LOAD	t3, UNIT(3)(src),	l_exc_copy)
	SUB	len, len, 8*NBYTES
EXC(	LOAD	t4, UNIT(4)(src),	l_exc_copy)
EXC(	LOAD	t7, UNIT(5)(src),	l_exc_copy)
	STORE	t0, UNIT(0)(dst)
	STORE	t1, UNIT(1)(dst)
EXC(	LOAD	t0, UNIT(6)(src),	l_exc_copy)
EXC(	LOAD	t1, UNIT(7)(src),	l_exc_copy)
	ADD	src, src, 8*NBYTES
	ADD	dst, dst, 8*NBYTES
	STORE	t2, UNIT(-6)(dst)
	STORE	t3, UNIT(-5)(dst)
	STORE	t4, UNIT(-4)(dst)
	STORE	t7, UNIT(-3)(dst)
	STORE	t0, UNIT(-2)(dst)
	STORE	t1, UNIT(-1)(dst)
	PREF(	0, 8*32(src) )
	PREF(	1, 8*32(dst) )
	bne	len, rem, 1b
	 nop

	/*
	 * len == rem == the number of bytes left to copy < 8*NBYTES
	 */
cleanup_both_aligned:
	beqz	len, done
	 sltu	t0, len, 4*NBYTES
	bnez	t0, less_than_4units
	 and	rem, len, (NBYTES-1)	# rem = len % NBYTES
	/*
	 * len >= 4*NBYTES
	 */
EXC(	LOAD	t0, UNIT(0)(src),	l_exc)
EXC(	LOAD	t1, UNIT(1)(src),	l_exc_copy)
EXC(	LOAD	t2, UNIT(2)(src),	l_exc_copy)
EXC(	LOAD	t3, UNIT(3)(src),	l_exc_copy)
	SUB	len, len, 4*NBYTES
	ADD	src, src, 4*NBYTES
	STORE	t0, UNIT(0)(dst)
	STORE	t1, UNIT(1)(dst)
	STORE	t2, UNIT(2)(dst)
	STORE	t3, UNIT(3)(dst)
	beqz	len, done
	 ADD	dst, dst, 4*NBYTES
less_than_4units:
	/*
	 * rem = len % NBYTES
	 */
	beq	rem, len, copy_bytes
	 nop
1:
EXC(	LOAD	t0, 0(src),		l_exc)
	ADD	src, src, NBYTES
	SUB	len, len, NBYTES
	STORE	t0, 0(dst)
	bne	rem, len, 1b
	 ADD	dst, dst, NBYTES

	/*
	 * src and dst are aligned, need to copy rem bytes (rem < NBYTES)
	 * A loop would do only a byte at a time with possible branch
	 * mispredicts.  Can't do an explicit LOAD dst,mask,or,STORE
	 * because can't assume read-access to dst.  Instead, use
	 * STREST dst, which doesn't require read access to dst.
	 *
	 * This code should perform better than a simple loop on modern,
	 * wide-issue mips processors because the code has fewer branches and
	 * more instruction-level parallelism.
	 */
#define bits t2
	beqz	len, done
	 ADD	t1, dst, len	# t1 is just past last byte of dst
	li	bits, 8*NBYTES
	SLL	rem, len, 3	# rem = number of bits to keep
EXC(	LOAD	t0, 0(src),		l_exc)
	SUB	bits, bits, rem	# bits = number of bits to discard
	SHIFT_DISCARD t0, t0, bits
	STREST	t0, -1(t1)
	jr	ra
	 move	len, zero
dst_unaligned:
	/*
	 * dst is unaligned
	 * t0 = src & ADDRMASK
	 * t1 = dst & ADDRMASK; T1 > 0
	 * len >= NBYTES
	 *
	 * Copy enough bytes to align dst
	 * Set match = (src and dst have same alignment)
	 */
#define match rem
EXC(	LDFIRST	t3, FIRST(0)(src),	l_exc)
	ADD	t2, zero, NBYTES
EXC(	LDREST	t3, REST(0)(src),	l_exc_copy)
	SUB	t2, t2, t1	# t2 = number of bytes copied
	xor	match, t0, t1
	STFIRST t3, FIRST(0)(dst)
	beq	len, t2, done
	 SUB	len, len, t2
	ADD	dst, dst, t2
	beqz	match, both_aligned
	 ADD	src, src, t2

src_unaligned_dst_aligned:
	SRL	t0, len, LOG_NBYTES+2    # +2 for 4 units/iter
	PREF(	0, 3*32(src) )
	beqz	t0, cleanup_src_unaligned
	 and	rem, len, (4*NBYTES-1)   # rem = len % 4*NBYTES
	PREF(	1, 3*32(dst) )
1:
/*
 * Avoid consecutive LD*'s to the same register since some mips
 * implementations can't issue them in the same cycle.
 * It's OK to load FIRST(N+1) before REST(N) because the two addresses
 * are to the same unit (unless src is aligned, but it's not).
 */
EXC(	LDFIRST	t0, FIRST(0)(src),	l_exc)
EXC(	LDFIRST	t1, FIRST(1)(src),	l_exc_copy)
	SUB     len, len, 4*NBYTES
EXC(	LDREST	t0, REST(0)(src),	l_exc_copy)
EXC(	LDREST	t1, REST(1)(src),	l_exc_copy)
EXC(	LDFIRST	t2, FIRST(2)(src),	l_exc_copy)
EXC(	LDFIRST	t3, FIRST(3)(src),	l_exc_copy)
EXC(	LDREST	t2, REST(2)(src),	l_exc_copy)
EXC(	LDREST	t3, REST(3)(src),	l_exc_copy)
	PREF(	0, 9*32(src) )		# 0 is PREF_LOAD  (not streamed)
	ADD	src, src, 4*NBYTES
#ifdef CONFIG_CPU_SB1
	nop				# improves slotting
#endif
	STORE	t0, UNIT(0)(dst)
	STORE	t1, UNIT(1)(dst)
	STORE	t2, UNIT(2)(dst)
	STORE	t3, UNIT(3)(dst)
	PREF(	1, 9*32(dst) )     	# 1 is PREF_STORE (not streamed)
	bne	len, rem, 1b
	 ADD	dst, dst, 4*NBYTES

cleanup_src_unaligned:
	beqz	len, done
	 and	rem, len, NBYTES-1  # rem = len % NBYTES
	beq	rem, len, copy_bytes
	 nop
1:
EXC(	LDFIRST t0, FIRST(0)(src),	l_exc)
EXC(	LDREST	t0, REST(0)(src),	l_exc_copy)
	ADD	src, src, NBYTES
	SUB	len, len, NBYTES
	STORE	t0, 0(dst)
	bne	len, rem, 1b
	 ADD	dst, dst, NBYTES

copy_bytes_checklen:
	beqz	len, done
	 nop
copy_bytes:
	/* 0 < len < NBYTES  */
#define COPY_BYTE(N)			\
EXC(	lb	t0, N(src), l_exc);	\
	SUB	len, len, 1;		\
	beqz	len, done;		\
	 sb	t0, N(dst)

	COPY_BYTE(0)
	COPY_BYTE(1)
#ifdef USE_DOUBLE
	COPY_BYTE(2)
	COPY_BYTE(3)
	COPY_BYTE(4)
	COPY_BYTE(5)
#endif
EXC(	lb	t0, NBYTES-2(src), l_exc)
	SUB	len, len, 1
	jr	ra
	 sb	t0, NBYTES-2(dst)
done:
	jr	ra
	 nop
	END(__copy_user_inatomic)

l_exc_copy:
	/*
	 * Copy bytes from src until faulting load address (or until a
	 * lb faults)
	 *
	 * When reached by a faulting LDFIRST/LDREST, THREAD_BUADDR($28)
	 * may be more than a byte beyond the last address.
	 * Hence, the lb below may get an exception.
	 *
	 * Assumes src < THREAD_BUADDR($28)
	 */
	LOAD	t0, TI_TASK($28)
	 nop
	LOAD	t0, THREAD_BUADDR(t0)
1:
EXC(	lb	t1, 0(src),	l_exc)
	ADD	src, src, 1
	sb	t1, 0(dst)	# can't fault -- we're copy_from_user
	bne	src, t0, 1b
	 ADD	dst, dst, 1
l_exc:
	LOAD	t0, TI_TASK($28)
	 nop
	LOAD	t0, THREAD_BUADDR(t0)	# t0 is just past last good address
	 nop
	SUB	len, AT, t0		# len number of uncopied bytes
	jr	ra
	 nop

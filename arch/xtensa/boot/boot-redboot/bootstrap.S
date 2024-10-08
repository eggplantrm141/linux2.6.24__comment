#include <asm/variant/core.h>
#include <asm/regs.h>
#include <asm/asmmacro.h>
#include <asm/cacheasm.h>
	/*
	 * RB-Data: RedBoot data/bss
	 * P:	    Boot-Parameters
	 * L:	    Kernel-Loader
	 *
	 * The Linux-Kernel image including the loader must be loaded
	 * to a position so that the kernel and the boot parameters
	 * can fit in the space before the load address.
	 *  ______________________________________________________
	 * |_RB-Data_|_P_|__________|_L_|___Linux-Kernel___|______|
	 *                          ^
	 *                          ^ Load address
	 *  ______________________________________________________
	 * |___Linux-Kernel___|_P_|_L_|___________________________|
	 *
	 * The loader copies the parameter to the position that will
	 * be the end of the kernel and itself to the end of the
	 * parameter list.
	 */

/* Make sure we have enough space for the 'uncompressor' */

#define STACK_SIZE 32768
#define HEAP_SIZE (131072*4)

	# a2: Parameter list
	# a3: Size of parameter list

	.section .start, "ax"

	.globl __start
	/* this must be the first byte of the loader! */
__start:
	entry	sp, 32		# we do not intend to return
	_call0	_start
__start_a0:
	.align 4

	.section .text, "ax"
	.begin literal_prefix .text

	/* put literals in here! */

	.globl _start
_start:

	/* 'reset' window registers */

	movi	a4, 1
	wsr	a4, PS
	rsync

	rsr	a5, WINDOWBASE
	ssl	a5
	sll	a4, a4
	wsr	a4, WINDOWSTART
	rsync

	movi	a4, 0x00040000
	wsr	a4, PS
	rsync

	/* copy the loader to its address
	 * Note: The loader itself is a very small piece, so we assume we
	 *       don't partially overlap. We also assume (even more important)
	 *	 that the kernel image is out of the way. Usually, when the
	 *	 load address of this image is not at an arbitrary address,
	 *	 but aligned to some 10K's we shouldn't overlap.
	 */

	/* Note: The assembler cannot relax "addi a0, a0, ..." to an
	   l32r, so we load to a4 first. */

	# addi	a4, a0, __start - __start_a0
	# mov	a0, a4

	movi	a4, __start
	movi	a5, __start_a0
	add	a4, a0, a4
	sub	a0, a4, a5

	movi	a4, __start
	movi	a5, __reloc_end

	# a0: address where this code has been loaded
	# a4: compiled address of __start
	# a5: compiled end address

	mov.n	a7, a0
	mov.n	a8, a4

1:
	l32i	a10, a7, 0
	l32i	a11, a7, 4
	s32i	a10, a8, 0
	s32i	a11, a8, 4
	l32i	a10, a7, 8
	l32i	a11, a7, 12
	s32i	a10, a8, 8
	s32i	a11, a8, 12
	addi	a8, a8, 16
	addi	a7, a7, 16
	blt	a8, a5, 1b


	/* We have to flush and invalidate the caches here before we jump. */

#if XCHAL_DCACHE_IS_WRITEBACK

	___flush_dcache_all a5 a6

#endif

	___invalidate_icache_all a5 a6
	isync

	movi	a11, _reloc
	jx	a11

	.globl _reloc
_reloc:

	/* RedBoot is now at the end of the memory, so we don't have
	 * to copy the parameter list. Keep the code around; in case
	 * we need it again. */
#if 0
	# a0: load address
	# a2: start address of parameter list
	# a3: length of parameter list
	# a4: __start

	/* copy the parameter list out of the way */

	movi	a6, _param_start
	add	a3, a2, a3
2:
	l32i	a8, a2, 0
	s32i	a8, a6, 0
	addi	a2, a2, 4
	addi	a6, a6, 4
	blt	a2, a3, 2b
#endif

	/* clear BSS section */
	movi	a6, __bss_start
	movi	a7, __bss_end
	movi.n	a5, 0
3:
	s32i	a5, a6, 0
	addi	a6, a6, 4
	blt	a6, a7, 3b

	movi	a5, -16
	movi	a1, _stack + STACK_SIZE
	and	a1, a1, a5

	/* Uncompress the kernel */

	# a0: load address
	# a2: boot parameter
	# a4: __start

	movi	a3, __image_load
	sub	a4, a3, a4
	add	a8, a0, a4

	# a1  Stack
	# a8(a4)  Load address of the image

	movi	a6, _image_start
	movi	a10, _image_end
	movi	a7, 0x1000000
	sub	a11, a10, a6
	movi	a9, complen
	s32i	a11, a9, 0

	movi	a0, 0

	# a6 destination
	# a7 maximum size of destination
	# a8 source
	# a9 ptr to length

	.extern gunzip
	movi	a4, gunzip
	beqz	a4, 1f

	callx4	a4

	j	2f


	# a6 destination start
	# a7 maximum size of destination
	# a8 source start
	# a9 ptr to length
	# a10 destination end

1:
        l32i    a9, a8, 0
        l32i    a11, a8, 4
        s32i    a9, a6, 0
        s32i    a11, a6, 4
        l32i    a9, a8, 8
        l32i    a11, a8, 12
        s32i    a9, a6, 8
        s32i    a11, a6, 12
        addi    a6, a6, 16
        addi    a8, a8, 16
        blt     a6, a10, 1b


	/* jump to the kernel */
2:
#if XCHAL_DCACHE_IS_WRITEBACK

	___flush_dcache_all a5 a6

#endif

	___invalidate_icache_all a5 a6

	isync

	movi	a5, __start
	movi	a3, boot_initrd_start
	movi	a4, boot_initrd_end
	sub	a3, a3, a5
	sub	a4, a4, a5
	add	a3, a0, a3
	add	a4, a0, a4

	# a2  Boot parameter list
	# a3  initrd_start (virtual load address)
	# a4  initrd_end   (virtual load address)

	movi	a0, _image_start
	jx	a0

	.align 16
	.data
	.globl avail_ram
avail_ram:
	.long	_heap
	.globl end_avail
end_avail:
	.long	_heap + HEAP_SIZE

	.comm _stack, STACK_SIZE
	.comm _heap, HEAP_SIZE

	.globl end_avail
	.comm complen, 4

	.end	literal_prefix

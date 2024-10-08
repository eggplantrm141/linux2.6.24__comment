/* NG2copy_from_user.S: Niagara-2 optimized copy from userspace.
 *
 * Copyright (C) 2007 David S. Miller (davem@davemloft.net)
 */

#define EX_LD(x)		\
98:	x;			\
	.section .fixup;	\
	.align 4;		\
99:	wr	%g0, ASI_AIUS, %asi;\
	retl;			\
	 mov	1, %o0;		\
	.section __ex_table,"a";\
	.align 4;		\
	.word 98b, 99b;		\
	.text;			\
	.align 4;

#ifndef ASI_AIUS
#define ASI_AIUS	0x11
#endif

#ifndef ASI_BLK_AIUS_4V
#define ASI_BLK_AIUS_4V	0x17
#endif

#define FUNC_NAME		NG2copy_from_user
#define LOAD(type,addr,dest)	type##a [addr] %asi, dest
#define LOAD_BLK(addr,dest)	ldda [addr] ASI_BLK_AIUS_4V, dest
#define EX_RETVAL(x)		0

#ifdef __KERNEL__
#define PREAMBLE					\
	rd		%asi, %g1;			\
	cmp		%g1, ASI_AIUS;			\
	bne,pn		%icc, memcpy_user_stub;		\
	 nop
#endif

#include "NG2memcpy.S"

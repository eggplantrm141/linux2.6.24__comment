/* Changes made by  LG Soft Oct 2004*/

#ifndef __ASMBFIN_ELF_H
#define __ASMBFIN_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>

/* Processor specific flags for the ELF header e_flags field.  */
#define EF_BFIN_PIC		0x00000001	/* -fpic */
#define EF_BFIN_FDPIC		0x00000002	/* -mfdpic */
#define EF_BFIN_CODE_IN_L1	0x00000010	/* --code-in-l1 */
#define EF_BFIN_DATA_IN_L1	0x00000020	/* --data-in-l1 */

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_bfinfp_struct elf_fpregset_t;
/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_BLACKFIN)

#define elf_check_fdpic(x) ((x)->e_flags & EF_BFIN_FDPIC /* && !((x)->e_flags & EF_FRV_NON_PIC_RELOCS) */)
#define elf_check_const_displacement(x) ((x)->e_flags & EF_BFIN_PIC)

/* EM_BLACKFIN defined in linux/elf.h	*/

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_BLACKFIN

#define ELF_PLAT_INIT(_r)	_r->p1 = 0

#define ELF_FDPIC_PLAT_INIT(_regs, _exec_map_addr, _interp_map_addr, _dynamic_addr)	\
do {											\
	_regs->r7	= 0;						\
	_regs->p0	= _exec_map_addr;				\
	_regs->p1	= _interp_map_addr;				\
	_regs->p2	= _dynamic_addr;				\
} while(0)

#define USE_ELF_CORE_DUMP
#define ELF_FDPIC_CORE_EFLAGS	EF_BFIN_FDPIC
#define ELF_EXEC_PAGESIZE	4096

#define	R_unused0	0	/* relocation type 0 is not defined */
#define R_pcrel5m2	1	/*LSETUP part a */
#define R_unused1	2	/* relocation type 2 is not defined */
#define R_pcrel10	3	/* type 3, if cc jump <target>  */
#define R_pcrel12_jump	4	/* type 4, jump <target> */
#define R_rimm16	5	/* type 0x5, rN = <target> */
#define R_luimm16	6	/* # 0x6, preg.l=<target> Load imm 16 to lower half */
#define R_huimm16  	7	/* # 0x7, preg.h=<target> Load imm 16 to upper half */
#define R_pcrel12_jump_s 8	/* # 0x8 jump.s <target> */
#define R_pcrel24_jump_x 9	/* # 0x9 jump.x <target> */
#define R_pcrel24       10	/* # 0xa call <target> , not expandable */
#define R_unusedb       11	/* # 0xb not generated */
#define R_unusedc       12	/* # 0xc  not used */
#define R_pcrel24_jump_l 13	/*0xd jump.l <target> */
#define R_pcrel24_call_x 14	/* 0xE, call.x <target> if <target> is above 24 bit limit call through P1 */
#define R_var_eq_symb    15	/* 0xf, linker should treat it same as 0x12 */
#define R_byte_data      16	/* 0x10, .byte var = symbol */
#define R_byte2_data     17	/* 0x11, .byte2 var = symbol */
#define R_byte4_data     18	/* 0x12, .byte4 var = symbol and .var var=symbol */
#define R_pcrel11        19	/* 0x13, lsetup part b */
#define R_unused14      20	/* 0x14, undefined */
#define R_unused15       21	/* not generated by VDSP 3.5 */

/* arithmetic relocations */
#define R_push		 0xE0
#define R_const		 0xE1
#define R_add		 0xE2
#define R_sub		 0xE3
#define R_mult		 0xE4
#define R_div		 0xE5
#define R_mod		 0xE6
#define R_lshift	 0xE7
#define R_rshift	 0xE8
#define R_and		 0xE9
#define R_or		 0xEA
#define R_xor		 0xEB
#define R_land		 0xEC
#define R_lor		 0xED
#define R_len		 0xEE
#define R_neg		 0xEF
#define R_comp		 0xF0
#define R_page		 0xF1
#define R_hwpage	 0xF2
#define R_addr		 0xF3

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         0xD0000000UL

#define ELF_CORE_COPY_REGS(pr_reg, regs)	\
        memcpy((char *) &pr_reg, (char *)regs,  \
               sizeof(struct pt_regs));

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.  */

#define ELF_PLATFORM  (NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)
#endif

#endif

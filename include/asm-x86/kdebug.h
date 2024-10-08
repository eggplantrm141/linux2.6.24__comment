#ifndef _ASM_X86_KDEBUG_H
#define _ASM_X86_KDEBUG_H

#include <linux/notifier.h>

struct pt_regs;

/* Grossly misnamed. */
enum die_val {
	DIE_OOPS = 1,
	DIE_INT3,
	DIE_DEBUG,
	DIE_PANIC,
	DIE_NMI,
	DIE_DIE,
	DIE_NMIWATCHDOG,
	DIE_KERNELDEBUG,
	DIE_TRAP,
	DIE_GPF,
	DIE_CALL,
	DIE_NMI_IPI,
	DIE_PAGE_FAULT,
};

extern void printk_address(unsigned long address);
extern void die(const char *,struct pt_regs *,long);
extern void __die(const char *,struct pt_regs *,long);
extern void show_registers(struct pt_regs *regs);
extern void dump_pagetable(unsigned long);
extern unsigned long oops_begin(void);
extern void oops_end(unsigned long);

#endif

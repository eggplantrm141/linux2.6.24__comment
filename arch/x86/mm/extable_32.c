/*
 * linux/arch/i386/mm/extable.c
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>

int fixup_exception(struct pt_regs *regs)
{
	const struct exception_table_entry *fixup;

#ifdef CONFIG_PNPBIOS
	if (unlikely(SEGMENT_IS_PNP_CODE(regs->xcs)))
	{
		extern u32 pnp_bios_fault_eip, pnp_bios_fault_esp;
		extern u32 pnp_bios_is_utter_crap;
		pnp_bios_is_utter_crap = 1;
		printk(KERN_CRIT "PNPBIOS fault.. attempting recovery.\n");
		__asm__ volatile(
			"movl %0, %%esp\n\t"
			"jmp *%1\n\t"
			: : "g" (pnp_bios_fault_esp), "g" (pnp_bios_fault_eip));
		panic("do_trap: can't hit this");
	}
#endif

	/* 根据异常发生时的指令地址，查找修复表 */
	fixup = search_exception_tables(regs->eip);
	if (fixup) {/* 在修复表中存在修复地址 */
		regs->eip = fixup->fixup;/* 修改返回地址，使其跳转到修复地址 */
		return 1;
	}

	return 0;
}

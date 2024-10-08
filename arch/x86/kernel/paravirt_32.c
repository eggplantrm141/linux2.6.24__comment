/*  Paravirtualization interfaces
    Copyright (C) 2006 Rusty Russell IBM Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/efi.h>
#include <linux/bcd.h>
#include <linux/highmem.h>

#include <asm/bug.h>
#include <asm/paravirt.h>
#include <asm/desc.h>
#include <asm/setup.h>
#include <asm/arch_hooks.h>
#include <asm/time.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/fixmap.h>
#include <asm/apic.h>
#include <asm/tlbflush.h>
#include <asm/timer.h>

/* nop stub */
void _paravirt_nop(void)
{
}

static void __init default_banner(void)
{
	printk(KERN_INFO "Booting paravirtualized kernel on %s\n",
	       pv_info.name);
}

char *memory_setup(void)
{
	return pv_init_ops.memory_setup();
}

/* Simple instruction patching code. */
#define DEF_NATIVE(ops, name, code)					\
	extern const char start_##ops##_##name[], end_##ops##_##name[];	\
	asm("start_" #ops "_" #name ": " code "; end_" #ops "_" #name ":")

DEF_NATIVE(pv_irq_ops, irq_disable, "cli");
DEF_NATIVE(pv_irq_ops, irq_enable, "sti");
DEF_NATIVE(pv_irq_ops, restore_fl, "push %eax; popf");
DEF_NATIVE(pv_irq_ops, save_fl, "pushf; pop %eax");
DEF_NATIVE(pv_cpu_ops, iret, "iret");
DEF_NATIVE(pv_cpu_ops, irq_enable_sysexit, "sti; sysexit");
DEF_NATIVE(pv_mmu_ops, read_cr2, "mov %cr2, %eax");
DEF_NATIVE(pv_mmu_ops, write_cr3, "mov %eax, %cr3");
DEF_NATIVE(pv_mmu_ops, read_cr3, "mov %cr3, %eax");
DEF_NATIVE(pv_cpu_ops, clts, "clts");
DEF_NATIVE(pv_cpu_ops, read_tsc, "rdtsc");

/* Undefined instruction for dealing with missing ops pointers. */
static const unsigned char ud2a[] = { 0x0f, 0x0b };

static unsigned native_patch(u8 type, u16 clobbers, void *ibuf,
			     unsigned long addr, unsigned len)
{
	const unsigned char *start, *end;
	unsigned ret;

	switch(type) {
#define SITE(ops, x)						\
	case PARAVIRT_PATCH(ops.x):				\
		start = start_##ops##_##x;			\
		end = end_##ops##_##x;				\
		goto patch_site

	SITE(pv_irq_ops, irq_disable);
	SITE(pv_irq_ops, irq_enable);
	SITE(pv_irq_ops, restore_fl);
	SITE(pv_irq_ops, save_fl);
	SITE(pv_cpu_ops, iret);
	SITE(pv_cpu_ops, irq_enable_sysexit);
	SITE(pv_mmu_ops, read_cr2);
	SITE(pv_mmu_ops, read_cr3);
	SITE(pv_mmu_ops, write_cr3);
	SITE(pv_cpu_ops, clts);
	SITE(pv_cpu_ops, read_tsc);
#undef SITE

	patch_site:
		ret = paravirt_patch_insns(ibuf, len, start, end);
		break;

	default:
		ret = paravirt_patch_default(type, clobbers, ibuf, addr, len);
		break;
	}

	return ret;
}

unsigned paravirt_patch_nop(void)
{
	return 0;
}

unsigned paravirt_patch_ignore(unsigned len)
{
	return len;
}

struct branch {
	unsigned char opcode;
	u32 delta;
} __attribute__((packed));

unsigned paravirt_patch_call(void *insnbuf,
			     const void *target, u16 tgt_clobbers,
			     unsigned long addr, u16 site_clobbers,
			     unsigned len)
{
	struct branch *b = insnbuf;
	unsigned long delta = (unsigned long)target - (addr+5);

	if (tgt_clobbers & ~site_clobbers)
		return len;	/* target would clobber too much for this site */
	if (len < 5)
		return len;	/* call too long for patch site */

	b->opcode = 0xe8; /* call */
	b->delta = delta;
	BUILD_BUG_ON(sizeof(*b) != 5);

	return 5;
}

unsigned paravirt_patch_jmp(void *insnbuf, const void *target,
			    unsigned long addr, unsigned len)
{
	struct branch *b = insnbuf;
	unsigned long delta = (unsigned long)target - (addr+5);

	if (len < 5)
		return len;	/* call too long for patch site */

	b->opcode = 0xe9;	/* jmp */
	b->delta = delta;

	return 5;
}

/* Neat trick to map patch type back to the call within the
 * corresponding structure. */
static void *get_call_destination(u8 type)
{
	struct paravirt_patch_template tmpl = {
		.pv_init_ops = pv_init_ops,
		.pv_time_ops = pv_time_ops,
		.pv_cpu_ops = pv_cpu_ops,
		.pv_irq_ops = pv_irq_ops,
		.pv_apic_ops = pv_apic_ops,
		.pv_mmu_ops = pv_mmu_ops,
	};
	return *((void **)&tmpl + type);
}

unsigned paravirt_patch_default(u8 type, u16 clobbers, void *insnbuf,
				unsigned long addr, unsigned len)
{
	void *opfunc = get_call_destination(type);
	unsigned ret;

	if (opfunc == NULL)
		/* If there's no function, patch it with a ud2a (BUG) */
		ret = paravirt_patch_insns(insnbuf, len, ud2a, ud2a+sizeof(ud2a));
	else if (opfunc == paravirt_nop)
		/* If the operation is a nop, then nop the callsite */
		ret = paravirt_patch_nop();
	else if (type == PARAVIRT_PATCH(pv_cpu_ops.iret) ||
		 type == PARAVIRT_PATCH(pv_cpu_ops.irq_enable_sysexit))
		/* If operation requires a jmp, then jmp */
		ret = paravirt_patch_jmp(insnbuf, opfunc, addr, len);
	else
		/* Otherwise call the function; assume target could
		   clobber any caller-save reg */
		ret = paravirt_patch_call(insnbuf, opfunc, CLBR_ANY,
					  addr, clobbers, len);

	return ret;
}

unsigned paravirt_patch_insns(void *insnbuf, unsigned len,
			      const char *start, const char *end)
{
	unsigned insn_len = end - start;

	if (insn_len > len || start == NULL)
		insn_len = len;
	else
		memcpy(insnbuf, start, insn_len);

	return insn_len;
}

void init_IRQ(void)
{
	pv_irq_ops.init_IRQ();
}

static void native_flush_tlb(void)
{
	__native_flush_tlb();
}

/*
 * Global pages have to be flushed a bit differently. Not a real
 * performance problem because this does not happen often.
 */
static void native_flush_tlb_global(void)
{
	__native_flush_tlb_global();
}

static void native_flush_tlb_single(unsigned long addr)
{
	__native_flush_tlb_single(addr);
}

/* These are in entry.S */
extern void native_iret(void);
extern void native_irq_enable_sysexit(void);

static int __init print_banner(void)
{
	pv_init_ops.banner();
	return 0;
}
core_initcall(print_banner);

static struct resource reserve_ioports = {
	.start = 0,
	.end = IO_SPACE_LIMIT,
	.name = "paravirt-ioport",
	.flags = IORESOURCE_IO | IORESOURCE_BUSY,
};

static struct resource reserve_iomem = {
	.start = 0,
	.end = -1,
	.name = "paravirt-iomem",
	.flags = IORESOURCE_MEM | IORESOURCE_BUSY,
};

/*
 * Reserve the whole legacy IO space to prevent any legacy drivers
 * from wasting time probing for their hardware.  This is a fairly
 * brute-force approach to disabling all non-virtual drivers.
 *
 * Note that this must be called very early to have any effect.
 */
int paravirt_disable_iospace(void)
{
	int ret;

	ret = request_resource(&ioport_resource, &reserve_ioports);
	if (ret == 0) {
		ret = request_resource(&iomem_resource, &reserve_iomem);
		if (ret)
			release_resource(&reserve_ioports);
	}

	return ret;
}

static DEFINE_PER_CPU(enum paravirt_lazy_mode, paravirt_lazy_mode) = PARAVIRT_LAZY_NONE;

static inline void enter_lazy(enum paravirt_lazy_mode mode)
{
	BUG_ON(x86_read_percpu(paravirt_lazy_mode) != PARAVIRT_LAZY_NONE);
	BUG_ON(preemptible());

	x86_write_percpu(paravirt_lazy_mode, mode);
}

void paravirt_leave_lazy(enum paravirt_lazy_mode mode)
{
	BUG_ON(x86_read_percpu(paravirt_lazy_mode) != mode);
	BUG_ON(preemptible());

	x86_write_percpu(paravirt_lazy_mode, PARAVIRT_LAZY_NONE);
}

void paravirt_enter_lazy_mmu(void)
{
	enter_lazy(PARAVIRT_LAZY_MMU);
}

void paravirt_leave_lazy_mmu(void)
{
	paravirt_leave_lazy(PARAVIRT_LAZY_MMU);
}

void paravirt_enter_lazy_cpu(void)
{
	enter_lazy(PARAVIRT_LAZY_CPU);
}

void paravirt_leave_lazy_cpu(void)
{
	paravirt_leave_lazy(PARAVIRT_LAZY_CPU);
}

enum paravirt_lazy_mode paravirt_get_lazy_mode(void)
{
	return x86_read_percpu(paravirt_lazy_mode);
}

struct pv_info pv_info = {
	.name = "bare hardware",
	.paravirt_enabled = 0,
	.kernel_rpl = 0,
	.shared_kernel_pmd = 1,	/* Only used when CONFIG_X86_PAE is set */
};

struct pv_init_ops pv_init_ops = {
	.patch = native_patch,
	.banner = default_banner,
	.arch_setup = paravirt_nop,
	.memory_setup = machine_specific_memory_setup,
};

struct pv_time_ops pv_time_ops = {
	.time_init = hpet_time_init,
	.get_wallclock = native_get_wallclock,
	.set_wallclock = native_set_wallclock,
	.sched_clock = native_sched_clock,
	.get_cpu_khz = native_calculate_cpu_khz,
};

struct pv_irq_ops pv_irq_ops = {
	.init_IRQ = native_init_IRQ,
	.save_fl = native_save_fl,
	.restore_fl = native_restore_fl,
	.irq_disable = native_irq_disable,
	.irq_enable = native_irq_enable,
	.safe_halt = native_safe_halt,
	.halt = native_halt,
};

struct pv_cpu_ops pv_cpu_ops = {
	.cpuid = native_cpuid,
	.get_debugreg = native_get_debugreg,
	.set_debugreg = native_set_debugreg,
	.clts = native_clts,
	.read_cr0 = native_read_cr0,
	.write_cr0 = native_write_cr0,
	.read_cr4 = native_read_cr4,
	.read_cr4_safe = native_read_cr4_safe,
	.write_cr4 = native_write_cr4,
	.wbinvd = native_wbinvd,
	.read_msr = native_read_msr_safe,
	.write_msr = native_write_msr_safe,
	.read_tsc = native_read_tsc,
	.read_pmc = native_read_pmc,
	.load_tr_desc = native_load_tr_desc,
	.set_ldt = native_set_ldt,
	.load_gdt = native_load_gdt,
	.load_idt = native_load_idt,
	.store_gdt = native_store_gdt,
	.store_idt = native_store_idt,
	.store_tr = native_store_tr,
	.load_tls = native_load_tls,
	.write_ldt_entry = write_dt_entry,
	.write_gdt_entry = write_dt_entry,
	.write_idt_entry = write_dt_entry,
	.load_esp0 = native_load_esp0,

	.irq_enable_sysexit = native_irq_enable_sysexit,
	.iret = native_iret,

	.set_iopl_mask = native_set_iopl_mask,
	.io_delay = native_io_delay,

	.lazy_mode = {
		.enter = paravirt_nop,
		.leave = paravirt_nop,
	},
};

struct pv_apic_ops pv_apic_ops = {
#ifdef CONFIG_X86_LOCAL_APIC
	.apic_write = native_apic_write,
	.apic_write_atomic = native_apic_write_atomic,
	.apic_read = native_apic_read,
	.setup_boot_clock = setup_boot_APIC_clock,
	.setup_secondary_clock = setup_secondary_APIC_clock,
	.startup_ipi_hook = paravirt_nop,
#endif
};

struct pv_mmu_ops pv_mmu_ops = {
	.pagetable_setup_start = native_pagetable_setup_start,
	.pagetable_setup_done = native_pagetable_setup_done,

	.read_cr2 = native_read_cr2,
	.write_cr2 = native_write_cr2,
	.read_cr3 = native_read_cr3,
	.write_cr3 = native_write_cr3,

	.flush_tlb_user = native_flush_tlb,
	.flush_tlb_kernel = native_flush_tlb_global,
	.flush_tlb_single = native_flush_tlb_single,
	.flush_tlb_others = native_flush_tlb_others,

	.alloc_pt = paravirt_nop,
	.alloc_pd = paravirt_nop,
	.alloc_pd_clone = paravirt_nop,
	.release_pt = paravirt_nop,
	.release_pd = paravirt_nop,

	.set_pte = native_set_pte,
	.set_pte_at = native_set_pte_at,
	.set_pmd = native_set_pmd,
	.pte_update = paravirt_nop,
	.pte_update_defer = paravirt_nop,

#ifdef CONFIG_HIGHPTE
	.kmap_atomic_pte = kmap_atomic,
#endif

#ifdef CONFIG_X86_PAE
	.set_pte_atomic = native_set_pte_atomic,
	.set_pte_present = native_set_pte_present,
	.set_pud = native_set_pud,
	.pte_clear = native_pte_clear,
	.pmd_clear = native_pmd_clear,

	.pmd_val = native_pmd_val,
	.make_pmd = native_make_pmd,
#endif

	.pte_val = native_pte_val,
	.pgd_val = native_pgd_val,

	.make_pte = native_make_pte,
	.make_pgd = native_make_pgd,

	.dup_mmap = paravirt_nop,
	.exit_mmap = paravirt_nop,
	.activate_mm = paravirt_nop,

	.lazy_mode = {
		.enter = paravirt_nop,
		.leave = paravirt_nop,
	},
};

EXPORT_SYMBOL_GPL(pv_time_ops);
EXPORT_SYMBOL    (pv_cpu_ops);
EXPORT_SYMBOL    (pv_mmu_ops);
EXPORT_SYMBOL_GPL(pv_apic_ops);
EXPORT_SYMBOL_GPL(pv_info);
EXPORT_SYMBOL    (pv_irq_ops);

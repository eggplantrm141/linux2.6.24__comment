/*
 * Kernel-based Virtual Machine driver for Linux
 *
 * AMD SVM support
 *
 * Copyright (C) 2006 Qumranet, Inc.
 *
 * Authors:
 *   Yaniv Kamay  <yaniv@qumranet.com>
 *   Avi Kivity   <avi@qumranet.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "kvm_svm.h"
#include "x86_emulate.h"
#include "irq.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/sched.h>

#include <asm/desc.h>

MODULE_AUTHOR("Qumranet");
MODULE_LICENSE("GPL");

#define IOPM_ALLOC_ORDER 2
#define MSRPM_ALLOC_ORDER 1

#define DB_VECTOR 1
#define UD_VECTOR 6
#define GP_VECTOR 13

#define DR7_GD_MASK (1 << 13)
#define DR6_BD_MASK (1 << 13)

#define SEG_TYPE_LDT 2
#define SEG_TYPE_BUSY_TSS16 3

#define KVM_EFER_LMA (1 << 10)
#define KVM_EFER_LME (1 << 8)

#define SVM_FEATURE_NPT  (1 << 0)
#define SVM_FEATURE_LBRV (1 << 1)
#define SVM_DEATURE_SVML (1 << 2)

static void kvm_reput_irq(struct vcpu_svm *svm);

static inline struct vcpu_svm *to_svm(struct kvm_vcpu *vcpu)
{
	return container_of(vcpu, struct vcpu_svm, vcpu);
}

unsigned long iopm_base;
unsigned long msrpm_base;

struct kvm_ldttss_desc {
	u16 limit0;
	u16 base0;
	unsigned base1 : 8, type : 5, dpl : 2, p : 1;
	unsigned limit1 : 4, zero0 : 3, g : 1, base2 : 8;
	u32 base3;
	u32 zero1;
} __attribute__((packed));

struct svm_cpu_data {
	int cpu;

	u64 asid_generation;
	u32 max_asid;
	u32 next_asid;
	struct kvm_ldttss_desc *tss_desc;

	struct page *save_area;
};

static DEFINE_PER_CPU(struct svm_cpu_data *, svm_data);
static uint32_t svm_features;

struct svm_init_data {
	int cpu;
	int r;
};

static u32 msrpm_ranges[] = {0, 0xc0000000, 0xc0010000};

#define NUM_MSR_MAPS ARRAY_SIZE(msrpm_ranges)
#define MSRS_RANGE_SIZE 2048
#define MSRS_IN_RANGE (MSRS_RANGE_SIZE * 8 / 2)

#define MAX_INST_SIZE 15

static inline u32 svm_has(u32 feat)
{
	return svm_features & feat;
}

static inline u8 pop_irq(struct kvm_vcpu *vcpu)
{
	int word_index = __ffs(vcpu->irq_summary);
	int bit_index = __ffs(vcpu->irq_pending[word_index]);
	int irq = word_index * BITS_PER_LONG + bit_index;

	clear_bit(bit_index, &vcpu->irq_pending[word_index]);
	if (!vcpu->irq_pending[word_index])
		clear_bit(word_index, &vcpu->irq_summary);
	return irq;
}

static inline void push_irq(struct kvm_vcpu *vcpu, u8 irq)
{
	set_bit(irq, vcpu->irq_pending);
	set_bit(irq / BITS_PER_LONG, &vcpu->irq_summary);
}

static inline void clgi(void)
{
	asm volatile (SVM_CLGI);
}

static inline void stgi(void)
{
	asm volatile (SVM_STGI);
}

static inline void invlpga(unsigned long addr, u32 asid)
{
	asm volatile (SVM_INVLPGA :: "a"(addr), "c"(asid));
}

static inline unsigned long kvm_read_cr2(void)
{
	unsigned long cr2;

	asm volatile ("mov %%cr2, %0" : "=r" (cr2));
	return cr2;
}

static inline void kvm_write_cr2(unsigned long val)
{
	asm volatile ("mov %0, %%cr2" :: "r" (val));
}

static inline unsigned long read_dr6(void)
{
	unsigned long dr6;

	asm volatile ("mov %%dr6, %0" : "=r" (dr6));
	return dr6;
}

static inline void write_dr6(unsigned long val)
{
	asm volatile ("mov %0, %%dr6" :: "r" (val));
}

static inline unsigned long read_dr7(void)
{
	unsigned long dr7;

	asm volatile ("mov %%dr7, %0" : "=r" (dr7));
	return dr7;
}

static inline void write_dr7(unsigned long val)
{
	asm volatile ("mov %0, %%dr7" :: "r" (val));
}

static inline void force_new_asid(struct kvm_vcpu *vcpu)
{
	to_svm(vcpu)->asid_generation--;
}

static inline void flush_guest_tlb(struct kvm_vcpu *vcpu)
{
	force_new_asid(vcpu);
}

static void svm_set_efer(struct kvm_vcpu *vcpu, u64 efer)
{
	if (!(efer & KVM_EFER_LMA))
		efer &= ~KVM_EFER_LME;

	to_svm(vcpu)->vmcb->save.efer = efer | MSR_EFER_SVME_MASK;
	vcpu->shadow_efer = efer;
}

static void svm_inject_gp(struct kvm_vcpu *vcpu, unsigned error_code)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	svm->vmcb->control.event_inj =		SVM_EVTINJ_VALID |
						SVM_EVTINJ_VALID_ERR |
						SVM_EVTINJ_TYPE_EXEPT |
						GP_VECTOR;
	svm->vmcb->control.event_inj_err = error_code;
}

static void inject_ud(struct kvm_vcpu *vcpu)
{
	to_svm(vcpu)->vmcb->control.event_inj = SVM_EVTINJ_VALID |
						SVM_EVTINJ_TYPE_EXEPT |
						UD_VECTOR;
}

static int is_page_fault(uint32_t info)
{
	info &= SVM_EVTINJ_VEC_MASK | SVM_EVTINJ_TYPE_MASK | SVM_EVTINJ_VALID;
	return info == (PF_VECTOR | SVM_EVTINJ_VALID | SVM_EVTINJ_TYPE_EXEPT);
}

static int is_external_interrupt(u32 info)
{
	info &= SVM_EVTINJ_TYPE_MASK | SVM_EVTINJ_VALID;
	return info == (SVM_EVTINJ_VALID | SVM_EVTINJ_TYPE_INTR);
}

static void skip_emulated_instruction(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	if (!svm->next_rip) {
		printk(KERN_DEBUG "%s: NOP\n", __FUNCTION__);
		return;
	}
	if (svm->next_rip - svm->vmcb->save.rip > MAX_INST_SIZE) {
		printk(KERN_ERR "%s: ip 0x%llx next 0x%llx\n",
		       __FUNCTION__,
		       svm->vmcb->save.rip,
		       svm->next_rip);
	}

	vcpu->rip = svm->vmcb->save.rip = svm->next_rip;
	svm->vmcb->control.int_state &= ~SVM_INTERRUPT_SHADOW_MASK;

	vcpu->interrupt_window_open = 1;
}

static int has_svm(void)
{
	uint32_t eax, ebx, ecx, edx;

	if (boot_cpu_data.x86_vendor != X86_VENDOR_AMD) {
		printk(KERN_INFO "has_svm: not amd\n");
		return 0;
	}

	cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	if (eax < SVM_CPUID_FUNC) {
		printk(KERN_INFO "has_svm: can't execute cpuid_8000000a\n");
		return 0;
	}

	cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	if (!(ecx & (1 << SVM_CPUID_FEATURE_SHIFT))) {
		printk(KERN_DEBUG "has_svm: svm not available\n");
		return 0;
	}
	return 1;
}

static void svm_hardware_disable(void *garbage)
{
	struct svm_cpu_data *svm_data
		= per_cpu(svm_data, raw_smp_processor_id());

	if (svm_data) {
		uint64_t efer;

		wrmsrl(MSR_VM_HSAVE_PA, 0);
		rdmsrl(MSR_EFER, efer);
		wrmsrl(MSR_EFER, efer & ~MSR_EFER_SVME_MASK);
		per_cpu(svm_data, raw_smp_processor_id()) = NULL;
		__free_page(svm_data->save_area);
		kfree(svm_data);
	}
}

static void svm_hardware_enable(void *garbage)
{

	struct svm_cpu_data *svm_data;
	uint64_t efer;
#ifdef CONFIG_X86_64
	struct desc_ptr gdt_descr;
#else
	struct Xgt_desc_struct gdt_descr;
#endif
	struct desc_struct *gdt;
	int me = raw_smp_processor_id();

	if (!has_svm()) {
		printk(KERN_ERR "svm_cpu_init: err EOPNOTSUPP on %d\n", me);
		return;
	}
	svm_data = per_cpu(svm_data, me);

	if (!svm_data) {
		printk(KERN_ERR "svm_cpu_init: svm_data is NULL on %d\n",
		       me);
		return;
	}

	svm_data->asid_generation = 1;
	svm_data->max_asid = cpuid_ebx(SVM_CPUID_FUNC) - 1;
	svm_data->next_asid = svm_data->max_asid + 1;
	svm_features = cpuid_edx(SVM_CPUID_FUNC);

	asm volatile ( "sgdt %0" : "=m"(gdt_descr) );
	gdt = (struct desc_struct *)gdt_descr.address;
	svm_data->tss_desc = (struct kvm_ldttss_desc *)(gdt + GDT_ENTRY_TSS);

	rdmsrl(MSR_EFER, efer);
	wrmsrl(MSR_EFER, efer | MSR_EFER_SVME_MASK);

	wrmsrl(MSR_VM_HSAVE_PA,
	       page_to_pfn(svm_data->save_area) << PAGE_SHIFT);
}

static int svm_cpu_init(int cpu)
{
	struct svm_cpu_data *svm_data;
	int r;

	svm_data = kzalloc(sizeof(struct svm_cpu_data), GFP_KERNEL);
	if (!svm_data)
		return -ENOMEM;
	svm_data->cpu = cpu;
	svm_data->save_area = alloc_page(GFP_KERNEL);
	r = -ENOMEM;
	if (!svm_data->save_area)
		goto err_1;

	per_cpu(svm_data, cpu) = svm_data;

	return 0;

err_1:
	kfree(svm_data);
	return r;

}

static void set_msr_interception(u32 *msrpm, unsigned msr,
				 int read, int write)
{
	int i;

	for (i = 0; i < NUM_MSR_MAPS; i++) {
		if (msr >= msrpm_ranges[i] &&
		    msr < msrpm_ranges[i] + MSRS_IN_RANGE) {
			u32 msr_offset = (i * MSRS_IN_RANGE + msr -
					  msrpm_ranges[i]) * 2;

			u32 *base = msrpm + (msr_offset / 32);
			u32 msr_shift = msr_offset % 32;
			u32 mask = ((write) ? 0 : 2) | ((read) ? 0 : 1);
			*base = (*base & ~(0x3 << msr_shift)) |
				(mask << msr_shift);
			return;
		}
	}
	BUG();
}

static __init int svm_hardware_setup(void)
{
	int cpu;
	struct page *iopm_pages;
	struct page *msrpm_pages;
	void *iopm_va, *msrpm_va;
	int r;

	iopm_pages = alloc_pages(GFP_KERNEL, IOPM_ALLOC_ORDER);

	if (!iopm_pages)
		return -ENOMEM;

	iopm_va = page_address(iopm_pages);
	memset(iopm_va, 0xff, PAGE_SIZE * (1 << IOPM_ALLOC_ORDER));
	clear_bit(0x80, iopm_va); /* allow direct access to PC debug port */
	iopm_base = page_to_pfn(iopm_pages) << PAGE_SHIFT;


	msrpm_pages = alloc_pages(GFP_KERNEL, MSRPM_ALLOC_ORDER);

	r = -ENOMEM;
	if (!msrpm_pages)
		goto err_1;

	msrpm_va = page_address(msrpm_pages);
	memset(msrpm_va, 0xff, PAGE_SIZE * (1 << MSRPM_ALLOC_ORDER));
	msrpm_base = page_to_pfn(msrpm_pages) << PAGE_SHIFT;

#ifdef CONFIG_X86_64
	set_msr_interception(msrpm_va, MSR_GS_BASE, 1, 1);
	set_msr_interception(msrpm_va, MSR_FS_BASE, 1, 1);
	set_msr_interception(msrpm_va, MSR_KERNEL_GS_BASE, 1, 1);
	set_msr_interception(msrpm_va, MSR_LSTAR, 1, 1);
	set_msr_interception(msrpm_va, MSR_CSTAR, 1, 1);
	set_msr_interception(msrpm_va, MSR_SYSCALL_MASK, 1, 1);
#endif
	set_msr_interception(msrpm_va, MSR_K6_STAR, 1, 1);
	set_msr_interception(msrpm_va, MSR_IA32_SYSENTER_CS, 1, 1);
	set_msr_interception(msrpm_va, MSR_IA32_SYSENTER_ESP, 1, 1);
	set_msr_interception(msrpm_va, MSR_IA32_SYSENTER_EIP, 1, 1);

	for_each_online_cpu(cpu) {
		r = svm_cpu_init(cpu);
		if (r)
			goto err_2;
	}
	return 0;

err_2:
	__free_pages(msrpm_pages, MSRPM_ALLOC_ORDER);
	msrpm_base = 0;
err_1:
	__free_pages(iopm_pages, IOPM_ALLOC_ORDER);
	iopm_base = 0;
	return r;
}

static __exit void svm_hardware_unsetup(void)
{
	__free_pages(pfn_to_page(msrpm_base >> PAGE_SHIFT), MSRPM_ALLOC_ORDER);
	__free_pages(pfn_to_page(iopm_base >> PAGE_SHIFT), IOPM_ALLOC_ORDER);
	iopm_base = msrpm_base = 0;
}

static void init_seg(struct vmcb_seg *seg)
{
	seg->selector = 0;
	seg->attrib = SVM_SELECTOR_P_MASK | SVM_SELECTOR_S_MASK |
		SVM_SELECTOR_WRITE_MASK; /* Read/Write Data Segment */
	seg->limit = 0xffff;
	seg->base = 0;
}

static void init_sys_seg(struct vmcb_seg *seg, uint32_t type)
{
	seg->selector = 0;
	seg->attrib = SVM_SELECTOR_P_MASK | type;
	seg->limit = 0xffff;
	seg->base = 0;
}

static void init_vmcb(struct vmcb *vmcb)
{
	struct vmcb_control_area *control = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

	control->intercept_cr_read = 	INTERCEPT_CR0_MASK |
					INTERCEPT_CR3_MASK |
					INTERCEPT_CR4_MASK;

	control->intercept_cr_write = 	INTERCEPT_CR0_MASK |
					INTERCEPT_CR3_MASK |
					INTERCEPT_CR4_MASK;

	control->intercept_dr_read = 	INTERCEPT_DR0_MASK |
					INTERCEPT_DR1_MASK |
					INTERCEPT_DR2_MASK |
					INTERCEPT_DR3_MASK;

	control->intercept_dr_write = 	INTERCEPT_DR0_MASK |
					INTERCEPT_DR1_MASK |
					INTERCEPT_DR2_MASK |
					INTERCEPT_DR3_MASK |
					INTERCEPT_DR5_MASK |
					INTERCEPT_DR7_MASK;

	control->intercept_exceptions = 1 << PF_VECTOR;


	control->intercept = 	(1ULL << INTERCEPT_INTR) |
				(1ULL << INTERCEPT_NMI) |
				(1ULL << INTERCEPT_SMI) |
		/*
		 * selective cr0 intercept bug?
		 *    	0:   0f 22 d8                mov    %eax,%cr3
		 *	3:   0f 20 c0                mov    %cr0,%eax
		 *	6:   0d 00 00 00 80          or     $0x80000000,%eax
		 *	b:   0f 22 c0                mov    %eax,%cr0
		 * set cr3 ->interception
		 * get cr0 ->interception
		 * set cr0 -> no interception
		 */
		/*              (1ULL << INTERCEPT_SELECTIVE_CR0) | */
				(1ULL << INTERCEPT_CPUID) |
				(1ULL << INTERCEPT_INVD) |
				(1ULL << INTERCEPT_HLT) |
				(1ULL << INTERCEPT_INVLPGA) |
				(1ULL << INTERCEPT_IOIO_PROT) |
				(1ULL << INTERCEPT_MSR_PROT) |
				(1ULL << INTERCEPT_TASK_SWITCH) |
				(1ULL << INTERCEPT_SHUTDOWN) |
				(1ULL << INTERCEPT_VMRUN) |
				(1ULL << INTERCEPT_VMMCALL) |
				(1ULL << INTERCEPT_VMLOAD) |
				(1ULL << INTERCEPT_VMSAVE) |
				(1ULL << INTERCEPT_STGI) |
				(1ULL << INTERCEPT_CLGI) |
				(1ULL << INTERCEPT_SKINIT) |
				(1ULL << INTERCEPT_WBINVD) |
				(1ULL << INTERCEPT_MONITOR) |
				(1ULL << INTERCEPT_MWAIT);

	control->iopm_base_pa = iopm_base;
	control->msrpm_base_pa = msrpm_base;
	control->tsc_offset = 0;
	control->int_ctl = V_INTR_MASKING_MASK;

	init_seg(&save->es);
	init_seg(&save->ss);
	init_seg(&save->ds);
	init_seg(&save->fs);
	init_seg(&save->gs);

	save->cs.selector = 0xf000;
	/* Executable/Readable Code Segment */
	save->cs.attrib = SVM_SELECTOR_READ_MASK | SVM_SELECTOR_P_MASK |
		SVM_SELECTOR_S_MASK | SVM_SELECTOR_CODE_MASK;
	save->cs.limit = 0xffff;
	/*
	 * cs.base should really be 0xffff0000, but vmx can't handle that, so
	 * be consistent with it.
	 *
	 * Replace when we have real mode working for vmx.
	 */
	save->cs.base = 0xf0000;

	save->gdtr.limit = 0xffff;
	save->idtr.limit = 0xffff;

	init_sys_seg(&save->ldtr, SEG_TYPE_LDT);
	init_sys_seg(&save->tr, SEG_TYPE_BUSY_TSS16);

	save->efer = MSR_EFER_SVME_MASK;

        save->dr6 = 0xffff0ff0;
	save->dr7 = 0x400;
	save->rflags = 2;
	save->rip = 0x0000fff0;

	/*
	 * cr0 val on cpu init should be 0x60000010, we enable cpu
	 * cache by default. the orderly way is to enable cache in bios.
	 */
	save->cr0 = 0x00000010 | X86_CR0_PG | X86_CR0_WP;
	save->cr4 = X86_CR4_PAE;
	/* rdx = ?? */
}

static void svm_vcpu_reset(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	init_vmcb(svm->vmcb);

	if (vcpu->vcpu_id != 0) {
		svm->vmcb->save.rip = 0;
		svm->vmcb->save.cs.base = svm->vcpu.sipi_vector << 12;
		svm->vmcb->save.cs.selector = svm->vcpu.sipi_vector << 8;
	}
}

static struct kvm_vcpu *svm_create_vcpu(struct kvm *kvm, unsigned int id)
{
	struct vcpu_svm *svm;
	struct page *page;
	int err;

	svm = kmem_cache_zalloc(kvm_vcpu_cache, GFP_KERNEL);
	if (!svm) {
		err = -ENOMEM;
		goto out;
	}

	err = kvm_vcpu_init(&svm->vcpu, kvm, id);
	if (err)
		goto free_svm;

	if (irqchip_in_kernel(kvm)) {
		err = kvm_create_lapic(&svm->vcpu);
		if (err < 0)
			goto free_svm;
	}

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		err = -ENOMEM;
		goto uninit;
	}

	svm->vmcb = page_address(page);
	clear_page(svm->vmcb);
	svm->vmcb_pa = page_to_pfn(page) << PAGE_SHIFT;
	svm->asid_generation = 0;
	memset(svm->db_regs, 0, sizeof(svm->db_regs));
	init_vmcb(svm->vmcb);

	fx_init(&svm->vcpu);
	svm->vcpu.fpu_active = 1;
	svm->vcpu.apic_base = 0xfee00000 | MSR_IA32_APICBASE_ENABLE;
	if (svm->vcpu.vcpu_id == 0)
		svm->vcpu.apic_base |= MSR_IA32_APICBASE_BSP;

	return &svm->vcpu;

uninit:
	kvm_vcpu_uninit(&svm->vcpu);
free_svm:
	kmem_cache_free(kvm_vcpu_cache, svm);
out:
	return ERR_PTR(err);
}

static void svm_free_vcpu(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	__free_page(pfn_to_page(svm->vmcb_pa >> PAGE_SHIFT));
	kvm_vcpu_uninit(vcpu);
	kmem_cache_free(kvm_vcpu_cache, svm);
}

static void svm_vcpu_load(struct kvm_vcpu *vcpu, int cpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	int i;

	if (unlikely(cpu != vcpu->cpu)) {
		u64 tsc_this, delta;

		/*
		 * Make sure that the guest sees a monotonically
		 * increasing TSC.
		 */
		rdtscll(tsc_this);
		delta = vcpu->host_tsc - tsc_this;
		svm->vmcb->control.tsc_offset += delta;
		vcpu->cpu = cpu;
		kvm_migrate_apic_timer(vcpu);
	}

	for (i = 0; i < NR_HOST_SAVE_USER_MSRS; i++)
		rdmsrl(host_save_user_msrs[i], svm->host_user_msrs[i]);
}

static void svm_vcpu_put(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	int i;

	for (i = 0; i < NR_HOST_SAVE_USER_MSRS; i++)
		wrmsrl(host_save_user_msrs[i], svm->host_user_msrs[i]);

	rdtscll(vcpu->host_tsc);
	kvm_put_guest_fpu(vcpu);
}

static void svm_vcpu_decache(struct kvm_vcpu *vcpu)
{
}

static void svm_cache_regs(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	vcpu->regs[VCPU_REGS_RAX] = svm->vmcb->save.rax;
	vcpu->regs[VCPU_REGS_RSP] = svm->vmcb->save.rsp;
	vcpu->rip = svm->vmcb->save.rip;
}

static void svm_decache_regs(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	svm->vmcb->save.rax = vcpu->regs[VCPU_REGS_RAX];
	svm->vmcb->save.rsp = vcpu->regs[VCPU_REGS_RSP];
	svm->vmcb->save.rip = vcpu->rip;
}

static unsigned long svm_get_rflags(struct kvm_vcpu *vcpu)
{
	return to_svm(vcpu)->vmcb->save.rflags;
}

static void svm_set_rflags(struct kvm_vcpu *vcpu, unsigned long rflags)
{
	to_svm(vcpu)->vmcb->save.rflags = rflags;
}

static struct vmcb_seg *svm_seg(struct kvm_vcpu *vcpu, int seg)
{
	struct vmcb_save_area *save = &to_svm(vcpu)->vmcb->save;

	switch (seg) {
	case VCPU_SREG_CS: return &save->cs;
	case VCPU_SREG_DS: return &save->ds;
	case VCPU_SREG_ES: return &save->es;
	case VCPU_SREG_FS: return &save->fs;
	case VCPU_SREG_GS: return &save->gs;
	case VCPU_SREG_SS: return &save->ss;
	case VCPU_SREG_TR: return &save->tr;
	case VCPU_SREG_LDTR: return &save->ldtr;
	}
	BUG();
	return NULL;
}

static u64 svm_get_segment_base(struct kvm_vcpu *vcpu, int seg)
{
	struct vmcb_seg *s = svm_seg(vcpu, seg);

	return s->base;
}

static void svm_get_segment(struct kvm_vcpu *vcpu,
			    struct kvm_segment *var, int seg)
{
	struct vmcb_seg *s = svm_seg(vcpu, seg);

	var->base = s->base;
	var->limit = s->limit;
	var->selector = s->selector;
	var->type = s->attrib & SVM_SELECTOR_TYPE_MASK;
	var->s = (s->attrib >> SVM_SELECTOR_S_SHIFT) & 1;
	var->dpl = (s->attrib >> SVM_SELECTOR_DPL_SHIFT) & 3;
	var->present = (s->attrib >> SVM_SELECTOR_P_SHIFT) & 1;
	var->avl = (s->attrib >> SVM_SELECTOR_AVL_SHIFT) & 1;
	var->l = (s->attrib >> SVM_SELECTOR_L_SHIFT) & 1;
	var->db = (s->attrib >> SVM_SELECTOR_DB_SHIFT) & 1;
	var->g = (s->attrib >> SVM_SELECTOR_G_SHIFT) & 1;
	var->unusable = !var->present;
}

static void svm_get_idt(struct kvm_vcpu *vcpu, struct descriptor_table *dt)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	dt->limit = svm->vmcb->save.idtr.limit;
	dt->base = svm->vmcb->save.idtr.base;
}

static void svm_set_idt(struct kvm_vcpu *vcpu, struct descriptor_table *dt)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	svm->vmcb->save.idtr.limit = dt->limit;
	svm->vmcb->save.idtr.base = dt->base ;
}

static void svm_get_gdt(struct kvm_vcpu *vcpu, struct descriptor_table *dt)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	dt->limit = svm->vmcb->save.gdtr.limit;
	dt->base = svm->vmcb->save.gdtr.base;
}

static void svm_set_gdt(struct kvm_vcpu *vcpu, struct descriptor_table *dt)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	svm->vmcb->save.gdtr.limit = dt->limit;
	svm->vmcb->save.gdtr.base = dt->base ;
}

static void svm_decache_cr4_guest_bits(struct kvm_vcpu *vcpu)
{
}

static void svm_set_cr0(struct kvm_vcpu *vcpu, unsigned long cr0)
{
	struct vcpu_svm *svm = to_svm(vcpu);

#ifdef CONFIG_X86_64
	if (vcpu->shadow_efer & KVM_EFER_LME) {
		if (!is_paging(vcpu) && (cr0 & X86_CR0_PG)) {
			vcpu->shadow_efer |= KVM_EFER_LMA;
			svm->vmcb->save.efer |= KVM_EFER_LMA | KVM_EFER_LME;
		}

		if (is_paging(vcpu) && !(cr0 & X86_CR0_PG) ) {
			vcpu->shadow_efer &= ~KVM_EFER_LMA;
			svm->vmcb->save.efer &= ~(KVM_EFER_LMA | KVM_EFER_LME);
		}
	}
#endif
	if ((vcpu->cr0 & X86_CR0_TS) && !(cr0 & X86_CR0_TS)) {
		svm->vmcb->control.intercept_exceptions &= ~(1 << NM_VECTOR);
		vcpu->fpu_active = 1;
	}

	vcpu->cr0 = cr0;
	cr0 |= X86_CR0_PG | X86_CR0_WP;
	cr0 &= ~(X86_CR0_CD | X86_CR0_NW);
	svm->vmcb->save.cr0 = cr0;
}

static void svm_set_cr4(struct kvm_vcpu *vcpu, unsigned long cr4)
{
       vcpu->cr4 = cr4;
       to_svm(vcpu)->vmcb->save.cr4 = cr4 | X86_CR4_PAE;
}

static void svm_set_segment(struct kvm_vcpu *vcpu,
			    struct kvm_segment *var, int seg)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	struct vmcb_seg *s = svm_seg(vcpu, seg);

	s->base = var->base;
	s->limit = var->limit;
	s->selector = var->selector;
	if (var->unusable)
		s->attrib = 0;
	else {
		s->attrib = (var->type & SVM_SELECTOR_TYPE_MASK);
		s->attrib |= (var->s & 1) << SVM_SELECTOR_S_SHIFT;
		s->attrib |= (var->dpl & 3) << SVM_SELECTOR_DPL_SHIFT;
		s->attrib |= (var->present & 1) << SVM_SELECTOR_P_SHIFT;
		s->attrib |= (var->avl & 1) << SVM_SELECTOR_AVL_SHIFT;
		s->attrib |= (var->l & 1) << SVM_SELECTOR_L_SHIFT;
		s->attrib |= (var->db & 1) << SVM_SELECTOR_DB_SHIFT;
		s->attrib |= (var->g & 1) << SVM_SELECTOR_G_SHIFT;
	}
	if (seg == VCPU_SREG_CS)
		svm->vmcb->save.cpl
			= (svm->vmcb->save.cs.attrib
			   >> SVM_SELECTOR_DPL_SHIFT) & 3;

}

/* FIXME:

	svm(vcpu)->vmcb->control.int_ctl &= ~V_TPR_MASK;
	svm(vcpu)->vmcb->control.int_ctl |= (sregs->cr8 & V_TPR_MASK);

*/

static int svm_guest_debug(struct kvm_vcpu *vcpu, struct kvm_debug_guest *dbg)
{
	return -EOPNOTSUPP;
}

static int svm_get_irq(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u32 exit_int_info = svm->vmcb->control.exit_int_info;

	if (is_external_interrupt(exit_int_info))
		return exit_int_info & SVM_EVTINJ_VEC_MASK;
	return -1;
}

static void load_host_msrs(struct kvm_vcpu *vcpu)
{
#ifdef CONFIG_X86_64
	wrmsrl(MSR_GS_BASE, to_svm(vcpu)->host_gs_base);
#endif
}

static void save_host_msrs(struct kvm_vcpu *vcpu)
{
#ifdef CONFIG_X86_64
	rdmsrl(MSR_GS_BASE, to_svm(vcpu)->host_gs_base);
#endif
}

static void new_asid(struct vcpu_svm *svm, struct svm_cpu_data *svm_data)
{
	if (svm_data->next_asid > svm_data->max_asid) {
		++svm_data->asid_generation;
		svm_data->next_asid = 1;
		svm->vmcb->control.tlb_ctl = TLB_CONTROL_FLUSH_ALL_ASID;
	}

	svm->vcpu.cpu = svm_data->cpu;
	svm->asid_generation = svm_data->asid_generation;
	svm->vmcb->control.asid = svm_data->next_asid++;
}

static unsigned long svm_get_dr(struct kvm_vcpu *vcpu, int dr)
{
	return to_svm(vcpu)->db_regs[dr];
}

static void svm_set_dr(struct kvm_vcpu *vcpu, int dr, unsigned long value,
		       int *exception)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	*exception = 0;

	if (svm->vmcb->save.dr7 & DR7_GD_MASK) {
		svm->vmcb->save.dr7 &= ~DR7_GD_MASK;
		svm->vmcb->save.dr6 |= DR6_BD_MASK;
		*exception = DB_VECTOR;
		return;
	}

	switch (dr) {
	case 0 ... 3:
		svm->db_regs[dr] = value;
		return;
	case 4 ... 5:
		if (vcpu->cr4 & X86_CR4_DE) {
			*exception = UD_VECTOR;
			return;
		}
	case 7: {
		if (value & ~((1ULL << 32) - 1)) {
			*exception = GP_VECTOR;
			return;
		}
		svm->vmcb->save.dr7 = value;
		return;
	}
	default:
		printk(KERN_DEBUG "%s: unexpected dr %u\n",
		       __FUNCTION__, dr);
		*exception = UD_VECTOR;
		return;
	}
}

static int pf_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	u32 exit_int_info = svm->vmcb->control.exit_int_info;
	struct kvm *kvm = svm->vcpu.kvm;
	u64 fault_address;
	u32 error_code;
	enum emulation_result er;
	int r;

	if (!irqchip_in_kernel(kvm) &&
		is_external_interrupt(exit_int_info))
		push_irq(&svm->vcpu, exit_int_info & SVM_EVTINJ_VEC_MASK);

	mutex_lock(&kvm->lock);

	fault_address  = svm->vmcb->control.exit_info_2;
	error_code = svm->vmcb->control.exit_info_1;
	r = kvm_mmu_page_fault(&svm->vcpu, fault_address, error_code);
	if (r < 0) {
		mutex_unlock(&kvm->lock);
		return r;
	}
	if (!r) {
		mutex_unlock(&kvm->lock);
		return 1;
	}
	er = emulate_instruction(&svm->vcpu, kvm_run, fault_address,
				 error_code);
	mutex_unlock(&kvm->lock);

	switch (er) {
	case EMULATE_DONE:
		return 1;
	case EMULATE_DO_MMIO:
		++svm->vcpu.stat.mmio_exits;
		return 0;
	case EMULATE_FAIL:
		kvm_report_emulation_failure(&svm->vcpu, "pagetable");
		break;
	default:
		BUG();
	}

	kvm_run->exit_reason = KVM_EXIT_UNKNOWN;
	return 0;
}

static int nm_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	svm->vmcb->control.intercept_exceptions &= ~(1 << NM_VECTOR);
	if (!(svm->vcpu.cr0 & X86_CR0_TS))
		svm->vmcb->save.cr0 &= ~X86_CR0_TS;
	svm->vcpu.fpu_active = 1;

	return 1;
}

static int shutdown_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	/*
	 * VMCB is undefined after a SHUTDOWN intercept
	 * so reinitialize it.
	 */
	clear_page(svm->vmcb);
	init_vmcb(svm->vmcb);

	kvm_run->exit_reason = KVM_EXIT_SHUTDOWN;
	return 0;
}

static int io_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	u32 io_info = svm->vmcb->control.exit_info_1; //address size bug?
	int size, down, in, string, rep;
	unsigned port;

	++svm->vcpu.stat.io_exits;

	svm->next_rip = svm->vmcb->control.exit_info_2;

	string = (io_info & SVM_IOIO_STR_MASK) != 0;

	if (string) {
		if (emulate_instruction(&svm->vcpu, kvm_run, 0, 0) == EMULATE_DO_MMIO)
			return 0;
		return 1;
	}

	in = (io_info & SVM_IOIO_TYPE_MASK) != 0;
	port = io_info >> 16;
	size = (io_info & SVM_IOIO_SIZE_MASK) >> SVM_IOIO_SIZE_SHIFT;
	rep = (io_info & SVM_IOIO_REP_MASK) != 0;
	down = (svm->vmcb->save.rflags & X86_EFLAGS_DF) != 0;

	return kvm_emulate_pio(&svm->vcpu, kvm_run, in, size, port);
}

static int nop_on_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	return 1;
}

static int halt_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	svm->next_rip = svm->vmcb->save.rip + 1;
	skip_emulated_instruction(&svm->vcpu);
	return kvm_emulate_halt(&svm->vcpu);
}

static int vmmcall_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	svm->next_rip = svm->vmcb->save.rip + 3;
	skip_emulated_instruction(&svm->vcpu);
	return kvm_hypercall(&svm->vcpu, kvm_run);
}

static int invalid_op_interception(struct vcpu_svm *svm,
				   struct kvm_run *kvm_run)
{
	inject_ud(&svm->vcpu);
	return 1;
}

static int task_switch_interception(struct vcpu_svm *svm,
				    struct kvm_run *kvm_run)
{
	pr_unimpl(&svm->vcpu, "%s: task switch is unsupported\n", __FUNCTION__);
	kvm_run->exit_reason = KVM_EXIT_UNKNOWN;
	return 0;
}

static int cpuid_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	svm->next_rip = svm->vmcb->save.rip + 2;
	kvm_emulate_cpuid(&svm->vcpu);
	return 1;
}

static int emulate_on_interception(struct vcpu_svm *svm,
				   struct kvm_run *kvm_run)
{
	if (emulate_instruction(&svm->vcpu, NULL, 0, 0) != EMULATE_DONE)
		pr_unimpl(&svm->vcpu, "%s: failed\n", __FUNCTION__);
	return 1;
}

static int svm_get_msr(struct kvm_vcpu *vcpu, unsigned ecx, u64 *data)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	switch (ecx) {
	case MSR_IA32_TIME_STAMP_COUNTER: {
		u64 tsc;

		rdtscll(tsc);
		*data = svm->vmcb->control.tsc_offset + tsc;
		break;
	}
	case MSR_K6_STAR:
		*data = svm->vmcb->save.star;
		break;
#ifdef CONFIG_X86_64
	case MSR_LSTAR:
		*data = svm->vmcb->save.lstar;
		break;
	case MSR_CSTAR:
		*data = svm->vmcb->save.cstar;
		break;
	case MSR_KERNEL_GS_BASE:
		*data = svm->vmcb->save.kernel_gs_base;
		break;
	case MSR_SYSCALL_MASK:
		*data = svm->vmcb->save.sfmask;
		break;
#endif
	case MSR_IA32_SYSENTER_CS:
		*data = svm->vmcb->save.sysenter_cs;
		break;
	case MSR_IA32_SYSENTER_EIP:
		*data = svm->vmcb->save.sysenter_eip;
		break;
	case MSR_IA32_SYSENTER_ESP:
		*data = svm->vmcb->save.sysenter_esp;
		break;
	default:
		return kvm_get_msr_common(vcpu, ecx, data);
	}
	return 0;
}

static int rdmsr_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	u32 ecx = svm->vcpu.regs[VCPU_REGS_RCX];
	u64 data;

	if (svm_get_msr(&svm->vcpu, ecx, &data))
		svm_inject_gp(&svm->vcpu, 0);
	else {
		svm->vmcb->save.rax = data & 0xffffffff;
		svm->vcpu.regs[VCPU_REGS_RDX] = data >> 32;
		svm->next_rip = svm->vmcb->save.rip + 2;
		skip_emulated_instruction(&svm->vcpu);
	}
	return 1;
}

static int svm_set_msr(struct kvm_vcpu *vcpu, unsigned ecx, u64 data)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	switch (ecx) {
	case MSR_IA32_TIME_STAMP_COUNTER: {
		u64 tsc;

		rdtscll(tsc);
		svm->vmcb->control.tsc_offset = data - tsc;
		break;
	}
	case MSR_K6_STAR:
		svm->vmcb->save.star = data;
		break;
#ifdef CONFIG_X86_64
	case MSR_LSTAR:
		svm->vmcb->save.lstar = data;
		break;
	case MSR_CSTAR:
		svm->vmcb->save.cstar = data;
		break;
	case MSR_KERNEL_GS_BASE:
		svm->vmcb->save.kernel_gs_base = data;
		break;
	case MSR_SYSCALL_MASK:
		svm->vmcb->save.sfmask = data;
		break;
#endif
	case MSR_IA32_SYSENTER_CS:
		svm->vmcb->save.sysenter_cs = data;
		break;
	case MSR_IA32_SYSENTER_EIP:
		svm->vmcb->save.sysenter_eip = data;
		break;
	case MSR_IA32_SYSENTER_ESP:
		svm->vmcb->save.sysenter_esp = data;
		break;
	default:
		return kvm_set_msr_common(vcpu, ecx, data);
	}
	return 0;
}

static int wrmsr_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	u32 ecx = svm->vcpu.regs[VCPU_REGS_RCX];
	u64 data = (svm->vmcb->save.rax & -1u)
		| ((u64)(svm->vcpu.regs[VCPU_REGS_RDX] & -1u) << 32);
	svm->next_rip = svm->vmcb->save.rip + 2;
	if (svm_set_msr(&svm->vcpu, ecx, data))
		svm_inject_gp(&svm->vcpu, 0);
	else
		skip_emulated_instruction(&svm->vcpu);
	return 1;
}

static int msr_interception(struct vcpu_svm *svm, struct kvm_run *kvm_run)
{
	if (svm->vmcb->control.exit_info_1)
		return wrmsr_interception(svm, kvm_run);
	else
		return rdmsr_interception(svm, kvm_run);
}

static int interrupt_window_interception(struct vcpu_svm *svm,
				   struct kvm_run *kvm_run)
{
	svm->vmcb->control.intercept &= ~(1ULL << INTERCEPT_VINTR);
	svm->vmcb->control.int_ctl &= ~V_IRQ_MASK;
	/*
	 * If the user space waits to inject interrupts, exit as soon as
	 * possible
	 */
	if (kvm_run->request_interrupt_window &&
	    !svm->vcpu.irq_summary) {
		++svm->vcpu.stat.irq_window_exits;
		kvm_run->exit_reason = KVM_EXIT_IRQ_WINDOW_OPEN;
		return 0;
	}

	return 1;
}

static int (*svm_exit_handlers[])(struct vcpu_svm *svm,
				      struct kvm_run *kvm_run) = {
	[SVM_EXIT_READ_CR0]           		= emulate_on_interception,
	[SVM_EXIT_READ_CR3]           		= emulate_on_interception,
	[SVM_EXIT_READ_CR4]           		= emulate_on_interception,
	/* for now: */
	[SVM_EXIT_WRITE_CR0]          		= emulate_on_interception,
	[SVM_EXIT_WRITE_CR3]          		= emulate_on_interception,
	[SVM_EXIT_WRITE_CR4]          		= emulate_on_interception,
	[SVM_EXIT_READ_DR0] 			= emulate_on_interception,
	[SVM_EXIT_READ_DR1]			= emulate_on_interception,
	[SVM_EXIT_READ_DR2]			= emulate_on_interception,
	[SVM_EXIT_READ_DR3]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR0]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR1]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR2]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR3]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR5]			= emulate_on_interception,
	[SVM_EXIT_WRITE_DR7]			= emulate_on_interception,
	[SVM_EXIT_EXCP_BASE + PF_VECTOR] 	= pf_interception,
	[SVM_EXIT_EXCP_BASE + NM_VECTOR] 	= nm_interception,
	[SVM_EXIT_INTR] 			= nop_on_interception,
	[SVM_EXIT_NMI]				= nop_on_interception,
	[SVM_EXIT_SMI]				= nop_on_interception,
	[SVM_EXIT_INIT]				= nop_on_interception,
	[SVM_EXIT_VINTR]			= interrupt_window_interception,
	/* [SVM_EXIT_CR0_SEL_WRITE]		= emulate_on_interception, */
	[SVM_EXIT_CPUID]			= cpuid_interception,
	[SVM_EXIT_INVD]                         = emulate_on_interception,
	[SVM_EXIT_HLT]				= halt_interception,
	[SVM_EXIT_INVLPG]			= emulate_on_interception,
	[SVM_EXIT_INVLPGA]			= invalid_op_interception,
	[SVM_EXIT_IOIO] 		  	= io_interception,
	[SVM_EXIT_MSR]				= msr_interception,
	[SVM_EXIT_TASK_SWITCH]			= task_switch_interception,
	[SVM_EXIT_SHUTDOWN]			= shutdown_interception,
	[SVM_EXIT_VMRUN]			= invalid_op_interception,
	[SVM_EXIT_VMMCALL]			= vmmcall_interception,
	[SVM_EXIT_VMLOAD]			= invalid_op_interception,
	[SVM_EXIT_VMSAVE]			= invalid_op_interception,
	[SVM_EXIT_STGI]				= invalid_op_interception,
	[SVM_EXIT_CLGI]				= invalid_op_interception,
	[SVM_EXIT_SKINIT]			= invalid_op_interception,
	[SVM_EXIT_WBINVD]                       = emulate_on_interception,
	[SVM_EXIT_MONITOR]			= invalid_op_interception,
	[SVM_EXIT_MWAIT]			= invalid_op_interception,
};


static int handle_exit(struct kvm_run *kvm_run, struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u32 exit_code = svm->vmcb->control.exit_code;

	kvm_reput_irq(svm);

	if (svm->vmcb->control.exit_code == SVM_EXIT_ERR) {
		kvm_run->exit_reason = KVM_EXIT_FAIL_ENTRY;
		kvm_run->fail_entry.hardware_entry_failure_reason
			= svm->vmcb->control.exit_code;
		return 0;
	}

	if (is_external_interrupt(svm->vmcb->control.exit_int_info) &&
	    exit_code != SVM_EXIT_EXCP_BASE + PF_VECTOR)
		printk(KERN_ERR "%s: unexpected exit_ini_info 0x%x "
		       "exit_code 0x%x\n",
		       __FUNCTION__, svm->vmcb->control.exit_int_info,
		       exit_code);

	if (exit_code >= ARRAY_SIZE(svm_exit_handlers)
	    || svm_exit_handlers[exit_code] == 0) {
		kvm_run->exit_reason = KVM_EXIT_UNKNOWN;
		kvm_run->hw.hardware_exit_reason = exit_code;
		return 0;
	}

	return svm_exit_handlers[exit_code](svm, kvm_run);
}

static void reload_tss(struct kvm_vcpu *vcpu)
{
	int cpu = raw_smp_processor_id();

	struct svm_cpu_data *svm_data = per_cpu(svm_data, cpu);
	svm_data->tss_desc->type = 9; //available 32/64-bit TSS
	load_TR_desc();
}

static void pre_svm_run(struct vcpu_svm *svm)
{
	int cpu = raw_smp_processor_id();

	struct svm_cpu_data *svm_data = per_cpu(svm_data, cpu);

	svm->vmcb->control.tlb_ctl = TLB_CONTROL_DO_NOTHING;
	if (svm->vcpu.cpu != cpu ||
	    svm->asid_generation != svm_data->asid_generation)
		new_asid(svm, svm_data);
}


static inline void svm_inject_irq(struct vcpu_svm *svm, int irq)
{
	struct vmcb_control_area *control;

	control = &svm->vmcb->control;
	control->int_vector = irq;
	control->int_ctl &= ~V_INTR_PRIO_MASK;
	control->int_ctl |= V_IRQ_MASK |
		((/*control->int_vector >> 4*/ 0xf) << V_INTR_PRIO_SHIFT);
}

static void svm_set_irq(struct kvm_vcpu *vcpu, int irq)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	svm_inject_irq(svm, irq);
}

static void svm_intr_assist(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	struct vmcb *vmcb = svm->vmcb;
	int intr_vector = -1;

	kvm_inject_pending_timer_irqs(vcpu);
	if ((vmcb->control.exit_int_info & SVM_EVTINJ_VALID) &&
	    ((vmcb->control.exit_int_info & SVM_EVTINJ_TYPE_MASK) == 0)) {
		intr_vector = vmcb->control.exit_int_info &
			      SVM_EVTINJ_VEC_MASK;
		vmcb->control.exit_int_info = 0;
		svm_inject_irq(svm, intr_vector);
		return;
	}

	if (vmcb->control.int_ctl & V_IRQ_MASK)
		return;

	if (!kvm_cpu_has_interrupt(vcpu))
		return;

	if (!(vmcb->save.rflags & X86_EFLAGS_IF) ||
	    (vmcb->control.int_state & SVM_INTERRUPT_SHADOW_MASK) ||
	    (vmcb->control.event_inj & SVM_EVTINJ_VALID)) {
		/* unable to deliver irq, set pending irq */
		vmcb->control.intercept |= (1ULL << INTERCEPT_VINTR);
		svm_inject_irq(svm, 0x0);
		return;
	}
	/* Okay, we can deliver the interrupt: grab it and update PIC state. */
	intr_vector = kvm_cpu_get_interrupt(vcpu);
	svm_inject_irq(svm, intr_vector);
	kvm_timer_intr_post(vcpu, intr_vector);
}

static void kvm_reput_irq(struct vcpu_svm *svm)
{
	struct vmcb_control_area *control = &svm->vmcb->control;

	if ((control->int_ctl & V_IRQ_MASK)
	    && !irqchip_in_kernel(svm->vcpu.kvm)) {
		control->int_ctl &= ~V_IRQ_MASK;
		push_irq(&svm->vcpu, control->int_vector);
	}

	svm->vcpu.interrupt_window_open =
		!(control->int_state & SVM_INTERRUPT_SHADOW_MASK);
}

static void svm_do_inject_vector(struct vcpu_svm *svm)
{
	struct kvm_vcpu *vcpu = &svm->vcpu;
	int word_index = __ffs(vcpu->irq_summary);
	int bit_index = __ffs(vcpu->irq_pending[word_index]);
	int irq = word_index * BITS_PER_LONG + bit_index;

	clear_bit(bit_index, &vcpu->irq_pending[word_index]);
	if (!vcpu->irq_pending[word_index])
		clear_bit(word_index, &vcpu->irq_summary);
	svm_inject_irq(svm, irq);
}

static void do_interrupt_requests(struct kvm_vcpu *vcpu,
				       struct kvm_run *kvm_run)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	struct vmcb_control_area *control = &svm->vmcb->control;

	svm->vcpu.interrupt_window_open =
		(!(control->int_state & SVM_INTERRUPT_SHADOW_MASK) &&
		 (svm->vmcb->save.rflags & X86_EFLAGS_IF));

	if (svm->vcpu.interrupt_window_open && svm->vcpu.irq_summary)
		/*
		 * If interrupts enabled, and not blocked by sti or mov ss. Good.
		 */
		svm_do_inject_vector(svm);

	/*
	 * Interrupts blocked.  Wait for unblock.
	 */
	if (!svm->vcpu.interrupt_window_open &&
	    (svm->vcpu.irq_summary || kvm_run->request_interrupt_window)) {
		control->intercept |= 1ULL << INTERCEPT_VINTR;
	} else
		control->intercept &= ~(1ULL << INTERCEPT_VINTR);
}

static void save_db_regs(unsigned long *db_regs)
{
	asm volatile ("mov %%dr0, %0" : "=r"(db_regs[0]));
	asm volatile ("mov %%dr1, %0" : "=r"(db_regs[1]));
	asm volatile ("mov %%dr2, %0" : "=r"(db_regs[2]));
	asm volatile ("mov %%dr3, %0" : "=r"(db_regs[3]));
}

static void load_db_regs(unsigned long *db_regs)
{
	asm volatile ("mov %0, %%dr0" : : "r"(db_regs[0]));
	asm volatile ("mov %0, %%dr1" : : "r"(db_regs[1]));
	asm volatile ("mov %0, %%dr2" : : "r"(db_regs[2]));
	asm volatile ("mov %0, %%dr3" : : "r"(db_regs[3]));
}

static void svm_flush_tlb(struct kvm_vcpu *vcpu)
{
	force_new_asid(vcpu);
}

static void svm_prepare_guest_switch(struct kvm_vcpu *vcpu)
{
}

static void svm_vcpu_run(struct kvm_vcpu *vcpu, struct kvm_run *kvm_run)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	u16 fs_selector;
	u16 gs_selector;
	u16 ldt_selector;

	pre_svm_run(svm);

	save_host_msrs(vcpu);
	fs_selector = read_fs();
	gs_selector = read_gs();
	ldt_selector = read_ldt();
	svm->host_cr2 = kvm_read_cr2();
	svm->host_dr6 = read_dr6();
	svm->host_dr7 = read_dr7();
	svm->vmcb->save.cr2 = vcpu->cr2;

	if (svm->vmcb->save.dr7 & 0xff) {
		write_dr7(0);
		save_db_regs(svm->host_db_regs);
		load_db_regs(svm->db_regs);
	}

	clgi();

	local_irq_enable();

	asm volatile (
#ifdef CONFIG_X86_64
		"push %%rbx; push %%rcx; push %%rdx;"
		"push %%rsi; push %%rdi; push %%rbp;"
		"push %%r8;  push %%r9;  push %%r10; push %%r11;"
		"push %%r12; push %%r13; push %%r14; push %%r15;"
#else
		"push %%ebx; push %%ecx; push %%edx;"
		"push %%esi; push %%edi; push %%ebp;"
#endif

#ifdef CONFIG_X86_64
		"mov %c[rbx](%[svm]), %%rbx \n\t"
		"mov %c[rcx](%[svm]), %%rcx \n\t"
		"mov %c[rdx](%[svm]), %%rdx \n\t"
		"mov %c[rsi](%[svm]), %%rsi \n\t"
		"mov %c[rdi](%[svm]), %%rdi \n\t"
		"mov %c[rbp](%[svm]), %%rbp \n\t"
		"mov %c[r8](%[svm]),  %%r8  \n\t"
		"mov %c[r9](%[svm]),  %%r9  \n\t"
		"mov %c[r10](%[svm]), %%r10 \n\t"
		"mov %c[r11](%[svm]), %%r11 \n\t"
		"mov %c[r12](%[svm]), %%r12 \n\t"
		"mov %c[r13](%[svm]), %%r13 \n\t"
		"mov %c[r14](%[svm]), %%r14 \n\t"
		"mov %c[r15](%[svm]), %%r15 \n\t"
#else
		"mov %c[rbx](%[svm]), %%ebx \n\t"
		"mov %c[rcx](%[svm]), %%ecx \n\t"
		"mov %c[rdx](%[svm]), %%edx \n\t"
		"mov %c[rsi](%[svm]), %%esi \n\t"
		"mov %c[rdi](%[svm]), %%edi \n\t"
		"mov %c[rbp](%[svm]), %%ebp \n\t"
#endif

#ifdef CONFIG_X86_64
		/* Enter guest mode */
		"push %%rax \n\t"
		"mov %c[vmcb](%[svm]), %%rax \n\t"
		SVM_VMLOAD "\n\t"
		SVM_VMRUN "\n\t"
		SVM_VMSAVE "\n\t"
		"pop %%rax \n\t"
#else
		/* Enter guest mode */
		"push %%eax \n\t"
		"mov %c[vmcb](%[svm]), %%eax \n\t"
		SVM_VMLOAD "\n\t"
		SVM_VMRUN "\n\t"
		SVM_VMSAVE "\n\t"
		"pop %%eax \n\t"
#endif

		/* Save guest registers, load host registers */
#ifdef CONFIG_X86_64
		"mov %%rbx, %c[rbx](%[svm]) \n\t"
		"mov %%rcx, %c[rcx](%[svm]) \n\t"
		"mov %%rdx, %c[rdx](%[svm]) \n\t"
		"mov %%rsi, %c[rsi](%[svm]) \n\t"
		"mov %%rdi, %c[rdi](%[svm]) \n\t"
		"mov %%rbp, %c[rbp](%[svm]) \n\t"
		"mov %%r8,  %c[r8](%[svm]) \n\t"
		"mov %%r9,  %c[r9](%[svm]) \n\t"
		"mov %%r10, %c[r10](%[svm]) \n\t"
		"mov %%r11, %c[r11](%[svm]) \n\t"
		"mov %%r12, %c[r12](%[svm]) \n\t"
		"mov %%r13, %c[r13](%[svm]) \n\t"
		"mov %%r14, %c[r14](%[svm]) \n\t"
		"mov %%r15, %c[r15](%[svm]) \n\t"

		"pop  %%r15; pop  %%r14; pop  %%r13; pop  %%r12;"
		"pop  %%r11; pop  %%r10; pop  %%r9;  pop  %%r8;"
		"pop  %%rbp; pop  %%rdi; pop  %%rsi;"
		"pop  %%rdx; pop  %%rcx; pop  %%rbx; \n\t"
#else
		"mov %%ebx, %c[rbx](%[svm]) \n\t"
		"mov %%ecx, %c[rcx](%[svm]) \n\t"
		"mov %%edx, %c[rdx](%[svm]) \n\t"
		"mov %%esi, %c[rsi](%[svm]) \n\t"
		"mov %%edi, %c[rdi](%[svm]) \n\t"
		"mov %%ebp, %c[rbp](%[svm]) \n\t"

		"pop  %%ebp; pop  %%edi; pop  %%esi;"
		"pop  %%edx; pop  %%ecx; pop  %%ebx; \n\t"
#endif
		:
		: [svm]"a"(svm),
		  [vmcb]"i"(offsetof(struct vcpu_svm, vmcb_pa)),
		  [rbx]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RBX])),
		  [rcx]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RCX])),
		  [rdx]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RDX])),
		  [rsi]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RSI])),
		  [rdi]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RDI])),
		  [rbp]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_RBP]))
#ifdef CONFIG_X86_64
		  ,[r8 ]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R8])),
		  [r9 ]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R9 ])),
		  [r10]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R10])),
		  [r11]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R11])),
		  [r12]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R12])),
		  [r13]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R13])),
		  [r14]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R14])),
		  [r15]"i"(offsetof(struct vcpu_svm,vcpu.regs[VCPU_REGS_R15]))
#endif
		: "cc", "memory" );

	if ((svm->vmcb->save.dr7 & 0xff))
		load_db_regs(svm->host_db_regs);

	vcpu->cr2 = svm->vmcb->save.cr2;

	write_dr6(svm->host_dr6);
	write_dr7(svm->host_dr7);
	kvm_write_cr2(svm->host_cr2);

	load_fs(fs_selector);
	load_gs(gs_selector);
	load_ldt(ldt_selector);
	load_host_msrs(vcpu);

	reload_tss(vcpu);

	local_irq_disable();

	stgi();

	svm->next_rip = 0;
}

static void svm_set_cr3(struct kvm_vcpu *vcpu, unsigned long root)
{
	struct vcpu_svm *svm = to_svm(vcpu);

	svm->vmcb->save.cr3 = root;
	force_new_asid(vcpu);

	if (vcpu->fpu_active) {
		svm->vmcb->control.intercept_exceptions |= (1 << NM_VECTOR);
		svm->vmcb->save.cr0 |= X86_CR0_TS;
		vcpu->fpu_active = 0;
	}
}

static void svm_inject_page_fault(struct kvm_vcpu *vcpu,
				  unsigned long  addr,
				  uint32_t err_code)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	uint32_t exit_int_info = svm->vmcb->control.exit_int_info;

	++vcpu->stat.pf_guest;

	if (is_page_fault(exit_int_info)) {

		svm->vmcb->control.event_inj_err = 0;
		svm->vmcb->control.event_inj = 	SVM_EVTINJ_VALID |
						SVM_EVTINJ_VALID_ERR |
						SVM_EVTINJ_TYPE_EXEPT |
						DF_VECTOR;
		return;
	}
	vcpu->cr2 = addr;
	svm->vmcb->save.cr2 = addr;
	svm->vmcb->control.event_inj = 	SVM_EVTINJ_VALID |
					SVM_EVTINJ_VALID_ERR |
					SVM_EVTINJ_TYPE_EXEPT |
					PF_VECTOR;
	svm->vmcb->control.event_inj_err = err_code;
}


static int is_disabled(void)
{
	u64 vm_cr;

	rdmsrl(MSR_VM_CR, vm_cr);
	if (vm_cr & (1 << SVM_VM_CR_SVM_DISABLE))
		return 1;

	return 0;
}

static void
svm_patch_hypercall(struct kvm_vcpu *vcpu, unsigned char *hypercall)
{
	/*
	 * Patch in the VMMCALL instruction:
	 */
	hypercall[0] = 0x0f;
	hypercall[1] = 0x01;
	hypercall[2] = 0xd9;
	hypercall[3] = 0xc3;
}

static void svm_check_processor_compat(void *rtn)
{
	*(int *)rtn = 0;
}

static struct kvm_x86_ops svm_x86_ops = {
	.cpu_has_kvm_support = has_svm,
	.disabled_by_bios = is_disabled,
	.hardware_setup = svm_hardware_setup,
	.hardware_unsetup = svm_hardware_unsetup,
	.check_processor_compatibility = svm_check_processor_compat,
	.hardware_enable = svm_hardware_enable,
	.hardware_disable = svm_hardware_disable,

	.vcpu_create = svm_create_vcpu,
	.vcpu_free = svm_free_vcpu,
	.vcpu_reset = svm_vcpu_reset,

	.prepare_guest_switch = svm_prepare_guest_switch,
	.vcpu_load = svm_vcpu_load,
	.vcpu_put = svm_vcpu_put,
	.vcpu_decache = svm_vcpu_decache,

	.set_guest_debug = svm_guest_debug,
	.get_msr = svm_get_msr,
	.set_msr = svm_set_msr,
	.get_segment_base = svm_get_segment_base,
	.get_segment = svm_get_segment,
	.set_segment = svm_set_segment,
	.get_cs_db_l_bits = kvm_get_cs_db_l_bits,
	.decache_cr4_guest_bits = svm_decache_cr4_guest_bits,
	.set_cr0 = svm_set_cr0,
	.set_cr3 = svm_set_cr3,
	.set_cr4 = svm_set_cr4,
	.set_efer = svm_set_efer,
	.get_idt = svm_get_idt,
	.set_idt = svm_set_idt,
	.get_gdt = svm_get_gdt,
	.set_gdt = svm_set_gdt,
	.get_dr = svm_get_dr,
	.set_dr = svm_set_dr,
	.cache_regs = svm_cache_regs,
	.decache_regs = svm_decache_regs,
	.get_rflags = svm_get_rflags,
	.set_rflags = svm_set_rflags,

	.tlb_flush = svm_flush_tlb,
	.inject_page_fault = svm_inject_page_fault,

	.inject_gp = svm_inject_gp,

	.run = svm_vcpu_run,
	.handle_exit = handle_exit,
	.skip_emulated_instruction = skip_emulated_instruction,
	.patch_hypercall = svm_patch_hypercall,
	.get_irq = svm_get_irq,
	.set_irq = svm_set_irq,
	.inject_pending_irq = svm_intr_assist,
	.inject_pending_vectors = do_interrupt_requests,
};

static int __init svm_init(void)
{
	return kvm_init_x86(&svm_x86_ops, sizeof(struct vcpu_svm),
			      THIS_MODULE);
}

static void __exit svm_exit(void)
{
	kvm_exit_x86();
}

module_init(svm_init)
module_exit(svm_exit)

/*
 *  linux/arch/i386/mm/fault.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>		/* For unblank_screen() */
#include <linux/highmem.h>
#include <linux/bootmem.h>		/* for max_low_pfn */
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>
#include <linux/kdebug.h>
#include <linux/kprobes.h>

#include <asm/system.h>
#include <asm/desc.h>
#include <asm/segment.h>

extern void die(const char *,struct pt_regs *,long);

#ifdef CONFIG_KPROBES
static inline int notify_page_fault(struct pt_regs *regs)
{
	int ret = 0;

	/* kprobe_running() needs smp_processor_id() */
	if (!user_mode_vm(regs)) {
		preempt_disable();
		if (kprobe_running() && kprobe_fault_handler(regs, 14))
			ret = 1;
		preempt_enable();
	}

	return ret;
}
#else
static inline int notify_page_fault(struct pt_regs *regs)
{
	return 0;
}
#endif

/*
 * Return EIP plus the CS segment base.  The segment limit is also
 * adjusted, clamped to the kernel/user address space (whichever is
 * appropriate), and returned in *eip_limit.
 *
 * The segment is checked, because it might have been changed by another
 * task between the original faulting instruction and here.
 *
 * If CS is no longer a valid code segment, or if EIP is beyond the
 * limit, or if it is a kernel address when CS is not a kernel segment,
 * then the returned value will be greater than *eip_limit.
 * 
 * This is slow, but is very rarely executed.
 */
static inline unsigned long get_segment_eip(struct pt_regs *regs,
					    unsigned long *eip_limit)
{
	unsigned long eip = regs->eip;
	unsigned seg = regs->xcs & 0xffff;
	u32 seg_ar, seg_limit, base, *desc;

	/* Unlikely, but must come before segment checks. */
	if (unlikely(regs->eflags & VM_MASK)) {
		base = seg << 4;
		*eip_limit = base + 0xffff;
		return base + (eip & 0xffff);
	}

	/* The standard kernel/user address space limit. */
	*eip_limit = user_mode(regs) ? USER_DS.seg : KERNEL_DS.seg;
	
	/* By far the most common cases. */
	if (likely(SEGMENT_IS_FLAT_CODE(seg)))
		return eip;

	/* Check the segment exists, is within the current LDT/GDT size,
	   that kernel/user (ring 0..3) has the appropriate privilege,
	   that it's a code segment, and get the limit. */
	__asm__ ("larl %3,%0; lsll %3,%1"
		 : "=&r" (seg_ar), "=r" (seg_limit) : "0" (0), "rm" (seg));
	if ((~seg_ar & 0x9800) || eip > seg_limit) {
		*eip_limit = 0;
		return 1;	 /* So that returned eip > *eip_limit. */
	}

	/* Get the GDT/LDT descriptor base. 
	   When you look for races in this code remember that
	   LDT and other horrors are only used in user space. */
	if (seg & (1<<2)) {
		/* Must lock the LDT while reading it. */
		mutex_lock(&current->mm->context.lock);
		desc = current->mm->context.ldt;
		desc = (void *)desc + (seg & ~7);
	} else {
		/* Must disable preemption while reading the GDT. */
 		desc = (u32 *)get_cpu_gdt_table(get_cpu());
		desc = (void *)desc + (seg & ~7);
	}

	/* Decode the code segment base from the descriptor */
	base = get_desc_base((unsigned long *)desc);

	if (seg & (1<<2)) { 
		mutex_unlock(&current->mm->context.lock);
	} else
		put_cpu();

	/* Adjust EIP and segment limit, and clamp at the kernel limit.
	   It's legitimate for segments to wrap at 0xffffffff. */
	seg_limit += base;
	if (seg_limit < *eip_limit && seg_limit >= base)
		*eip_limit = seg_limit;
	return eip + base;
}

/* 
 * Sometimes AMD Athlon/Opteron CPUs report invalid exceptions on prefetch.
 * Check that here and ignore it.
 */
static int __is_prefetch(struct pt_regs *regs, unsigned long addr)
{ 
	unsigned long limit;
	unsigned char *instr = (unsigned char *)get_segment_eip (regs, &limit);
	int scan_more = 1;
	int prefetch = 0; 
	int i;

	for (i = 0; scan_more && i < 15; i++) { 
		unsigned char opcode;
		unsigned char instr_hi;
		unsigned char instr_lo;

		if (instr > (unsigned char *)limit)
			break;
		if (probe_kernel_address(instr, opcode))
			break; 

		instr_hi = opcode & 0xf0; 
		instr_lo = opcode & 0x0f; 
		instr++;

		switch (instr_hi) { 
		case 0x20:
		case 0x30:
			/* Values 0x26,0x2E,0x36,0x3E are valid x86 prefixes. */
			scan_more = ((instr_lo & 7) == 0x6);
			break;
			
		case 0x60:
			/* 0x64 thru 0x67 are valid prefixes in all modes. */
			scan_more = (instr_lo & 0xC) == 0x4;
			break;		
		case 0xF0:
			/* 0xF0, 0xF2, and 0xF3 are valid prefixes */
			scan_more = !instr_lo || (instr_lo>>1) == 1;
			break;			
		case 0x00:
			/* Prefetch instruction is 0x0F0D or 0x0F18 */
			scan_more = 0;
			if (instr > (unsigned char *)limit)
				break;
			if (probe_kernel_address(instr, opcode))
				break;
			prefetch = (instr_lo == 0xF) &&
				(opcode == 0x0D || opcode == 0x18);
			break;			
		default:
			scan_more = 0;
			break;
		} 
	}
	return prefetch;
}

static inline int is_prefetch(struct pt_regs *regs, unsigned long addr,
			      unsigned long error_code)
{
	if (unlikely(boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
		     boot_cpu_data.x86 >= 6)) {
		/* Catch an obscure case of prefetch inside an NX page. */
		if (nx_enabled && (error_code & 16))
			return 0;
		return __is_prefetch(regs, addr);
	}
	return 0;
} 

static noinline void force_sig_info_fault(int si_signo, int si_code,
	unsigned long address, struct task_struct *tsk)
{
	siginfo_t info;

	info.si_signo = si_signo;
	info.si_errno = 0;
	info.si_code = si_code;
	info.si_addr = (void __user *)address;
	force_sig_info(si_signo, &info, tsk);
}

fastcall void do_invalid_op(struct pt_regs *, unsigned long);

static inline pmd_t *vmalloc_sync_one(pgd_t *pgd, unsigned long address)
{
	unsigned index = pgd_index(address);
	pgd_t *pgd_k;
	pud_t *pud, *pud_k;
	pmd_t *pmd, *pmd_k;

	pgd += index;
	pgd_k = init_mm.pgd + index;

	if (!pgd_present(*pgd_k))
		return NULL;

	/*
	 * set_pgd(pgd, *pgd_k); here would be useless on PAE
	 * and redundant with the set_pmd() on non-PAE. As would
	 * set_pud.
	 */

	pud = pud_offset(pgd, address);
	pud_k = pud_offset(pgd_k, address);
	if (!pud_present(*pud_k))
		return NULL;

	pmd = pmd_offset(pud, address);
	pmd_k = pmd_offset(pud_k, address);
	if (!pmd_present(*pmd_k))
		return NULL;
	if (!pmd_present(*pmd)) {
		set_pmd(pmd, *pmd_k);
		arch_flush_lazy_mmu_mode();
	} else
		BUG_ON(pmd_page(*pmd) != pmd_page(*pmd_k));
	return pmd_k;
}

/*
 * Handle a fault on the vmalloc or module mapping area
 *
 * This assumes no large pages in there.
 */
static inline int vmalloc_fault(unsigned long address)
{
	unsigned long pgd_paddr;
	pmd_t *pmd_k;
	pte_t *pte_k;
	/*
	 * Synchronize this task's top level page-table
	 * with the 'reference' page table.
	 *
	 * Do _not_ use "current" here. We might be inside
	 * an interrupt in the middle of a task switch..
	 */
	pgd_paddr = read_cr3();
	pmd_k = vmalloc_sync_one(__va(pgd_paddr), address);
	if (!pmd_k)
		return -1;
	pte_k = pte_offset_kernel(pmd_k, address);
	if (!pte_present(*pte_k))
		return -1;
	return 0;
}

int show_unhandled_signals = 1;

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 *
 * error_code:
 *	bit 0 == 0 means no page found, 1 means protection fault
 *	bit 1 == 0 means read, 1 means write
 *	bit 2 == 0 means kernel, 1 means user-mode
 *	bit 3 == 1 means use of reserved bit detected
 *	bit 4 == 1 means fault was an instruction fetch
 */
/**
 * 缺页异常处理程序
 * regs:缺页时的寄存器现场
 * error_code:0位-缺页或保护异常，1-读访问造成的异常，2-内核态，3-使用了保留位，4-取指异常
 */
fastcall void __kprobes do_page_fault(struct pt_regs *regs,
				      unsigned long error_code)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	unsigned long address;
	int write, si_code;
	int fault;

	/*
	 * We can fault from pretty much anywhere, with unknown IRQ state.
	 */
	trace_hardirqs_fixup();

	/* get the address */
	/* 缺页时的异常地址 */
        address = read_cr2();

	tsk = current;

	si_code = SEGV_MAPERR;

	/*
	 * We fault-in kernel-space virtual memory on-demand. The
	 * 'reference' page table is init_mm.pgd.
	 *
	 * NOTE! We MUST NOT take any locks for this case. We may
	 * be in an interrupt or a critical region, and should
	 * only copy the information from the master page table,
	 * nothing more.
	 *
	 * This verifies that the fault happens in kernel space
	 * (error_code & 4) == 0, and that the fault was not a
	 * protection error (error_code & 9) == 0.
	 */
	/* 超出了用户态地址范围，可能是内核态vmalloc区域的异常 */
	if (unlikely(address >= TASK_SIZE)) {
		/* vmalloc_fault将进程页表与内核的主页表中的信息进行同步 */
		if (!(error_code & 0x0000000d) && vmalloc_fault(address) >= 0)/* 确保不是在内核态的保护性错误 */
			return;
		/* 通知kprobe处理 */
		if (notify_page_fault(regs))
			return;
		/*
		 * Don't take the mm semaphore here. If we fixup a prefetch
		 * fault we could otherwise deadlock.
		 */
		goto bad_area_nosemaphore;
	}

	if (notify_page_fault(regs))
		return;

	/* It's safe to allow irq's after cr2 has been saved and the vmalloc
	   fault has been handled. */
	if (regs->eflags & (X86_EFLAGS_IF|VM_MASK))
		local_irq_enable();

	mm = tsk->mm;

	/*
	 * If we're in an interrupt, have no user context or are running in an
	 * atomic region then we must not take the fault..
	 */
	if (in_atomic() || !mm)/* 在中断或原子状态下，或者是内核线程。此时不允许访问用户态内容，异常 */
		goto bad_area_nosemaphore;

	/* When running in the kernel we expect faults to occur only to
	 * addresses in user space.  All other faults represent errors in the
	 * kernel and should generate an OOPS.  Unfortunately, in the case of an
	 * erroneous fault occurring in a code path which already holds mmap_sem
	 * we will deadlock attempting to validate the fault against the
	 * address space.  Luckily the kernel only validly references user
	 * space from well defined areas of code, which are listed in the
	 * exceptions table.
	 *
	 * As the vast majority of faults will be valid we will only perform
	 * the source reference check when there is a possibility of a deadlock.
	 * Attempt to lock the address space, if we cannot we then validate the
	 * source.  If this is invalid we can skip the address space check,
	 * thus avoiding the deadlock.
	 */
	if (!down_read_trylock(&mm->mmap_sem)) {
		if ((error_code & 4) == 0 &&
		    !search_exception_tables(regs->eip))
			goto bad_area_nosemaphore;
		down_read(&mm->mmap_sem);
	}

	/* 不是中断上下文，也有自己的上下文，则查找进程的地址 */
	vma = find_vma(mm, address);
	if (!vma)/* 错误地址 */
		goto bad_area;
	if (vma->vm_start <= address)/* 有效地址空间 */
		goto good_area;
	/* 异常地址之后有一个vma区域，但是异常地址没有包含在该区域内 */
	if (!(vma->vm_flags & VM_GROWSDOWN))/* 判断该区域是否为向下扩展的堆栈 */
		goto bad_area;
	if (error_code & 4) {/* 缺页异常，判断是否可以扩展堆栈 */
		/*
		 * Accessing the stack below %esp is always a bug.
		 * The large cushion allows instructions like enter
		 * and pusha to work.  ("enter $65535,$31" pushes
		 * 32 pointers and then decrements %esp by 65535.)
		 */
		if (address + 65536 + 32 * sizeof(unsigned long) < regs->esp)/* 地址太小，不可能是访问堆栈造成的 */
			goto bad_area;
	}
	/* 试图扩展堆栈 */
	if (expand_stack(vma, address))
		goto bad_area;
/*
 * Ok, we have a good vm_area for this memory access, so
 * we can handle it..
 */
good_area:
	/* 运行到这里，说明访问的地址区间是正常的 */
	si_code = SEGV_ACCERR;
	write = 0;
	switch (error_code & 3) {
		default:	/* 3: write, present */
				/* fall through */
		case 2:		/* write, not present */
			/* 错误代码为2、3，说明是写造成的异常 */
			if (!(vma->vm_flags & VM_WRITE))/* 相关区域不允许写 */
				goto bad_area;
			write++;
			break;
		case 1:		/* read, present */
			/* 读引起的异常，并且相关的pte有效。可能是用户访问了保护页，异常 */
			goto bad_area;
		case 0:		/* read, not present */
			if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))/* 判断vma是否允许读 */
				goto bad_area;
	}

 survive:
	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */ 
	/* 运行到这里，说明用户访问的区域是有效的，仅仅是内核还没有准备好页表 */
	fault = handle_mm_fault(mm, vma, address, write);/* 处理缺页 */
	if (unlikely(fault & VM_FAULT_ERROR)) {/* 由于特定的异常造成无法处理缺页 */
		if (fault & VM_FAULT_OOM)/* 没有内存了 */
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)/* 奇怪的错误，例如要访问的文件已经被收缩，向用户发送信号 */
			goto do_sigbus;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)/* 根据本次异常是从块设备中读取数据还是从内存中读取数据，来对异常缺页进行统计 */
		tsk->maj_flt++;
	else
		tsk->min_flt++;

	/*
	 * Did it hit the DOS screen memory VA from vm86 mode?
	 */
	if (regs->eflags & VM_MASK) {/* 这里是处理实模式程序，略过 */
		unsigned long bit = (address - 0xA0000) >> PAGE_SHIFT;
		if (bit < 32)
			tsk->thread.screen_bitmap |= 1 << bit;
	}
	up_read(&mm->mmap_sem);/* 释放mmap信号量 */
	return;

/*
 * Something tried to access memory that isn't in our memory map..
 * Fix it, but check if it's kernel or user first..
 */
bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	/* User mode accesses just cause a SIGSEGV */
	if (error_code & 4) {/* 异常来自于用户态 */
		/*
		 * It's possible to have interrupts off here.
		 */
		local_irq_enable();/* 强制打开中断 */

		/* 
		 * Valid to do another page fault here because this one came 
		 * from user space.
		 */
		if (is_prefetch(regs, address, error_code))
			return;

		if (show_unhandled_signals && unhandled_signal(tsk, SIGSEGV) &&
		    printk_ratelimit()) {/* 用户没有处理段错误，这里打印警告 */
			printk("%s%s[%d]: segfault at %08lx eip %08lx "
			    "esp %08lx error %lx\n",
			    task_pid_nr(tsk) > 1 ? KERN_INFO : KERN_EMERG,
			    tsk->comm, task_pid_nr(tsk), address, regs->eip,
			    regs->esp, error_code);
		}
		tsk->thread.cr2 = address;
		/* Kernel addresses are always protection faults */
		tsk->thread.error_code = error_code | (address >= TASK_SIZE);
		tsk->thread.trap_no = 14;
		/* 强制向进程发送段错误信号 */
		force_sig_info_fault(SIGSEGV, si_code, address, tsk);
		return;
	}

#ifdef CONFIG_X86_F00F_BUG
	/*
	 * Pentium F0 0F C7 C8 bug workaround.
	 */
	if (boot_cpu_data.f00f_bug) {
		unsigned long nr;
		
		nr = (address - idt_descr.address) >> 3;

		if (nr == 6) {
			do_invalid_op(regs, 0);
			return;
		}
	}
#endif

no_context:/* 运行到这里，说明是内核态产生了异常，没有进程上下文 */
	/* Are we prepared to handle this kernel fault?  */
	/* 异常发生在内核态，首先尝试用异常修复表修复 */
	if (fixup_exception(regs))
		return;

	/* 
	 * Valid to do another page fault here, because if this fault
	 * had been triggered by is_prefetch fixup_exception would have 
	 * handled it.
	 */
	/* 某些CPU会在预取时发生段错误，这实际上并不是错误 */
 	if (is_prefetch(regs, address, error_code))
 		return;

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 */

	/* 运行到这里，说明真正错误了，打印一些错误信息，用于调试 */
	bust_spinlocks(1);

	if (oops_may_print()) {
		__typeof__(pte_val(__pte(0))) page;

#ifdef CONFIG_X86_PAE
		if (error_code & 16) {
			pte_t *pte = lookup_address(address);

			if (pte && pte_present(*pte) && !pte_exec_kernel(*pte))
				printk(KERN_CRIT "kernel tried to execute "
					"NX-protected page - exploit attempt? "
					"(uid: %d)\n", current->uid);
		}
#endif
		if (address < PAGE_SIZE)
			printk(KERN_ALERT "BUG: unable to handle kernel NULL "
					"pointer dereference");
		else
			printk(KERN_ALERT "BUG: unable to handle kernel paging"
					" request");
		printk(" at virtual address %08lx\n",address);
		printk(KERN_ALERT "printing eip: %08lx ", regs->eip);

		page = read_cr3();
		page = ((__typeof__(page) *) __va(page))[address >> PGDIR_SHIFT];
#ifdef CONFIG_X86_PAE
		printk("*pdpt = %016Lx ", page);
		if ((page >> PAGE_SHIFT) < max_low_pfn
		    && page & _PAGE_PRESENT) {
			page &= PAGE_MASK;
			page = ((__typeof__(page) *) __va(page))[(address >> PMD_SHIFT)
			                                         & (PTRS_PER_PMD - 1)];
			printk(KERN_CONT "*pde = %016Lx ", page);
			page &= ~_PAGE_NX;
		}
#else
		printk("*pde = %08lx ", page);
#endif

		/*
		 * We must not directly access the pte in the highpte
		 * case if the page table is located in highmem.
		 * And let's rather not kmap-atomic the pte, just in case
		 * it's allocated already.
		 */
		if ((page >> PAGE_SHIFT) < max_low_pfn
		    && (page & _PAGE_PRESENT)
		    && !(page & _PAGE_PSE)) {
			page &= PAGE_MASK;
			page = ((__typeof__(page) *) __va(page))[(address >> PAGE_SHIFT)
			                                         & (PTRS_PER_PTE - 1)];
			printk("*pte = %0*Lx ", sizeof(page)*2, (u64)page);
		}

		printk("\n");
	}

	tsk->thread.cr2 = address;
	tsk->thread.trap_no = 14;
	tsk->thread.error_code = error_code;
	die("Oops", regs, error_code);
	bust_spinlocks(0);
	do_exit(SIGKILL);

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	up_read(&mm->mmap_sem);
	if (is_global_init(tsk)) {
		yield();
		down_read(&mm->mmap_sem);
		goto survive;
	}
	printk("VM: killing process %s\n", tsk->comm);
	if (error_code & 4)
		do_group_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	/* Kernel mode? Handle exceptions or die */
	if (!(error_code & 4))
		goto no_context;

	/* User space => ok to do another page fault */
	if (is_prefetch(regs, address, error_code))
		return;

	tsk->thread.cr2 = address;
	tsk->thread.error_code = error_code;
	tsk->thread.trap_no = 14;
	force_sig_info_fault(SIGBUS, BUS_ADRERR, address, tsk);
}

void vmalloc_sync_all(void)
{
	/*
	 * Note that races in the updates of insync and start aren't
	 * problematic: insync can only get set bits added, and updates to
	 * start are only improving performance (without affecting correctness
	 * if undone).
	 */
	static DECLARE_BITMAP(insync, PTRS_PER_PGD);
	static unsigned long start = TASK_SIZE;
	unsigned long address;

	if (SHARED_KERNEL_PMD)
		return;

	BUILD_BUG_ON(TASK_SIZE & ~PGDIR_MASK);
	for (address = start; address >= TASK_SIZE; address += PGDIR_SIZE) {
		if (!test_bit(pgd_index(address), insync)) {
			unsigned long flags;
			struct page *page;

			spin_lock_irqsave(&pgd_lock, flags);
			for (page = pgd_list; page; page =
					(struct page *)page->index)
				if (!vmalloc_sync_one(page_address(page),
								address)) {
					BUG_ON(page != pgd_list);
					break;
				}
			spin_unlock_irqrestore(&pgd_lock, flags);
			if (!page)
				set_bit(pgd_index(address), insync);
		}
		if (address == start && test_bit(pgd_index(address), insync))
			start = address + PGDIR_SIZE;
	}
}

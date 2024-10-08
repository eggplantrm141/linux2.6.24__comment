/* ld script to make i386 Linux kernel
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>;
 *
 * Don't define absolute symbols until and unless you know that symbol
 * value is should remain constant even if kernel image is relocated
 * at run time. Absolute symbols are not relocated. If symbol value should
 * change if kernel is relocated, make the symbol section relative and
 * put it inside the section definition.
 */

/* Don't define absolute symbols until and unless you know that symbol
 * value is should remain constant even if kernel image is relocated
 * at run time. Absolute symbols are not relocated. If symbol value should
 * change if kernel is relocated, make the symbol section relative and
 * put it inside the section definition.
 */
#define LOAD_OFFSET __PAGE_OFFSET

#include <asm-generic/vmlinux.lds.h>
#include <asm/thread_info.h>
#include <asm/page.h>
#include <asm/cache.h>
#include <asm/boot.h>

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(phys_startup_32)
jiffies = jiffies_64;

PHDRS {
	text PT_LOAD FLAGS(5);	/* R_E */
	data PT_LOAD FLAGS(7);	/* RWE */
	note PT_NOTE FLAGS(0);	/* ___ */
}
SECTIONS
{
  . = LOAD_OFFSET + LOAD_PHYSICAL_ADDR;
  phys_startup_32 = startup_32 - LOAD_OFFSET;

  .text.head : AT(ADDR(.text.head) - LOAD_OFFSET) {
  	_text = .;			/* Text and read-only data */
	*(.text.head)
  } :text = 0x9090

  /* read-only */
  .text : AT(ADDR(.text) - LOAD_OFFSET) {
	TEXT_TEXT
	SCHED_TEXT
	LOCK_TEXT
	KPROBES_TEXT
	*(.fixup)
	*(.gnu.warning)
  	_etext = .;			/* End of text section */
  } :text = 0x9090

  . = ALIGN(16);		/* Exception table */
  __ex_table : AT(ADDR(__ex_table) - LOAD_OFFSET) {
  	__start___ex_table = .;
	 *(__ex_table)
  	__stop___ex_table = .;
  }

  NOTES :text :note

  BUG_TABLE :text

  . = ALIGN(4);
  .tracedata : AT(ADDR(.tracedata) - LOAD_OFFSET) {
  	__tracedata_start = .;
	*(.tracedata)
  	__tracedata_end = .;
  }

  RODATA

  /* writeable */
  . = ALIGN(4096);
  .data : AT(ADDR(.data) - LOAD_OFFSET) {	/* Data */
	DATA_DATA
	CONSTRUCTORS
	} :data

  . = ALIGN(4096);
  .data_nosave : AT(ADDR(.data_nosave) - LOAD_OFFSET) {
  	__nosave_begin = .;
	*(.data.nosave)
  	. = ALIGN(4096);
  	__nosave_end = .;
  }

  . = ALIGN(4096);
  .data.page_aligned : AT(ADDR(.data.page_aligned) - LOAD_OFFSET) {
	*(.data.page_aligned)
	*(.data.idt)
  }

  . = ALIGN(32);
  .data.cacheline_aligned : AT(ADDR(.data.cacheline_aligned) - LOAD_OFFSET) {
	*(.data.cacheline_aligned)
  }

  /* rarely changed data like cpu maps */
  . = ALIGN(32);
  .data.read_mostly : AT(ADDR(.data.read_mostly) - LOAD_OFFSET) {
	*(.data.read_mostly)
	_edata = .;		/* End of data section */
  }

  . = ALIGN(THREAD_SIZE);	/* init_task */
  .data.init_task : AT(ADDR(.data.init_task) - LOAD_OFFSET) {
	*(.data.init_task)
  }

  /* might get freed after init */
  . = ALIGN(4096);
  .smp_locks : AT(ADDR(.smp_locks) - LOAD_OFFSET) {
  	__smp_locks = .;
	*(.smp_locks)
	__smp_locks_end = .;
  }
  /* will be freed after init
   * Following ALIGN() is required to make sure no other data falls on the
   * same page where __smp_alt_end is pointing as that page might be freed
   * after boot. Always make sure that ALIGN() directive is present after
   * the section which contains __smp_alt_end.
   */
  . = ALIGN(4096);

  /* will be freed after init */
  . = ALIGN(4096);		/* Init code and data */
  .init.text : AT(ADDR(.init.text) - LOAD_OFFSET) {
  	__init_begin = .;
	_sinittext = .;
	*(.init.text)
	_einittext = .;
  }
  .init.data : AT(ADDR(.init.data) - LOAD_OFFSET) { *(.init.data) }
  . = ALIGN(16);
  .init.setup : AT(ADDR(.init.setup) - LOAD_OFFSET) {
  	__setup_start = .;
	*(.init.setup)
  	__setup_end = .;
   }
  .initcall.init : AT(ADDR(.initcall.init) - LOAD_OFFSET) {
  	__initcall_start = .;
	INITCALLS
  	__initcall_end = .;
  }
  .con_initcall.init : AT(ADDR(.con_initcall.init) - LOAD_OFFSET) {
  	__con_initcall_start = .;
	*(.con_initcall.init)
  	__con_initcall_end = .;
  }
  SECURITY_INIT
  . = ALIGN(4);
  .altinstructions : AT(ADDR(.altinstructions) - LOAD_OFFSET) {
  	__alt_instructions = .;
	*(.altinstructions)
	__alt_instructions_end = .;
  }
  .altinstr_replacement : AT(ADDR(.altinstr_replacement) - LOAD_OFFSET) {
	*(.altinstr_replacement)
  }
  . = ALIGN(4);
  .parainstructions : AT(ADDR(.parainstructions) - LOAD_OFFSET) {
  	__parainstructions = .;
	*(.parainstructions)
  	__parainstructions_end = .;
  }
  /* .exit.text is discard at runtime, not link time, to deal with references
     from .altinstructions and .eh_frame */
  .exit.text : AT(ADDR(.exit.text) - LOAD_OFFSET) { *(.exit.text) }
  .exit.data : AT(ADDR(.exit.data) - LOAD_OFFSET) { *(.exit.data) }
#if defined(CONFIG_BLK_DEV_INITRD)
  . = ALIGN(4096);
  .init.ramfs : AT(ADDR(.init.ramfs) - LOAD_OFFSET) {
	__initramfs_start = .;
	*(.init.ramfs)
	__initramfs_end = .;
  }
#endif
  . = ALIGN(4096);
  .data.percpu  : AT(ADDR(.data.percpu) - LOAD_OFFSET) {
	__per_cpu_start = .;
	*(.data.percpu)
	*(.data.percpu.shared_aligned)
	__per_cpu_end = .;
  }
  . = ALIGN(4096);
  /* freed after init ends here */
	
  .bss : AT(ADDR(.bss) - LOAD_OFFSET) {
	__init_end = .;
	__bss_start = .;		/* BSS */
	*(.bss.page_aligned)
	*(.bss)
	. = ALIGN(4);
	__bss_stop = .;
  	_end = . ;
	/* This is where the kernel creates the early boot page tables */
	. = ALIGN(4096);
	pg0 = . ;
  }

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.exitcall.exit)
	}

  STABS_DEBUG

  DWARF_DEBUG
}

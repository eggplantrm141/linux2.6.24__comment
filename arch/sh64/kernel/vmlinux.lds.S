/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh5/vmlinux.lds.S
 *
 * ld script to make ST50 Linux kernel
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 * benedict.gaster@superh.com:	 2nd May 2002
 *    Add definition of empty_zero_page to be the first page of kernel image.
 *
 * benedict.gaster@superh.com:	 3rd May 2002
 *    Added support for ramdisk, removing statically linked romfs at the same time.
 *
 * lethal@linux-sh.org:          9th May 2003
 *    Kill off GLOBAL_NAME() usage and other CDC-isms.
 *
 * lethal@linux-sh.org:         19th May 2003
 *    Remove support for ancient toolchains.
 */

#include <asm/page.h>
#include <asm/cache.h>
#include <asm/processor.h>
#include <asm/thread_info.h>

#define LOAD_OFFSET	CONFIG_CACHED_MEMORY_OFFSET
#include <asm-generic/vmlinux.lds.h>

OUTPUT_ARCH(sh:sh5)

#define C_PHYS(x) AT (ADDR(x) - LOAD_OFFSET)

ENTRY(__start)
SECTIONS
{
  . = CONFIG_CACHED_MEMORY_OFFSET + CONFIG_MEMORY_START + PAGE_SIZE;
  _text = .;			/* Text and read-only data */
  text = .;			/* Text and read-only data */

  .empty_zero_page : C_PHYS(.empty_zero_page) {
	*(.empty_zero_page)
	} = 0

  .text : C_PHYS(.text) {
  	*(.text.head)
	TEXT_TEXT
	*(.text64)
        *(.text..SHmedia32)
	SCHED_TEXT
	LOCK_TEXT
	*(.fixup)
	*(.gnu.warning)
#ifdef CONFIG_LITTLE_ENDIAN
	} = 0x6ff0fff0
#else
	} = 0xf0fff06f
#endif

  /* We likely want __ex_table to be Cache Line aligned */
  . = ALIGN(L1_CACHE_BYTES);		/* Exception table */
  __start___ex_table = .;
  __ex_table : C_PHYS(__ex_table) { *(__ex_table) }
  __stop___ex_table = .;

  _etext = .;			/* End of text section */

  NOTES 

  RODATA

  .data : C_PHYS(.data) {			/* Data */
	DATA_DATA
	CONSTRUCTORS
	}

  . = ALIGN(PAGE_SIZE);
  .data.page_aligned : C_PHYS(.data.page_aligned) { *(.data.page_aligned) }

  PERCPU(PAGE_SIZE)

  . = ALIGN(L1_CACHE_BYTES);
  .data.cacheline_aligned : C_PHYS(.data.cacheline_aligned) { *(.data.cacheline_aligned) }

  _edata = .;			/* End of data section */

  . = ALIGN(THREAD_SIZE);	/* init_task: structure size aligned */
  .data.init_task : C_PHYS(.data.init_task) { *(.data.init_task) }

  . = ALIGN(PAGE_SIZE);		/* Init code and data */
  __init_begin = .;
  _sinittext = .;
  .init.text : C_PHYS(.init.text) { *(.init.text) }
  _einittext = .;
  .init.data : C_PHYS(.init.data) { *(.init.data) }
  . = ALIGN(L1_CACHE_BYTES);	/* Better if Cache Line aligned */
  __setup_start = .;
  .init.setup : C_PHYS(.init.setup) { *(.init.setup) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : C_PHYS(.initcall.init) {
	INITCALLS
  }
  __initcall_end = .;
  __con_initcall_start = .;
  .con_initcall.init : C_PHYS(.con_initcall.init) { *(.con_initcall.init) }
  __con_initcall_end = .;
  SECURITY_INIT

#ifdef CONFIG_BLK_DEV_INITRD
  __initramfs_start = .;
  .init.ramfs : C_PHYS(.init.ramfs) { *(.init.ramfs) }
  __initramfs_end = .;
#endif

  . = ALIGN(PAGE_SIZE);
  __init_end = .;

  /* Align to the biggest single data representation, head and tail */
  . = ALIGN(8);
  __bss_start = .;		/* BSS */
  .bss : C_PHYS(.bss) {
	*(.bss)
	}
  . = ALIGN(8);
  _end = . ;

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.exit.text)
	*(.exit.data)
	*(.exitcall.exit)
	}

  STABS_DEBUG
  DWARF_DEBUG
}

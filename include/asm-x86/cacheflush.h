#ifndef _ASM_X86_CACHEFLUSH_H
#define _ASM_X86_CACHEFLUSH_H

/* Keep includes the same across arches.  */
#include <linux/mm.h>

/* Caches aren't brain-dead on the intel. */
/* 将整个高速缓存刷出 */
#define flush_cache_all()			do { } while (0)
/* 将某个地址空间的高速缓存刷出 */
#define flush_cache_mm(mm)			do { } while (0)
#define flush_cache_dup_mm(mm)			do { } while (0)
#define flush_cache_range(vma, start, end)	do { } while (0)
#define flush_cache_page(vma, vmaddr, pfn)	do { } while (0)
/* 刷新数据缓存，防止页面别名 */
#define flush_dcache_page(page)			do { } while (0)
#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)
/* 刷新指令缓存，自修改代码、模块均使用它 */
#define flush_icache_range(start, end)		do { } while (0)
#define flush_icache_page(vma,pg)		do { } while (0)
/* 刷新用户态的指令缓存，用于ptrace */
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)
#define flush_cache_vmap(start, end)		do { } while (0)
#define flush_cache_vunmap(start, end)		do { } while (0)

#define copy_to_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)
#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)

void global_flush_tlb(void);
int change_page_attr(struct page *page, int numpages, pgprot_t prot);
int change_page_attr_addr(unsigned long addr, int numpages, pgprot_t prot);
void clflush_cache_range(void *addr, int size);

#ifdef CONFIG_DEBUG_PAGEALLOC
/* internal debugging function */
void kernel_map_pages(struct page *page, int numpages, int enable);
#endif

#ifdef CONFIG_DEBUG_RODATA
void mark_rodata_ro(void);
#endif

#endif

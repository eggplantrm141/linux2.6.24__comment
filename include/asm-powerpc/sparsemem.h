#ifndef _ASM_POWERPC_SPARSEMEM_H
#define _ASM_POWERPC_SPARSEMEM_H 1
#ifdef __KERNEL__

#ifdef CONFIG_SPARSEMEM
/*
 * SECTION_SIZE_BITS		2^N: how big each section will be
 * MAX_PHYSADDR_BITS		2^N: how much physical address space we have
 * MAX_PHYSMEM_BITS		2^N: how much memory we can have in that space
 */
#define SECTION_SIZE_BITS       24

#if defined(CONFIG_PS3_USE_LPAR_ADDR)
#define MAX_PHYSADDR_BITS       47
#define MAX_PHYSMEM_BITS        47
#else
#define MAX_PHYSADDR_BITS       44
#define MAX_PHYSMEM_BITS        44
#endif

#ifdef CONFIG_MEMORY_HOTPLUG
extern void create_section_mapping(unsigned long start, unsigned long end);
#ifdef CONFIG_NUMA
extern int hot_add_scn_to_nid(unsigned long scn_addr);
#else
static inline int hot_add_scn_to_nid(unsigned long scn_addr)
{
	return 0;
}
#endif /* CONFIG_NUMA */
#endif /* CONFIG_MEMORY_HOTPLUG */

#endif /* CONFIG_SPARSEMEM */

#endif /* __KERNEL__ */
#endif /* _ASM_POWERPC_SPARSEMEM_H */

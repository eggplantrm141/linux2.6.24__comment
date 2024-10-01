#ifndef _X86_64_CURRENT_H
#define _X86_64_CURRENT_H

#if !defined(__ASSEMBLY__) 
struct task_struct;

#include <asm/pda.h>

static inline struct task_struct *get_current(void) 
{
  /* 这里的read_pda是一个宏，这里的pcurrent相当于是一个字段 */
  /* 从特殊的内核栈中获取task的指针 */
	struct task_struct *t = read_pda(pcurrent); 
	return t;
} 
/* current 则调用get_current 函数获取task_struct * */
#define current get_current()

#else

#ifndef ASM_OFFSET_H
#include <asm/asm-offsets.h> 
#endif

#define GET_CURRENT(reg) movq %gs:(pda_pcurrent),reg

#endif

#endif /* !(_X86_64_CURRENT_H) */

/*
 * mm/thrash.c
 *
 * Copyright (C) 2004, Red Hat, Inc.
 * Copyright (C) 2004, Rik van Riel <riel@redhat.com>
 * Released under the GPL, see the file COPYING for details.
 *
 * Simple token based thrashing protection, using the algorithm
 * described in:  http://www.cs.wm.edu/~sjiang/token.pdf
 *
 * Sep 2006, Ashwin Chaugule <ashwin.chaugule@celunite.com>
 * Improved algorithm to pass token:
 * Each task has a priority which is incremented if it contended
 * for the token in an interval less than its previous attempt.
 * If the token is acquired, that task's priority is boosted to prevent
 * the token from bouncing around too often and to let the task make
 * some progress in its execution.
 */

#include <linux/jiffies.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/swap.h>

static DEFINE_SPINLOCK(swap_token_lock);
/**
 * 当前持有交换令牌的进程
 */
struct mm_struct *swap_token_mm;
/**
 * 每次换页过程中，都会调用do_swap_page，它会对本变量加1.用于判断交换令牌的频繁程度。
 */
static unsigned int global_faults;

/**
 * 获取交换令牌
 */
void grab_swap_token(void)
{
	int current_interval;

	global_faults++;

	current_interval = global_faults - current->mm->faultstamp;

	if (!spin_trylock(&swap_token_lock))
		return;

	/* First come first served */
	if (swap_token_mm == NULL) {/* 交换令牌未分配给任何进程 */
		/* 修改令牌优先级 */
		current->mm->token_priority = current->mm->token_priority + 2;
		swap_token_mm = current->mm;
		goto out;
	}

	if (current->mm != swap_token_mm) {/* 其他进程获取了令牌 */
		if (current_interval < current->mm->last_interval)
			current->mm->token_priority++;
		else {
			if (likely(current->mm->token_priority > 0))
				current->mm->token_priority--;
		}
		/* Check if we deserve the token */
		if (current->mm->token_priority >
				swap_token_mm->token_priority) {
			current->mm->token_priority += 2;
			swap_token_mm = current->mm;
		}
	} else {
		/* Token holder came in again! */
		current->mm->token_priority += 2;
	}

out:
	current->mm->faultstamp = global_faults;
	current->mm->last_interval = current_interval;
	spin_unlock(&swap_token_lock);
return;
}

/* Called on process exit. */
void __put_swap_token(struct mm_struct *mm)
{
	spin_lock(&swap_token_lock);
	if (likely(mm == swap_token_mm))
		swap_token_mm = NULL;
	spin_unlock(&swap_token_lock);
}

/*
 * linux/include/linux/jbd2.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD2_H
#define _LINUX_JBD2_H

/* Allow this file to be included directly into e2fsprogs */
#ifndef __KERNEL__
#include "jfs_compat.h"
#define JBD2_DEBUG
#define jfs_debug jbd_debug
#else

#include <linux/types.h>
#include <linux/buffer_head.h>
#include <linux/journal-head.h>
#include <linux/stddef.h>
#include <linux/bit_spinlock.h>
#include <linux/mutex.h>
#include <linux/timer.h>

#include <asm/semaphore.h>
#endif

#define journal_oom_retry 1

/*
 * Define JBD2_PARANIOD_IOFAIL to cause a kernel BUG() if ext4 finds
 * certain classes of error which can occur due to failed IOs.  Under
 * normal use we want ext4 to continue after such errors, because
 * hardware _can_ fail, but for debugging purposes when running tests on
 * known-good hardware we may want to trap these errors.
 */
#undef JBD2_PARANOID_IOFAIL

/*
 * The default maximum commit age, in seconds.
 */
#define JBD2_DEFAULT_MAX_COMMIT_AGE 5

#ifdef CONFIG_JBD2_DEBUG
/*
 * Define JBD2_EXPENSIVE_CHECKING to enable more expensive internal
 * consistency checks.  By default we don't do this unless
 * CONFIG_JBD2_DEBUG is on.
 */
#define JBD2_EXPENSIVE_CHECKING
extern u8 jbd2_journal_enable_debug;

#define jbd_debug(n, f, a...)						\
	do {								\
		if ((n) <= jbd2_journal_enable_debug) {			\
			printk (KERN_DEBUG "(%s, %d): %s: ",		\
				__FILE__, __LINE__, __FUNCTION__);	\
			printk (f, ## a);				\
		}							\
	} while (0)
#else
#define jbd_debug(f, a...)	/**/
#endif

static inline void *jbd2_alloc(size_t size, gfp_t flags)
{
	return (void *)__get_free_pages(flags, get_order(size));
}

static inline void jbd2_free(void *ptr, size_t size)
{
	free_pages((unsigned long)ptr, get_order(size));
};

#define JBD2_MIN_JOURNAL_BLOCKS 1024

#ifdef __KERNEL__

/**
 * typedef handle_t - The handle_t type represents a single atomic update being performed by some process.
 *
 * All filesystem modifications made by the process go
 * through this handle.  Recursive operations (such as quota operations)
 * are gathered into a single update.
 *
 * The buffer credits field is used to account for journaled buffers
 * being modified by the running process.  To ensure that there is
 * enough log space for all outstanding operations, we need to limit the
 * number of outstanding buffers possible at any time.  When the
 * operation completes, any buffer credits not used are credited back to
 * the transaction, so that at all times we know how many buffers the
 * outstanding updates on a transaction might possibly touch.
 *
 * This is an opaque datatype.
 **/
typedef struct handle_s		handle_t;	/* Atomic operation type */


/**
 * typedef journal_t - The journal_t maintains all of the journaling state information for a single filesystem.
 *
 * journal_t is linked to from the fs superblock structure.
 *
 * We use the journal_t to keep track of all outstanding transaction
 * activity on the filesystem, and to manage the state of the log
 * writing process.
 *
 * This is an opaque datatype.
 **/
typedef struct journal_s	journal_t;	/* Journal control structure */
#endif

/*
 * Internal structures used by the logging mechanism:
 */

#define JBD2_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/*
 * Descriptor block types:
 */

#define JBD2_DESCRIPTOR_BLOCK	1
#define JBD2_COMMIT_BLOCK	2
#define JBD2_SUPERBLOCK_V1	3
#define JBD2_SUPERBLOCK_V2	4
#define JBD2_REVOKE_BLOCK	5

/*
 * Standard header for all descriptor blocks:
 */
typedef struct journal_header_s
{
	__be32		h_magic;
	__be32		h_blocktype;
	__be32		h_sequence;
} journal_header_t;


/*
 * The block tag: used to describe a single buffer in the journal.
 * t_blocknr_high is only used if INCOMPAT_64BIT is set, so this
 * raw struct shouldn't be used for pointer math or sizeof() - use
 * journal_tag_bytes(journal) instead to compute this.
 */
typedef struct journal_block_tag_s
{
	__be32		t_blocknr;	/* The on-disk block number */
	__be32		t_flags;	/* See below */
	__be32		t_blocknr_high; /* most-significant high 32bits. */
} journal_block_tag_t;

#define JBD2_TAG_SIZE32 (offsetof(journal_block_tag_t, t_blocknr_high))
#define JBD2_TAG_SIZE64 (sizeof(journal_block_tag_t))

/*
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log
 */
typedef struct jbd2_journal_revoke_header_s
{
	journal_header_t r_header;
	__be32		 r_count;	/* Count of bytes used in the block */
} jbd2_journal_revoke_header_t;


/* Definitions for the journal tag flags word: */
#define JBD2_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JBD2_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JBD2_FLAG_DELETED	4	/* block deleted by this transaction */
#define JBD2_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock.  All fields are in big-endian byte order.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__be32	s_blocksize;		/* journal device blocksize */
	__be32	s_maxlen;		/* total blocks in journal file */
	__be32	s_first;		/* first block of log information */

/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__be32	s_sequence;		/* first commit ID expected in log */
	__be32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by jbd2_journal_abort(). */
	__be32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__be32	s_feature_compat;	/* compatible feature set */
	__be32	s_feature_incompat;	/* incompatible feature set */
	__be32	s_feature_ro_compat;	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__be32	s_nr_users;		/* Nr of filesystems sharing log */

	__be32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/

/* 0x0048 */
	__be32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__be32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u32	s_padding[44];

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;

#define JBD2_HAS_COMPAT_FEATURE(j,mask)					\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_compat & cpu_to_be32((mask))))
#define JBD2_HAS_RO_COMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_ro_compat & cpu_to_be32((mask))))
#define JBD2_HAS_INCOMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_incompat & cpu_to_be32((mask))))

#define JBD2_FEATURE_INCOMPAT_REVOKE	0x00000001
#define JBD2_FEATURE_INCOMPAT_64BIT	0x00000002

/* Features known to this kernel version: */
#define JBD2_KNOWN_COMPAT_FEATURES	0
#define JBD2_KNOWN_ROCOMPAT_FEATURES	0
#define JBD2_KNOWN_INCOMPAT_FEATURES	(JBD2_FEATURE_INCOMPAT_REVOKE | \
					 JBD2_FEATURE_INCOMPAT_64BIT)

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>

#define JBD2_ASSERTIONS
#ifdef JBD2_ASSERTIONS
#define J_ASSERT(assert)						\
do {									\
	if (!(assert)) {						\
		printk (KERN_EMERG					\
			"Assertion failure in %s() at %s:%d: \"%s\"\n",	\
			__FUNCTION__, __FILE__, __LINE__, # assert);	\
		BUG();							\
	}								\
} while (0)

#if defined(CONFIG_BUFFER_DEBUG)
void buffer_assertion_failure(struct buffer_head *bh);
#define J_ASSERT_BH(bh, expr)						\
	do {								\
		if (!(expr))						\
			buffer_assertion_failure(bh);			\
		J_ASSERT(expr);						\
	} while (0)
#define J_ASSERT_JH(jh, expr)	J_ASSERT_BH(jh2bh(jh), expr)
#else
#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)
#endif

#else
#define J_ASSERT(assert)	do { } while (0)
#endif		/* JBD2_ASSERTIONS */

#if defined(JBD2_PARANOID_IOFAIL)
#define J_EXPECT(expr, why...)		J_ASSERT(expr)
#define J_EXPECT_BH(bh, expr, why...)	J_ASSERT_BH(bh, expr)
#define J_EXPECT_JH(jh, expr, why...)	J_ASSERT_JH(jh, expr)
#else
#define __journal_expect(expr, why...)					     \
	({								     \
		int val = (expr);					     \
		if (!val) {						     \
			printk(KERN_ERR					     \
				"EXT3-fs unexpected failure: %s;\n",# expr); \
			printk(KERN_ERR why "\n");			     \
		}							     \
		val;							     \
	})
#define J_EXPECT(expr, why...)		__journal_expect(expr, ## why)
#define J_EXPECT_BH(bh, expr, why...)	__journal_expect(expr, ## why)
#define J_EXPECT_JH(jh, expr, why...)	__journal_expect(expr, ## why)
#endif

enum jbd_state_bits {
	BH_JBD			/* Has an attached ext3 journal_head */
	  = BH_PrivateStart,
	BH_JWrite,		/* Being written to log (@@@ DEBUGGING) */
	BH_Freed,		/* Has been freed (truncated) */
	BH_Revoked,		/* Has been revoked from the log */
	BH_RevokeValid,		/* Revoked flag is valid */
	BH_JBDDirty,		/* Is dirty but journaled */
	BH_State,		/* Pins most journal_head state */
	BH_JournalHead,		/* Pins bh->b_private and jh->b_bh */
	BH_Unshadow,		/* Dummy bit, for BJ_Shadow wakeup filtering */
};

BUFFER_FNS(JBD, jbd)
BUFFER_FNS(JWrite, jwrite)
BUFFER_FNS(JBDDirty, jbddirty)
TAS_BUFFER_FNS(JBDDirty, jbddirty)
BUFFER_FNS(Revoked, revoked)
TAS_BUFFER_FNS(Revoked, revoked)
BUFFER_FNS(RevokeValid, revokevalid)
TAS_BUFFER_FNS(RevokeValid, revokevalid)
BUFFER_FNS(Freed, freed)

static inline struct buffer_head *jh2bh(struct journal_head *jh)
{
	return jh->b_bh;
}

static inline struct journal_head *bh2jh(struct buffer_head *bh)
{
	return bh->b_private;
}

static inline void jbd_lock_bh_state(struct buffer_head *bh)
{
	bit_spin_lock(BH_State, &bh->b_state);
}

static inline int jbd_trylock_bh_state(struct buffer_head *bh)
{
	return bit_spin_trylock(BH_State, &bh->b_state);
}

static inline int jbd_is_locked_bh_state(struct buffer_head *bh)
{
	return bit_spin_is_locked(BH_State, &bh->b_state);
}

static inline void jbd_unlock_bh_state(struct buffer_head *bh)
{
	bit_spin_unlock(BH_State, &bh->b_state);
}

static inline void jbd_lock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_lock(BH_JournalHead, &bh->b_state);
}

static inline void jbd_unlock_bh_journal_head(struct buffer_head *bh)
{
	bit_spin_unlock(BH_JournalHead, &bh->b_state);
}

struct jbd2_revoke_table_s;

/**
 * struct handle_s - The handle_s type is the concrete type associated with
 *     handle_t.
 * @h_transaction: Which compound transaction is this update a part of?
 * @h_buffer_credits: Number of remaining buffers we are allowed to dirty.
 * @h_ref: Reference count on this handle
 * @h_err: Field for caller's use to track errors through large fs operations
 * @h_sync: flag for sync-on-close
 * @h_jdata: flag to force data journaling
 * @h_aborted: flag indicating fatal error on handle
 **/

/* Docbook can't yet cope with the bit fields, but will leave the documentation
 * in so it can be fixed later.
 */
/**
 * 日志句柄
 */
struct handle_s
{
	/* Which compound transaction is this update a part of? */
	/* 当前句柄相关的事务 */
	transaction_t		*h_transaction;

	/* Number of remaining buffers we are allowed to dirty: */
	/* 日志操作还有多少空闲缓冲区 */
	int			h_buffer_credits;

	/* Reference count on this handle */
	int			h_ref;

	/* Field for caller's use to track errors through large fs */
	/* operations */
	int			h_err;

	/* Flags [no locking] */
	unsigned int	h_sync:		1;	/* sync-on-close */
	unsigned int	h_jdata:	1;	/* force data journaling */
	unsigned int	h_aborted:	1;	/* fatal error on handle */
};


/* The transaction_t type is the guts of the journaling mechanism.  It
 * tracks a compound transaction through its various states:
 *
 * RUNNING:	accepting new updates
 * LOCKED:	Updates still running but we don't accept new ones
 * RUNDOWN:	Updates are tidying up but have finished requesting
 *		new buffers to modify (state not used for now)
 * FLUSH:       All updates complete, but we are still writing to disk
 * COMMIT:      All data on disk, writing commit record
 * FINISHED:	We still have to keep the transaction for checkpointing.
 *
 * The transaction keeps track of all of the buffers modified by a
 * running transaction, and all of the buffers committed but not yet
 * flushed to home for finished transactions.
 */

/*
 * Lock ranking:
 *
 *    j_list_lock
 *      ->jbd_lock_bh_journal_head()	(This is "innermost")
 *
 *    j_state_lock
 *    ->jbd_lock_bh_state()
 *
 *    jbd_lock_bh_state()
 *    ->j_list_lock
 *
 *    j_state_lock
 *    ->t_handle_lock
 *
 *    j_state_lock
 *    ->j_list_lock			(journal_unmap_buffer)
 *
 */

/**
 * 文件系统事务描述符
 */
struct transaction_s
{
	/* Pointer to the journal for this transaction. [no locking] */
	/* 事务所属的日志 */
	journal_t		*t_journal;

	/* Sequence number for this transaction [no locking] */
	tid_t			t_tid;

	/*
	 * Transaction's current state
	 * [no locking - only kjournald2 alters this]
	 * FIXME: needs barriers
	 * KLUDGE: [use j_state_lock]
	 */
	/* 事务状态 */
	enum {
		/* 可以向日志添加原子句柄 */
		T_RUNNING,
		T_LOCKED,
		T_RUNDOWN,
		/* 正在将事务写到磁盘 */
		T_FLUSH,
		/* 日志写入完毕，但是还需要处理元数据 */
		T_COMMIT,
		/* 所有日志项均已经安全的写到磁盘 */
		T_FINISHED
	}			t_state;

	/*
	 * Where in the log does this transaction's commit start? [no locking]
	 */
	unsigned long		t_log_start;

	/* Number of buffers on the t_buffers list [j_list_lock] */
	int			t_nr_buffers;

	/*
	 * Doubly-linked circular list of all buffers reserved but not yet
	 * modified by this transaction [j_list_lock]
	 */
	struct journal_head	*t_reserved_list;

	/*
	 * Doubly-linked circular list of all buffers under writeout during
	 * commit [j_list_lock]
	 */
	struct journal_head	*t_locked_list;

	/*
	 * Doubly-linked circular list of all metadata buffers owned by this
	 * transaction [j_list_lock]
	 */
	/* 该事务关联的缓冲区 */
	struct journal_head	*t_buffers;

	/*
	 * Doubly-linked circular list of all data buffers still to be
	 * flushed before this transaction can be committed [j_list_lock]
	 */
	struct journal_head	*t_sync_datalist;

	/*
	 * Doubly-linked circular list of all forget buffers (superseded
	 * buffers which we can un-checkpoint once this transaction commits)
	 * [j_list_lock]
	 */
	struct journal_head	*t_forget;

	/*
	 * Doubly-linked circular list of all buffers still to be flushed before
	 * this transaction can be checkpointed. [j_list_lock]
	 */
	struct journal_head	*t_checkpoint_list;

	/*
	 * Doubly-linked circular list of all buffers submitted for IO while
	 * checkpointing. [j_list_lock]
	 */
	struct journal_head	*t_checkpoint_io_list;

	/*
	 * Doubly-linked circular list of temporary buffers currently undergoing
	 * IO in the log [j_list_lock]
	 */
	struct journal_head	*t_iobuf_list;

	/*
	 * Doubly-linked circular list of metadata buffers being shadowed by log
	 * IO.  The IO buffers on the iobuf list and the shadow buffers on this
	 * list match each other one for one at all times. [j_list_lock]
	 */
	struct journal_head	*t_shadow_list;

	/*
	 * Doubly-linked circular list of control buffers being written to the
	 * log. [j_list_lock]
	 */
	struct journal_head	*t_log_list;

	/*
	 * Protects info related to handles
	 */
	spinlock_t		t_handle_lock;

	/*
	 * Number of outstanding updates running on this transaction
	 * [t_handle_lock]
	 */
	int			t_updates;

	/*
	 * Number of buffers reserved for use by all handles in this transaction
	 * handle but not yet modified. [t_handle_lock]
	 */
	int			t_outstanding_credits;

	/*
	 * Forward and backward links for the circular list of all transactions
	 * awaiting checkpoint. [j_list_lock]
	 */
	transaction_t		*t_cpnext, *t_cpprev;

	/*
	 * When will the transaction expire (become due for commit), in jiffies?
	 * [no locking]
	 */
	/* 超时时间，默认5秒内需要将日志写到物理设备 */
	unsigned long		t_expires;

	/*
	 * How many handles used this transaction? [t_handle_lock]
	 */
	/* 与事务关联的句柄数量 */
	int t_handle_count;

};

/**
 * struct journal_s - The journal_s type is the concrete type associated with
 *     journal_t.
 * @j_flags:  General journaling state flags
 * @j_errno:  Is there an outstanding uncleared error on the journal (from a
 *     prior abort)?
 * @j_sb_buffer: First part of superblock buffer
 * @j_superblock: Second part of superblock buffer
 * @j_format_version: Version of the superblock format
 * @j_state_lock: Protect the various scalars in the journal
 * @j_barrier_count:  Number of processes waiting to create a barrier lock
 * @j_barrier: The barrier lock itself
 * @j_running_transaction: The current running transaction..
 * @j_committing_transaction: the transaction we are pushing to disk
 * @j_checkpoint_transactions: a linked circular list of all transactions
 *  waiting for checkpointing
 * @j_wait_transaction_locked: Wait queue for waiting for a locked transaction
 *  to start committing, or for a barrier lock to be released
 * @j_wait_logspace: Wait queue for waiting for checkpointing to complete
 * @j_wait_done_commit: Wait queue for waiting for commit to complete
 * @j_wait_checkpoint:  Wait queue to trigger checkpointing
 * @j_wait_commit: Wait queue to trigger commit
 * @j_wait_updates: Wait queue to wait for updates to complete
 * @j_checkpoint_mutex: Mutex for locking against concurrent checkpoints
 * @j_head: Journal head - identifies the first unused block in the journal
 * @j_tail: Journal tail - identifies the oldest still-used block in the
 *  journal.
 * @j_free: Journal free - how many free blocks are there in the journal?
 * @j_first: The block number of the first usable block
 * @j_last: The block number one beyond the last usable block
 * @j_dev: Device where we store the journal
 * @j_blocksize: blocksize for the location where we store the journal.
 * @j_blk_offset: starting block offset for into the device where we store the
 *     journal
 * @j_fs_dev: Device which holds the client fs.  For internal journal this will
 *     be equal to j_dev
 * @j_maxlen: Total maximum capacity of the journal region on disk.
 * @j_list_lock: Protects the buffer lists and internal buffer state.
 * @j_inode: Optional inode where we store the journal.  If present, all journal
 *     block numbers are mapped into this inode via bmap().
 * @j_tail_sequence:  Sequence number of the oldest transaction in the log
 * @j_transaction_sequence: Sequence number of the next transaction to grant
 * @j_commit_sequence: Sequence number of the most recently committed
 *  transaction
 * @j_commit_request: Sequence number of the most recent transaction wanting
 *     commit
 * @j_uuid: Uuid of client object.
 * @j_task: Pointer to the current commit thread for this journal
 * @j_max_transaction_buffers:  Maximum number of metadata buffers to allow in a
 *     single compound commit transaction
 * @j_commit_interval: What is the maximum transaction lifetime before we begin
 *  a commit?
 * @j_commit_timer:  The timer used to wakeup the commit thread
 * @j_revoke_lock: Protect the revoke table
 * @j_revoke: The revoke table - maintains the list of revoked blocks in the
 *     current transaction.
 * @j_revoke_table: alternate revoke tables for j_revoke
 * @j_wbuf: array of buffer_heads for jbd2_journal_commit_transaction
 * @j_wbufsize: maximum number of buffer_heads allowed in j_wbuf, the
 *	number that will fit in j_blocksize
 * @j_last_sync_writer: most recent pid which did a synchronous write
 * @j_private: An opaque pointer to fs-private information.
 */

struct journal_s
{
	/* General journaling state flags [j_state_lock] */
	unsigned long		j_flags;

	/*
	 * Is there an outstanding uncleared error on the journal (from a prior
	 * abort)? [j_state_lock]
	 */
	int			j_errno;

	/* The superblock buffer */
	struct buffer_head	*j_sb_buffer;
	journal_superblock_t	*j_superblock;

	/* Version of the superblock format */
	int			j_format_version;

	/*
	 * Protect the various scalars in the journal
	 */
	spinlock_t		j_state_lock;

	/*
	 * Number of processes waiting to create a barrier lock [j_state_lock]
	 */
	int			j_barrier_count;

	/* The barrier lock itself */
	struct mutex		j_barrier;

	/*
	 * Transactions: The current running transaction...
	 * [j_state_lock] [caller holding open handle]
	 */
	transaction_t		*j_running_transaction;

	/*
	 * the transaction we are pushing to disk
	 * [j_state_lock] [caller holding open handle]
	 */
	transaction_t		*j_committing_transaction;

	/*
	 * ... and a linked circular list of all transactions waiting for
	 * checkpointing. [j_list_lock]
	 */
	transaction_t		*j_checkpoint_transactions;

	/*
	 * Wait queue for waiting for a locked transaction to start committing,
	 * or for a barrier lock to be released
	 */
	wait_queue_head_t	j_wait_transaction_locked;

	/* Wait queue for waiting for checkpointing to complete */
	wait_queue_head_t	j_wait_logspace;

	/* Wait queue for waiting for commit to complete */
	wait_queue_head_t	j_wait_done_commit;

	/* Wait queue to trigger checkpointing */
	wait_queue_head_t	j_wait_checkpoint;

	/* Wait queue to trigger commit */
	wait_queue_head_t	j_wait_commit;

	/* Wait queue to wait for updates to complete */
	wait_queue_head_t	j_wait_updates;

	/* Semaphore for locking against concurrent checkpoints */
	struct mutex		j_checkpoint_mutex;

	/*
	 * Journal head: identifies the first unused block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_head;

	/*
	 * Journal tail: identifies the oldest still-used block in the journal.
	 * [j_state_lock]
	 */
	unsigned long		j_tail;

	/*
	 * Journal free: how many free blocks are there in the journal?
	 * [j_state_lock]
	 */
	unsigned long		j_free;

	/*
	 * Journal start and end: the block numbers of the first usable block
	 * and one beyond the last usable block in the journal. [j_state_lock]
	 */
	unsigned long		j_first;
	unsigned long		j_last;

	/*
	 * Device, blocksize and starting block offset for the location where we
	 * store the journal.
	 */
	struct block_device	*j_dev;
	int			j_blocksize;
	unsigned long long		j_blk_offset;

	/*
	 * Device which holds the client fs.  For internal journal this will be
	 * equal to j_dev.
	 */
	struct block_device	*j_fs_dev;

	/* Total maximum capacity of the journal region on disk. */
	unsigned int		j_maxlen;

	/*
	 * Protects the buffer lists and internal buffer state.
	 */
	spinlock_t		j_list_lock;

	/* Optional inode where we store the journal.  If present, all */
	/* journal block numbers are mapped into this inode via */
	/* bmap(). */
	struct inode		*j_inode;

	/*
	 * Sequence number of the oldest transaction in the log [j_state_lock]
	 */
	tid_t			j_tail_sequence;

	/*
	 * Sequence number of the next transaction to grant [j_state_lock]
	 */
	tid_t			j_transaction_sequence;

	/*
	 * Sequence number of the most recently committed transaction
	 * [j_state_lock].
	 */
	tid_t			j_commit_sequence;

	/*
	 * Sequence number of the most recent transaction wanting commit
	 * [j_state_lock]
	 */
	tid_t			j_commit_request;

	/*
	 * Journal uuid: identifies the object (filesystem, LVM volume etc)
	 * backed by this journal.  This will eventually be replaced by an array
	 * of uuids, allowing us to index multiple devices within a single
	 * journal and to perform atomic updates across them.
	 */
	__u8			j_uuid[16];

	/* Pointer to the current commit thread for this journal */
	struct task_struct	*j_task;

	/*
	 * Maximum number of metadata buffers to allow in a single compound
	 * commit transaction
	 */
	int			j_max_transaction_buffers;

	/*
	 * What is the maximum transaction lifetime before we begin a commit?
	 */
	unsigned long		j_commit_interval;

	/* The timer used to wakeup the commit thread: */
	struct timer_list	j_commit_timer;

	/*
	 * The revoke table: maintains the list of revoked blocks in the
	 * current transaction.  [j_revoke_lock]
	 */
	spinlock_t		j_revoke_lock;
	struct jbd2_revoke_table_s *j_revoke;
	struct jbd2_revoke_table_s *j_revoke_table[2];

	/*
	 * array of bhs for jbd2_journal_commit_transaction
	 */
	struct buffer_head	**j_wbuf;
	int			j_wbufsize;

	pid_t			j_last_sync_writer;

	/*
	 * An opaque pointer to fs-private information.  ext3 puts its
	 * superblock pointer here
	 */
	void *j_private;
};

/*
 * Journal flag definitions
 */
#define JBD2_UNMOUNT	0x001	/* Journal thread is being destroyed */
#define JBD2_ABORT	0x002	/* Journaling has been aborted for errors. */
#define JBD2_ACK_ERR	0x004	/* The errno in the sb has been acked */
#define JBD2_FLUSHED	0x008	/* The journal superblock has been flushed */
#define JBD2_LOADED	0x010	/* The journal superblock has been loaded */
#define JBD2_BARRIER	0x020	/* Use IDE barriers */

/*
 * Function declarations for the journaling transaction and buffer
 * management
 */

/* Filing buffers */
extern void jbd2_journal_unfile_buffer(journal_t *, struct journal_head *);
extern void __jbd2_journal_unfile_buffer(struct journal_head *);
extern void __jbd2_journal_refile_buffer(struct journal_head *);
extern void jbd2_journal_refile_buffer(journal_t *, struct journal_head *);
extern void __jbd2_journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_free_buffer(struct journal_head *bh);
extern void jbd2_journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_clean_data_list(transaction_t *transaction);

/* Log buffer allocation */
extern struct journal_head * jbd2_journal_get_descriptor_buffer(journal_t *);
int jbd2_journal_next_log_block(journal_t *, unsigned long long *);

/* Commit management */
extern void jbd2_journal_commit_transaction(journal_t *);

/* Checkpoint list management */
int __jbd2_journal_clean_checkpoint_list(journal_t *journal);
int __jbd2_journal_remove_checkpoint(struct journal_head *);
void __jbd2_journal_insert_checkpoint(struct journal_head *, transaction_t *);

/* Buffer IO */
extern int
jbd2_journal_write_metadata_buffer(transaction_t	  *transaction,
			      struct journal_head  *jh_in,
			      struct journal_head **jh_out,
			      unsigned long long   blocknr);

/* Transaction locking */
extern void		__wait_on_journal (journal_t *);

/*
 * Journal locking.
 *
 * We need to lock the journal during transaction state changes so that nobody
 * ever tries to take a handle on the running transaction while we are in the
 * middle of moving it to the commit phase.  j_state_lock does this.
 *
 * Note that the locking is completely interrupt unsafe.  We never touch
 * journal structures from interrupts.
 */

/**
 * 获得当前进程的日志句柄
 */
static inline handle_t *journal_current_handle(void)
{
	return current->journal_info;
}

/* The journaling code user interface:
 *
 * Create and destroy handles
 * Register buffer modifications against the current transaction.
 */

extern handle_t *jbd2_journal_start(journal_t *, int nblocks);
extern int	 jbd2_journal_restart (handle_t *, int nblocks);
extern int	 jbd2_journal_extend (handle_t *, int nblocks);
extern int	 jbd2_journal_get_write_access(handle_t *, struct buffer_head *);
extern int	 jbd2_journal_get_create_access (handle_t *, struct buffer_head *);
extern int	 jbd2_journal_get_undo_access(handle_t *, struct buffer_head *);
extern int	 jbd2_journal_dirty_data (handle_t *, struct buffer_head *);
extern int	 jbd2_journal_dirty_metadata (handle_t *, struct buffer_head *);
extern void	 jbd2_journal_release_buffer (handle_t *, struct buffer_head *);
extern int	 jbd2_journal_forget (handle_t *, struct buffer_head *);
extern void	 journal_sync_buffer (struct buffer_head *);
extern void	 jbd2_journal_invalidatepage(journal_t *,
				struct page *, unsigned long);
extern int	 jbd2_journal_try_to_free_buffers(journal_t *, struct page *, gfp_t);
extern int	 jbd2_journal_stop(handle_t *);
extern int	 jbd2_journal_flush (journal_t *);
extern void	 jbd2_journal_lock_updates (journal_t *);
extern void	 jbd2_journal_unlock_updates (journal_t *);

extern journal_t * jbd2_journal_init_dev(struct block_device *bdev,
				struct block_device *fs_dev,
				unsigned long long start, int len, int bsize);
extern journal_t * jbd2_journal_init_inode (struct inode *);
extern int	   jbd2_journal_update_format (journal_t *);
extern int	   jbd2_journal_check_used_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_check_available_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_set_features
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   jbd2_journal_create     (journal_t *);
extern int	   jbd2_journal_load       (journal_t *journal);
extern void	   jbd2_journal_destroy    (journal_t *);
extern int	   jbd2_journal_recover    (journal_t *journal);
extern int	   jbd2_journal_wipe       (journal_t *, int);
extern int	   jbd2_journal_skip_recovery	(journal_t *);
extern void	   jbd2_journal_update_superblock	(journal_t *, int);
extern void	   __jbd2_journal_abort_hard	(journal_t *);
extern void	   jbd2_journal_abort      (journal_t *, int);
extern int	   jbd2_journal_errno      (journal_t *);
extern void	   jbd2_journal_ack_err    (journal_t *);
extern int	   jbd2_journal_clear_err  (journal_t *);
extern int	   jbd2_journal_bmap(journal_t *, unsigned long, unsigned long long *);
extern int	   jbd2_journal_force_commit(journal_t *);

/*
 * journal_head management
 */
struct journal_head *jbd2_journal_add_journal_head(struct buffer_head *bh);
struct journal_head *jbd2_journal_grab_journal_head(struct buffer_head *bh);
void jbd2_journal_remove_journal_head(struct buffer_head *bh);
void jbd2_journal_put_journal_head(struct journal_head *jh);

/*
 * handle management
 */
extern struct kmem_cache *jbd2_handle_cache;

static inline handle_t *jbd2_alloc_handle(gfp_t gfp_flags)
{
	return kmem_cache_alloc(jbd2_handle_cache, gfp_flags);
}

static inline void jbd2_free_handle(handle_t *handle)
{
	kmem_cache_free(jbd2_handle_cache, handle);
}

/* Primary revoke support */
#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   jbd2_journal_init_revoke(journal_t *, int);
extern void	   jbd2_journal_destroy_revoke_caches(void);
extern int	   jbd2_journal_init_revoke_caches(void);

extern void	   jbd2_journal_destroy_revoke(journal_t *);
extern int	   jbd2_journal_revoke (handle_t *, unsigned long long, struct buffer_head *);
extern int	   jbd2_journal_cancel_revoke(handle_t *, struct journal_head *);
extern void	   jbd2_journal_write_revoke_records(journal_t *, transaction_t *);

/* Recovery revoke support */
extern int	jbd2_journal_set_revoke(journal_t *, unsigned long long, tid_t);
extern int	jbd2_journal_test_revoke(journal_t *, unsigned long long, tid_t);
extern void	jbd2_journal_clear_revoke(journal_t *);
extern void	jbd2_journal_switch_revoke_table(journal_t *journal);

/*
 * The log thread user interface:
 *
 * Request space in the current transaction, and force transaction commit
 * transitions on demand.
 */

int __jbd2_log_space_left(journal_t *); /* Called with journal locked */
int jbd2_log_start_commit(journal_t *journal, tid_t tid);
int __jbd2_log_start_commit(journal_t *journal, tid_t tid);
int jbd2_journal_start_commit(journal_t *journal, tid_t *tid);
int jbd2_journal_force_commit_nested(journal_t *journal);
int jbd2_log_wait_commit(journal_t *journal, tid_t tid);
int jbd2_log_do_checkpoint(journal_t *journal);

void __jbd2_log_wait_for_space(journal_t *journal);
extern void	__jbd2_journal_drop_transaction(journal_t *, transaction_t *);
extern int	jbd2_cleanup_journal_tail(journal_t *);

/* Debugging code only: */

#define jbd_ENOSYS() \
do {								           \
	printk (KERN_ERR "JBD unimplemented function %s\n", __FUNCTION__); \
	current->state = TASK_UNINTERRUPTIBLE;			           \
	schedule();						           \
} while (1)

/*
 * is_journal_abort
 *
 * Simple test wrapper function to test the JBD2_ABORT state flag.  This
 * bit, when set, indicates that we have had a fatal error somewhere,
 * either inside the journaling layer or indicated to us by the client
 * (eg. ext3), and that we and should not commit any further
 * transactions.
 */

static inline int is_journal_aborted(journal_t *journal)
{
	return journal->j_flags & JBD2_ABORT;
}

static inline int is_handle_aborted(handle_t *handle)
{
	if (handle->h_aborted)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void jbd2_journal_abort_handle(handle_t *handle)
{
	handle->h_aborted = 1;
}

#endif /* __KERNEL__   */

/* Comparison functions for transaction IDs: perform comparisons using
 * modulo arithmetic so that they work over sequence number wraps. */

static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int jbd2_journal_blocks_per_page(struct inode *inode);
extern size_t journal_tag_bytes(journal_t *journal);

/*
 * Return the minimum number of blocks which must be free in the journal
 * before a new transaction may be started.  Must be called under j_state_lock.
 */
static inline int jbd_space_needed(journal_t *journal)
{
	int nblocks = journal->j_max_transaction_buffers;
	if (journal->j_committing_transaction)
		nblocks += journal->j_committing_transaction->
					t_outstanding_credits;
	return nblocks;
}

/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types */
#define BJ_None		0	/* Not journaled */
#define BJ_SyncData	1	/* Normal data: flush before commit */
#define BJ_Metadata	2	/* Normal journaled metadata */
#define BJ_Forget	3	/* Buffer superseded by this transaction */
#define BJ_IO		4	/* Buffer is for temporary IO use */
#define BJ_Shadow	5	/* Buffer contents being shadowed to the log */
#define BJ_LogCtl	6	/* Buffer contains log descriptors */
#define BJ_Reserved	7	/* Buffer is reserved for access by journal */
#define BJ_Locked	8	/* Locked for I/O during commit */
#define BJ_Types	9

extern int jbd_blocks_per_page(struct inode *inode);

#ifdef __KERNEL__

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	/* __KERNEL__ */

#endif	/* _LINUX_JBD2_H */

/*
 *  linux/include/linux/ext2_fs_sb.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs_sb.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _LINUX_EXT2_FS_SB
#define _LINUX_EXT2_FS_SB

#include <linux/blockgroup_lock.h>
#include <linux/percpu_counter.h>
#include <linux/rbtree.h>

/* XXX Here for now... not interested in restructing headers JUST now */

/* data type for block offset of block group */
typedef int ext2_grpblk_t;

/* data type for filesystem-wide blocks number */
typedef unsigned long ext2_fsblk_t;

#define E2FSBLK "%lu"

/**
 * ext2文件系统保留空间描述符
 */
struct ext2_reserve_window {
	ext2_fsblk_t		_rsv_start;	/* First byte reserved */
	ext2_fsblk_t		_rsv_end;	/* Last byte reserved or 0 */
};

struct ext2_reserve_window_node {
	/* 通过此字段将节点添加到超级块的红黑树中 */
	struct rb_node	 	rsv_node;
	/* 预留窗口的长度，可通过ioctl设置 */
	__u32			rsv_goal_size;
	/* 命中数 */
	__u32			rsv_alloc_hit;
	/* 预留的起始、结束点 */
	struct ext2_reserve_window	rsv_window;
};

struct ext2_block_alloc_info {
	/* information about reservation window */
	struct ext2_reserve_window_node	rsv_window_node;
	/*
	 * was i_next_alloc_block in ext2_inode_info
	 * is the logical (file-relative) number of the
	 * most-recently-allocated block in this file.
	 * We use this for detecting linearly ascending allocation requests.
	 */
	/* 上一次分配的逻辑块号 */
	__u32			last_alloc_logical_block;
	/*
	 * Was i_next_alloc_goal in ext2_inode_info
	 * is the *physical* companion to i_next_alloc_block.
	 * it the the physical block number of the block which was most-recentl
	 * allocated to this file.  This give us the goal (target) for the next
	 * allocation when we detect linearly ascending requests.
	 */
	ext2_fsblk_t		last_alloc_physical_block;
};

#define rsv_start rsv_window._rsv_start
#define rsv_end rsv_window._rsv_end

/*
 * second extended-fs super-block data in memory
 */
/**
 * ext2文件系统超级块，在内存中的描述符
 */
struct ext2_sb_info {
	unsigned long s_frag_size;	/* Size of a fragment in bytes */
	unsigned long s_frags_per_block;/* Number of fragments per block */
	unsigned long s_inodes_per_block;/* Number of inodes per block */
	unsigned long s_frags_per_group;/* Number of fragments in a group */
	unsigned long s_blocks_per_group;/* Number of blocks in a group */
	unsigned long s_inodes_per_group;/* Number of inodes in a group */
	unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
	unsigned long s_gdb_count;	/* Number of group descriptor blocks */
	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
	unsigned long s_groups_count;	/* Number of groups in the fs */
	/* 上一次计算的管理数据的块数和完全可用的块数 */
	unsigned long s_overhead_last;  /* Last calculated overhead */
	unsigned long s_blocks_last;    /* Last seen block count */
	struct buffer_head * s_sbh;	/* Buffer containing the super block */
	/* 磁盘块中的超级块原始数据指针 */
	struct ext2_super_block * s_es;	/* Pointer to the super block in the buffer */
	struct buffer_head ** s_group_desc;
	/* 装载选项 */
	unsigned long  s_mount_opt;
	/* 从哪一个块中读取的超级块，因为ext2有多份超级块的拷贝 */
	unsigned long s_sb_block;
	uid_t s_resuid;
	gid_t s_resgid;
	/* 当前的装载状态 */
	unsigned short s_mount_state;
	unsigned short s_pad;
	int s_addr_per_block_bits;
	int s_desc_per_block_bits;
	int s_inode_size;
	int s_first_ino;
	spinlock_t s_next_gen_lock;
	u32 s_next_generation;
	/* 目录总数，装载时确定 */
	unsigned long s_dir_count;
	/* 一个数组，每个元素与一个块组对应，orlov分配器用于保持块组中文件和目录之间保持平衡 */
	u8 *s_debts;
	/* 空闲块、inode和目录数目的近似计数器 */
	struct percpu_counter s_freeblocks_counter;
	struct percpu_counter s_freeinodes_counter;
	struct percpu_counter s_dirs_counter;
	struct blockgroup_lock s_blockgroup_lock;
	/* root of the per fs reservation window tree */
	spinlock_t s_rsv_window_lock;
	/* 红黑树，表示所有的预分配 */
	struct rb_root s_rsv_window_root;
	struct ext2_reserve_window_node s_rsv_window_head;
};

#endif	/* _LINUX_EXT2_FS_SB */

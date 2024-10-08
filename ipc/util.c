/*
 * linux/ipc/util.c
 * Copyright (C) 1992 Krishna Balasubramanian
 *
 * Sep 1997 - Call suser() last after "normal" permission checks so we
 *            get BSD style process accounting right.
 *            Occurs in several places in the IPC code.
 *            Chris Evans, <chris@ferret.lmh.ox.ac.uk>
 * Nov 1999 - ipc helper functions, unified SMP locking
 *	      Manfred Spraul <manfred@colorfullife.com>
 * Oct 2002 - One lock per IPC id. RCU ipc_free for lock-free grow_ary().
 *            Mingming Cao <cmm@us.ibm.com>
 * Mar 2006 - support for audit of ipc object properties
 *            Dustin Kirkland <dustin.kirkland@us.ibm.com>
 * Jun 2006 - namespaces ssupport
 *            OpenVZ, SWsoft Inc.
 *            Pavel Emelianov <xemul@openvz.org>
 */

#include <linux/mm.h>
#include <linux/shm.h>
#include <linux/init.h>
#include <linux/msg.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/capability.h>
#include <linux/highuid.h>
#include <linux/security.h>
#include <linux/rcupdate.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/audit.h>
#include <linux/nsproxy.h>
#include <linux/rwsem.h>

#include <asm/unistd.h>

#include "util.h"

struct ipc_proc_iface {
	const char *path;
	const char *header;
	int ids;
	int (*show)(struct seq_file *, void *);
};

struct ipc_namespace init_ipc_ns = {
	.kref = {
		.refcount	= ATOMIC_INIT(2),
	},
};

static struct ipc_namespace *clone_ipc_ns(struct ipc_namespace *old_ns)
{
	int err;
	struct ipc_namespace *ns;

	err = -ENOMEM;
	ns = kmalloc(sizeof(struct ipc_namespace), GFP_KERNEL);
	if (ns == NULL)
		goto err_mem;

	err = sem_init_ns(ns);
	if (err)
		goto err_sem;
	err = msg_init_ns(ns);
	if (err)
		goto err_msg;
	err = shm_init_ns(ns);
	if (err)
		goto err_shm;

	kref_init(&ns->kref);
	return ns;

err_shm:
	msg_exit_ns(ns);
err_msg:
	sem_exit_ns(ns);
err_sem:
	kfree(ns);
err_mem:
	return ERR_PTR(err);
}

struct ipc_namespace *copy_ipcs(unsigned long flags, struct ipc_namespace *ns)
{
	struct ipc_namespace *new_ns;

	BUG_ON(!ns);
	get_ipc_ns(ns);

	if (!(flags & CLONE_NEWIPC))
		return ns;

	new_ns = clone_ipc_ns(ns);

	put_ipc_ns(ns);
	return new_ns;
}

void free_ipc_ns(struct kref *kref)
{
	struct ipc_namespace *ns;

	ns = container_of(kref, struct ipc_namespace, kref);
	sem_exit_ns(ns);
	msg_exit_ns(ns);
	shm_exit_ns(ns);
	kfree(ns);
}

/**
 *	ipc_init	-	initialise IPC subsystem
 *
 *	The various system5 IPC resources (semaphores, messages and shared
 *	memory) are initialised
 */
 
static int __init ipc_init(void)
{
	sem_init();
	msg_init();
	shm_init();
	return 0;
}
__initcall(ipc_init);

/**
 *	ipc_init_ids		-	initialise IPC identifiers
 *	@ids: Identifier set
 *
 *	Set up the sequence range to use for the ipc identifier range (limited
 *	below IPCMNI) then initialise the ids idr.
 */
 
void ipc_init_ids(struct ipc_ids *ids)
{
	init_rwsem(&ids->rw_mutex);

	ids->in_use = 0;
	ids->seq = 0;
	{
		int seq_limit = INT_MAX/SEQ_MULTIPLIER;
		if(seq_limit > USHRT_MAX)
			ids->seq_max = USHRT_MAX;
		 else
		 	ids->seq_max = seq_limit;
	}

	idr_init(&ids->ipcs_idr);
}

#ifdef CONFIG_PROC_FS
static const struct file_operations sysvipc_proc_fops;
/**
 *	ipc_init_proc_interface	-  Create a proc interface for sysipc types using a seq_file interface.
 *	@path: Path in procfs
 *	@header: Banner to be printed at the beginning of the file.
 *	@ids: ipc id table to iterate.
 *	@show: show routine.
 */
void __init ipc_init_proc_interface(const char *path, const char *header,
		int ids, int (*show)(struct seq_file *, void *))
{
	struct proc_dir_entry *pde;
	struct ipc_proc_iface *iface;

	iface = kmalloc(sizeof(*iface), GFP_KERNEL);
	if (!iface)
		return;
	iface->path	= path;
	iface->header	= header;
	iface->ids	= ids;
	iface->show	= show;

	pde = create_proc_entry(path,
				S_IRUGO,        /* world readable */
				NULL            /* parent dir */);
	if (pde) {
		pde->data = iface;
		pde->proc_fops = &sysvipc_proc_fops;
	} else {
		kfree(iface);
	}
}
#endif

/**
 *	ipc_findkey	-	find a key in an ipc identifier set	
 *	@ids: Identifier set
 *	@key: The key to find
 *	
 *	Requires ipc_ids.rw_mutex locked.
 *	Returns the LOCKED pointer to the ipc structure if found or NULL
 *	if not.
 *	If key is found ipc points to the owning ipc structure
 */
 
static struct kern_ipc_perm *ipc_findkey(struct ipc_ids *ids, key_t key)
{
	struct kern_ipc_perm *ipc;
	int next_id;
	int total;

	for (total = 0, next_id = 0; total < ids->in_use; next_id++) {
		ipc = idr_find(&ids->ipcs_idr, next_id);

		if (ipc == NULL)
			continue;

		if (ipc->key != key) {
			total++;
			continue;
		}

		ipc_lock_by_ptr(ipc);
		return ipc;
	}

	return NULL;
}

/**
 *	ipc_get_maxid 	-	get the last assigned id
 *	@ids: IPC identifier set
 *
 *	Called with ipc_ids.rw_mutex held.
 */

int ipc_get_maxid(struct ipc_ids *ids)
{
	struct kern_ipc_perm *ipc;
	int max_id = -1;
	int total, id;

	if (ids->in_use == 0)
		return -1;

	if (ids->in_use == IPCMNI)
		return IPCMNI - 1;

	/* Look for the last assigned id */
	total = 0;
	for (id = 0; id < IPCMNI && total < ids->in_use; id++) {
		ipc = idr_find(&ids->ipcs_idr, id);
		if (ipc != NULL) {
			max_id = id;
			total++;
		}
	}
	return max_id;
}

/**
 *	ipc_addid 	-	add an IPC identifier
 *	@ids: IPC identifier set
 *	@new: new IPC permission set
 *	@size: limit for the number of used ids
 *
 *	Add an entry 'new' to the IPC ids idr. The permissions object is
 *	initialised and the first free entry is set up and the id assigned
 *	is returned. The 'new' entry is returned in a locked state on success.
 *	On failure the entry is not locked and a negative err-code is returned.
 *
 *	Called with ipc_ids.rw_mutex held as a writer.
 */
 
int ipc_addid(struct ipc_ids* ids, struct kern_ipc_perm* new, int size)
{
	int id, err;

	if (size > IPCMNI)
		size = IPCMNI;

	if (ids->in_use >= size)
		return -ENOSPC;

	err = idr_get_new(&ids->ipcs_idr, new, &id);
	if (err)
		return err;

	ids->in_use++;

	new->cuid = new->uid = current->euid;
	new->gid = new->cgid = current->egid;

	new->seq = ids->seq++;
	if(ids->seq > ids->seq_max)
		ids->seq = 0;

	spin_lock_init(&new->lock);
	new->deleted = 0;
	rcu_read_lock();
	spin_lock(&new->lock);
	return id;
}

/**
 *	ipcget_new	-	create a new ipc object
 *	@ns: namespace
 *	@ids: IPC identifer set
 *	@ops: the actual creation routine to call
 *	@params: its parameters
 *
 *	This routine is called by sys_msgget, sys_semget() and sys_shmget()
 *	when the key is IPC_PRIVATE.
 */
int ipcget_new(struct ipc_namespace *ns, struct ipc_ids *ids,
		struct ipc_ops *ops, struct ipc_params *params)
{
	int err;
retry:
	err = idr_pre_get(&ids->ipcs_idr, GFP_KERNEL);

	if (!err)
		return -ENOMEM;

	down_write(&ids->rw_mutex);
	err = ops->getnew(ns, params);
	up_write(&ids->rw_mutex);

	if (err == -EAGAIN)
		goto retry;

	return err;
}

/**
 *	ipc_check_perms	-	check security and permissions for an IPC
 *	@ipcp: ipc permission set
 *	@ops: the actual security routine to call
 *	@params: its parameters
 *
 *	This routine is called by sys_msgget(), sys_semget() and sys_shmget()
 *      when the key is not IPC_PRIVATE and that key already exists in the
 *      ids IDR.
 *
 *	On success, the IPC id is returned.
 *
 *	It is called with ipc_ids.rw_mutex and ipcp->lock held.
 */
static int ipc_check_perms(struct kern_ipc_perm *ipcp, struct ipc_ops *ops,
			struct ipc_params *params)
{
	int err;

	if (ipcperms(ipcp, params->flg))
		err = -EACCES;
	else {
		err = ops->associate(ipcp, params->flg);
		if (!err)
			err = ipcp->id;
	}

	return err;
}

/**
 *	ipcget_public	-	get an ipc object or create a new one
 *	@ns: namespace
 *	@ids: IPC identifer set
 *	@ops: the actual creation routine to call
 *	@params: its parameters
 *
 *	This routine is called by sys_msgget, sys_semget() and sys_shmget()
 *	when the key is not IPC_PRIVATE.
 *	It adds a new entry if the key is not found and does some permission
 *      / security checkings if the key is found.
 *
 *	On success, the ipc id is returned.
 */
int ipcget_public(struct ipc_namespace *ns, struct ipc_ids *ids,
		struct ipc_ops *ops, struct ipc_params *params)
{
	struct kern_ipc_perm *ipcp;
	int flg = params->flg;
	int err;
retry:
	err = idr_pre_get(&ids->ipcs_idr, GFP_KERNEL);

	/*
	 * Take the lock as a writer since we are potentially going to add
	 * a new entry + read locks are not "upgradable"
	 */
	down_write(&ids->rw_mutex);
	ipcp = ipc_findkey(ids, params->key);
	if (ipcp == NULL) {
		/* key not used */
		if (!(flg & IPC_CREAT))
			err = -ENOENT;
		else if (!err)
			err = -ENOMEM;
		else
			err = ops->getnew(ns, params);
	} else {
		/* ipc object has been locked by ipc_findkey() */

		if (flg & IPC_CREAT && flg & IPC_EXCL)
			err = -EEXIST;
		else {
			err = 0;
			if (ops->more_checks)
				err = ops->more_checks(ipcp, params);
			if (!err)
				/*
				 * ipc_check_perms returns the IPC id on
				 * success
				 */
				err = ipc_check_perms(ipcp, ops, params);
		}
		ipc_unlock(ipcp);
	}
	up_write(&ids->rw_mutex);

	if (err == -EAGAIN)
		goto retry;

	return err;
}


/**
 *	ipc_rmid	-	remove an IPC identifier
 *	@ids: IPC identifier set
 *	@ipcp: ipc perm structure containing the identifier to remove
 *
 *	ipc_ids.rw_mutex (as a writer) and the spinlock for this ID are held
 *	before this function is called, and remain locked on the exit.
 */
 
void ipc_rmid(struct ipc_ids *ids, struct kern_ipc_perm *ipcp)
{
	int lid = ipcid_to_idx(ipcp->id);

	idr_remove(&ids->ipcs_idr, lid);

	ids->in_use--;

	ipcp->deleted = 1;

	return;
}

/**
 *	ipc_alloc	-	allocate ipc space
 *	@size: size desired
 *
 *	Allocate memory from the appropriate pools and return a pointer to it.
 *	NULL is returned if the allocation fails
 */
 
void* ipc_alloc(int size)
{
	void* out;
	if(size > PAGE_SIZE)
		out = vmalloc(size);
	else
		out = kmalloc(size, GFP_KERNEL);
	return out;
}

/**
 *	ipc_free        -       free ipc space
 *	@ptr: pointer returned by ipc_alloc
 *	@size: size of block
 *
 *	Free a block created with ipc_alloc(). The caller must know the size
 *	used in the allocation call.
 */

void ipc_free(void* ptr, int size)
{
	if(size > PAGE_SIZE)
		vfree(ptr);
	else
		kfree(ptr);
}

/*
 * rcu allocations:
 * There are three headers that are prepended to the actual allocation:
 * - during use: ipc_rcu_hdr.
 * - during the rcu grace period: ipc_rcu_grace.
 * - [only if vmalloc]: ipc_rcu_sched.
 * Their lifetime doesn't overlap, thus the headers share the same memory.
 * Unlike a normal union, they are right-aligned, thus some container_of
 * forward/backward casting is necessary:
 */
struct ipc_rcu_hdr
{
	int refcount;
	int is_vmalloc;
	void *data[0];
};


struct ipc_rcu_grace
{
	struct rcu_head rcu;
	/* "void *" makes sure alignment of following data is sane. */
	void *data[0];
};

struct ipc_rcu_sched
{
	struct work_struct work;
	/* "void *" makes sure alignment of following data is sane. */
	void *data[0];
};

#define HDRLEN_KMALLOC		(sizeof(struct ipc_rcu_grace) > sizeof(struct ipc_rcu_hdr) ? \
					sizeof(struct ipc_rcu_grace) : sizeof(struct ipc_rcu_hdr))
#define HDRLEN_VMALLOC		(sizeof(struct ipc_rcu_sched) > HDRLEN_KMALLOC ? \
					sizeof(struct ipc_rcu_sched) : HDRLEN_KMALLOC)

static inline int rcu_use_vmalloc(int size)
{
	/* Too big for a single page? */
	if (HDRLEN_KMALLOC + size > PAGE_SIZE)
		return 1;
	return 0;
}

/**
 *	ipc_rcu_alloc	-	allocate ipc and rcu space 
 *	@size: size desired
 *
 *	Allocate memory for the rcu header structure +  the object.
 *	Returns the pointer to the object.
 *	NULL is returned if the allocation fails. 
 */
 
void* ipc_rcu_alloc(int size)
{
	void* out;
	/* 
	 * We prepend the allocation with the rcu struct, and
	 * workqueue if necessary (for vmalloc). 
	 */
	if (rcu_use_vmalloc(size)) {
		out = vmalloc(HDRLEN_VMALLOC + size);
		if (out) {
			out += HDRLEN_VMALLOC;
			container_of(out, struct ipc_rcu_hdr, data)->is_vmalloc = 1;
			container_of(out, struct ipc_rcu_hdr, data)->refcount = 1;
		}
	} else {
		out = kmalloc(HDRLEN_KMALLOC + size, GFP_KERNEL);
		if (out) {
			out += HDRLEN_KMALLOC;
			container_of(out, struct ipc_rcu_hdr, data)->is_vmalloc = 0;
			container_of(out, struct ipc_rcu_hdr, data)->refcount = 1;
		}
	}

	return out;
}

void ipc_rcu_getref(void *ptr)
{
	container_of(ptr, struct ipc_rcu_hdr, data)->refcount++;
}

static void ipc_do_vfree(struct work_struct *work)
{
	vfree(container_of(work, struct ipc_rcu_sched, work));
}

/**
 * ipc_schedule_free - free ipc + rcu space
 * @head: RCU callback structure for queued work
 * 
 * Since RCU callback function is called in bh,
 * we need to defer the vfree to schedule_work().
 */
static void ipc_schedule_free(struct rcu_head *head)
{
	struct ipc_rcu_grace *grace;
	struct ipc_rcu_sched *sched;

	grace = container_of(head, struct ipc_rcu_grace, rcu);
	sched = container_of(&(grace->data[0]), struct ipc_rcu_sched,
				data[0]);

	INIT_WORK(&sched->work, ipc_do_vfree);
	schedule_work(&sched->work);
}

/**
 * ipc_immediate_free - free ipc + rcu space
 * @head: RCU callback structure that contains pointer to be freed
 *
 * Free from the RCU callback context.
 */
static void ipc_immediate_free(struct rcu_head *head)
{
	struct ipc_rcu_grace *free =
		container_of(head, struct ipc_rcu_grace, rcu);
	kfree(free);
}

void ipc_rcu_putref(void *ptr)
{
	if (--container_of(ptr, struct ipc_rcu_hdr, data)->refcount > 0)
		return;

	if (container_of(ptr, struct ipc_rcu_hdr, data)->is_vmalloc) {
		call_rcu(&container_of(ptr, struct ipc_rcu_grace, data)->rcu,
				ipc_schedule_free);
	} else {
		call_rcu(&container_of(ptr, struct ipc_rcu_grace, data)->rcu,
				ipc_immediate_free);
	}
}

/**
 *	ipcperms	-	check IPC permissions
 *	@ipcp: IPC permission set
 *	@flag: desired permission set.
 *
 *	Check user, group, other permissions for access
 *	to ipc resources. return 0 if allowed
 */
/* 检查是否有权限对ipc进行操作 */ 
int ipcperms (struct kern_ipc_perm *ipcp, short flag)
{	/* flag will most probably be 0 or S_...UGO from <linux/stat.h> */
	int requested_mode, granted_mode, err;

	if (unlikely((err = audit_ipc_obj(ipcp))))
		return err;
	/* 所请求的权限位 */
	requested_mode = (flag >> 6) | (flag >> 3) | flag;
	/* ipc允许的权限位 */
	granted_mode = ipcp->mode;
	/* 当前任务是信号的创建者、所有者 */
	if (current->euid == ipcp->cuid || current->euid == ipcp->uid)
		granted_mode >>= 6;
	/* 当前任务与创建者属于同一组 */
	else if (in_group_p(ipcp->cgid) || in_group_p(ipcp->gid))
		granted_mode >>= 3;
	/* is there some bit set in requested_mode but not in granted_mode? */
	if ((requested_mode & ~granted_mode & 0007) && /* 请求的权限不符 */
	    !capable(CAP_IPC_OWNER))/* 没有特权 */
		return -1;

	return security_ipc_permission(ipcp, flag);/* 由安全模块来确定是否有权限 */
}

/*
 * Functions to convert between the kern_ipc_perm structure and the
 * old/new ipc_perm structures
 */

/**
 *	kernel_to_ipc64_perm	-	convert kernel ipc permissions to user
 *	@in: kernel permissions
 *	@out: new style IPC permissions
 *
 *	Turn the kernel object @in into a set of permissions descriptions
 *	for returning to userspace (@out).
 */
 

void kernel_to_ipc64_perm (struct kern_ipc_perm *in, struct ipc64_perm *out)
{
	out->key	= in->key;
	out->uid	= in->uid;
	out->gid	= in->gid;
	out->cuid	= in->cuid;
	out->cgid	= in->cgid;
	out->mode	= in->mode;
	out->seq	= in->seq;
}

/**
 *	ipc64_perm_to_ipc_perm	-	convert new ipc permissions to old
 *	@in: new style IPC permissions
 *	@out: old style IPC permissions
 *
 *	Turn the new style permissions object @in into a compatibility
 *	object and store it into the @out pointer.
 */
 
void ipc64_perm_to_ipc_perm (struct ipc64_perm *in, struct ipc_perm *out)
{
	out->key	= in->key;
	SET_UID(out->uid, in->uid);
	SET_GID(out->gid, in->gid);
	SET_UID(out->cuid, in->cuid);
	SET_GID(out->cgid, in->cgid);
	out->mode	= in->mode;
	out->seq	= in->seq;
}

/**
 * ipc_lock - Lock an ipc structure without rw_mutex held
 * @ids: IPC identifier set
 * @id: ipc id to look for
 *
 * Look for an id in the ipc ids idr and lock the associated ipc object.
 *
 * The ipc object is locked on exit.
 *
 * This is the routine that should be called when the rw_mutex is not already
 * held, i.e. idr tree not protected: it protects the idr tree in read mode
 * during the idr_find().
 */

struct kern_ipc_perm *ipc_lock(struct ipc_ids *ids, int id)
{
	struct kern_ipc_perm *out;
	int lid = ipcid_to_idx(id);

	down_read(&ids->rw_mutex);

	rcu_read_lock();
	out = idr_find(&ids->ipcs_idr, lid);
	if (out == NULL) {
		rcu_read_unlock();
		up_read(&ids->rw_mutex);
		return ERR_PTR(-EINVAL);
	}

	up_read(&ids->rw_mutex);

	spin_lock(&out->lock);
	
	/* ipc_rmid() may have already freed the ID while ipc_lock
	 * was spinning: here verify that the structure is still valid
	 */
	if (out->deleted) {
		spin_unlock(&out->lock);
		rcu_read_unlock();
		return ERR_PTR(-EINVAL);
	}

	return out;
}

/**
 * ipc_lock_down - Lock an ipc structure with rw_sem held
 * @ids: IPC identifier set
 * @id: ipc id to look for
 *
 * Look for an id in the ipc ids idr and lock the associated ipc object.
 *
 * The ipc object is locked on exit.
 *
 * This is the routine that should be called when the rw_mutex is already
 * held, i.e. idr tree protected.
 */

struct kern_ipc_perm *ipc_lock_down(struct ipc_ids *ids, int id)
{
	struct kern_ipc_perm *out;
	int lid = ipcid_to_idx(id);

	rcu_read_lock();
	out = idr_find(&ids->ipcs_idr, lid);
	if (out == NULL) {
		rcu_read_unlock();
		return ERR_PTR(-EINVAL);
	}

	spin_lock(&out->lock);

	/*
	 * No need to verify that the structure is still valid since the
	 * rw_mutex is held.
	 */
	return out;
}

#ifdef __ARCH_WANT_IPC_PARSE_VERSION


/**
 *	ipc_parse_version	-	IPC call version
 *	@cmd: pointer to command
 *
 *	Return IPC_64 for new style IPC and IPC_OLD for old style IPC. 
 *	The @cmd value is turned from an encoding command and version into
 *	just the command code.
 */
 
int ipc_parse_version (int *cmd)
{
	if (*cmd & IPC_64) {
		*cmd ^= IPC_64;
		return IPC_64;
	} else {
		return IPC_OLD;
	}
}

#endif /* __ARCH_WANT_IPC_PARSE_VERSION */

#ifdef CONFIG_PROC_FS
struct ipc_proc_iter {
	struct ipc_namespace *ns;
	struct ipc_proc_iface *iface;
};

/*
 * This routine locks the ipc structure found at least at position pos.
 */
struct kern_ipc_perm *sysvipc_find_ipc(struct ipc_ids *ids, loff_t pos,
					loff_t *new_pos)
{
	struct kern_ipc_perm *ipc;
	int total, id;

	total = 0;
	for (id = 0; id < pos && total < ids->in_use; id++) {
		ipc = idr_find(&ids->ipcs_idr, id);
		if (ipc != NULL)
			total++;
	}

	if (total >= ids->in_use)
		return NULL;

	for ( ; pos < IPCMNI; pos++) {
		ipc = idr_find(&ids->ipcs_idr, pos);
		if (ipc != NULL) {
			*new_pos = pos + 1;
			ipc_lock_by_ptr(ipc);
			return ipc;
		}
	}

	/* Out of range - return NULL to terminate iteration */
	return NULL;
}

static void *sysvipc_proc_next(struct seq_file *s, void *it, loff_t *pos)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct kern_ipc_perm *ipc = it;

	/* If we had an ipc id locked before, unlock it */
	if (ipc && ipc != SEQ_START_TOKEN)
		ipc_unlock(ipc);

	return sysvipc_find_ipc(iter->ns->ids[iface->ids], *pos, pos);
}

/*
 * File positions: pos 0 -> header, pos n -> ipc id = n - 1.
 * SeqFile iterator: iterator value locked ipc pointer or SEQ_TOKEN_START.
 */
static void *sysvipc_proc_start(struct seq_file *s, loff_t *pos)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct ipc_ids *ids;

	ids = iter->ns->ids[iface->ids];

	/*
	 * Take the lock - this will be released by the corresponding
	 * call to stop().
	 */
	down_read(&ids->rw_mutex);

	/* pos < 0 is invalid */
	if (*pos < 0)
		return NULL;

	/* pos == 0 means header */
	if (*pos == 0)
		return SEQ_START_TOKEN;

	/* Find the (pos-1)th ipc */
	return sysvipc_find_ipc(ids, *pos - 1, pos);
}

static void sysvipc_proc_stop(struct seq_file *s, void *it)
{
	struct kern_ipc_perm *ipc = it;
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;
	struct ipc_ids *ids;

	/* If we had a locked structure, release it */
	if (ipc && ipc != SEQ_START_TOKEN)
		ipc_unlock(ipc);

	ids = iter->ns->ids[iface->ids];
	/* Release the lock we took in start() */
	up_read(&ids->rw_mutex);
}

static int sysvipc_proc_show(struct seq_file *s, void *it)
{
	struct ipc_proc_iter *iter = s->private;
	struct ipc_proc_iface *iface = iter->iface;

	if (it == SEQ_START_TOKEN)
		return seq_puts(s, iface->header);

	return iface->show(s, it);
}

static struct seq_operations sysvipc_proc_seqops = {
	.start = sysvipc_proc_start,
	.stop  = sysvipc_proc_stop,
	.next  = sysvipc_proc_next,
	.show  = sysvipc_proc_show,
};

static int sysvipc_proc_open(struct inode *inode, struct file *file)
{
	int ret;
	struct seq_file *seq;
	struct ipc_proc_iter *iter;

	ret = -ENOMEM;
	iter = kmalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		goto out;

	ret = seq_open(file, &sysvipc_proc_seqops);
	if (ret)
		goto out_kfree;

	seq = file->private_data;
	seq->private = iter;

	iter->iface = PDE(inode)->data;
	iter->ns    = get_ipc_ns(current->nsproxy->ipc_ns);
out:
	return ret;
out_kfree:
	kfree(iter);
	goto out;
}

static int sysvipc_proc_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct ipc_proc_iter *iter = seq->private;
	put_ipc_ns(iter->ns);
	return seq_release_private(inode, file);
}

static const struct file_operations sysvipc_proc_fops = {
	.open    = sysvipc_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = sysvipc_proc_release,
};
#endif /* CONFIG_PROC_FS */

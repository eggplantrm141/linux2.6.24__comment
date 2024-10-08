/*
 *  linux/fs/ioctl.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/security.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/ioctls.h>

static long do_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int error = -ENOTTY;

	if (!filp->f_op)/* 文件没有指定f_op，也就没有实现其ioctl */
		goto out;

	if (filp->f_op->unlocked_ioctl) {/* 该文件实现了unlocked_ioctl，这是新的驱动程序 */
		/* 调用设备的ioctl方法，对块设备来说，是blkdev_ioctl */
		error = filp->f_op->unlocked_ioctl(filp, cmd, arg);
		if (error == -ENOIOCTLCMD)
			error = -EINVAL;
		goto out;
	} else if (filp->f_op->ioctl) {/* 旧驱动，没有实现unlock版本 */
		lock_kernel();/* 获取大内核锁以后再调用其ioctl回调 */
		error = filp->f_op->ioctl(filp->f_path.dentry->d_inode,
					  filp, cmd, arg);
		unlock_kernel();
	}

 out:
	return error;
}

static int file_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int error;
	int block;
	struct inode * inode = filp->f_path.dentry->d_inode;
	int __user *p = (int __user *)arg;

	switch (cmd) {
		case FIBMAP:
		{
			struct address_space *mapping = filp->f_mapping;
			int res;
			/* do we support this mess? */
			if (!mapping->a_ops->bmap)
				return -EINVAL;
			if (!capable(CAP_SYS_RAWIO))
				return -EPERM;
			if ((error = get_user(block, p)) != 0)
				return error;

			lock_kernel();
			res = mapping->a_ops->bmap(mapping, block);
			unlock_kernel();
			return put_user(res, p);
		}
		case FIGETBSZ:
			return put_user(inode->i_sb->s_blocksize, p);
		case FIONREAD:
			return put_user(i_size_read(inode) - filp->f_pos, p);
	}

	return do_ioctl(filp, cmd, arg);
}

/*
 * When you add any new common ioctls to the switches above and below
 * please update compat_sys_ioctl() too.
 *
 * vfs_ioctl() is not for drivers and not intended to be EXPORT_SYMBOL()'d.
 * It's just a simple helper for sys_ioctl and compat_sys_ioctl.
 */
int vfs_ioctl(struct file *filp, unsigned int fd, unsigned int cmd, unsigned long arg)
{
	unsigned int flag;
	int on, error = 0;

	switch (cmd) {
		case FIOCLEX:
			set_close_on_exec(fd, 1);
			break;

		case FIONCLEX:
			set_close_on_exec(fd, 0);
			break;

		case FIONBIO:
			if ((error = get_user(on, (int __user *)arg)) != 0)
				break;
			flag = O_NONBLOCK;
#ifdef __sparc__
			/* SunOS compatibility item. */
			if(O_NONBLOCK != O_NDELAY)
				flag |= O_NDELAY;
#endif
			if (on)
				filp->f_flags |= flag;
			else
				filp->f_flags &= ~flag;
			break;

		case FIOASYNC:
			if ((error = get_user(on, (int __user *)arg)) != 0)
				break;
			flag = on ? FASYNC : 0;

			/* Did FASYNC state change ? */
			if ((flag ^ filp->f_flags) & FASYNC) {
				if (filp->f_op && filp->f_op->fasync) {
					lock_kernel();
					error = filp->f_op->fasync(fd, filp, on);
					unlock_kernel();
				}
				else error = -ENOTTY;
			}
			if (error != 0)
				break;

			if (on)
				filp->f_flags |= FASYNC;
			else
				filp->f_flags &= ~FASYNC;
			break;

		case FIOQSIZE:
			if (S_ISDIR(filp->f_path.dentry->d_inode->i_mode) ||
			    S_ISREG(filp->f_path.dentry->d_inode->i_mode) ||
			    S_ISLNK(filp->f_path.dentry->d_inode->i_mode)) {
				loff_t res = inode_get_bytes(filp->f_path.dentry->d_inode);
				error = copy_to_user((loff_t __user *)arg, &res, sizeof(res)) ? -EFAULT : 0;
			}
			else
				error = -ENOTTY;
			break;
		default:/* 非标准的ioctl调用 */
			if (S_ISREG(filp->f_path.dentry->d_inode->i_mode))/* 普通文件 */
				error = file_ioctl(filp, cmd, arg);
			else/* 非普通文件，调用特定的文件ioctl */
				error = do_ioctl(filp, cmd, arg);
			break;
	}
	return error;
}

/**
 * ioctl系统调用
 */
asmlinkage long sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	struct file * filp;
	int error = -EBADF;
	int fput_needed;

	/* 根据文件id得到文件描述描述符 */
	filp = fget_light(fd, &fput_needed);
	if (!filp)/* 文件未打开，退出 */
		goto out;

	/* selinux检查是否有权限进行ioctl操作 */
	error = security_file_ioctl(filp, cmd, arg);
	if (error)
		goto out_fput;

	/* 进行ioctl调用 */
	error = vfs_ioctl(filp, fd, cmd, arg);
 out_fput:
 	/* 递减文件描述符的引用 */
	fput_light(filp, fput_needed);
 out:
	return error;
}

/*
 * linux/fs/9p/trans_fd.c
 *
 * Fd transport layer.  Includes deprecated socket layer.
 *
 *  Copyright (C) 2006 by Russ Cox <rsc@swtch.com>
 *  Copyright (C) 2004-2005 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004-2007 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 1997-2002 by Ron Minnich <rminnich@sarnoff.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#include <linux/in.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/ipv6.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/un.h>
#include <linux/uaccess.h>
#include <linux/inet.h>
#include <linux/idr.h>
#include <linux/file.h>
#include <linux/parser.h>
#include <net/9p/9p.h>
#include <net/9p/transport.h>

#define P9_PORT 564
#define MAX_SOCK_BUF (64*1024)


struct p9_fd_opts {
	int rfd;
	int wfd;
	u16 port;
};

struct p9_trans_fd {
	struct file *rd;
	struct file *wr;
};

/*
  * Option Parsing (code inspired by NFS code)
  *  - a little lazy - parse all fd-transport options
  */

enum {
	/* Options that take integer arguments */
	Opt_port, Opt_rfdno, Opt_wfdno, Opt_err,
};

static match_table_t tokens = {
	{Opt_port, "port=%u"},
	{Opt_rfdno, "rfdno=%u"},
	{Opt_wfdno, "wfdno=%u"},
	{Opt_err, NULL},
};

/**
 * v9fs_parse_options - parse mount options into session structure
 * @options: options string passed from mount
 * @v9ses: existing v9fs session information
 *
 */

static void parse_opts(char *options, struct p9_fd_opts *opts)
{
	char *p;
	substring_t args[MAX_OPT_ARGS];
	int option;
	int ret;

	opts->port = P9_PORT;
	opts->rfd = ~0;
	opts->wfd = ~0;

	if (!options)
		return;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;
		token = match_token(p, tokens, args);
		ret = match_int(&args[0], &option);
		if (ret < 0) {
			P9_DPRINTK(P9_DEBUG_ERROR,
			 "integer field, but no integer?\n");
			continue;
		}
		switch (token) {
		case Opt_port:
			opts->port = option;
			break;
		case Opt_rfdno:
			opts->rfd = option;
			break;
		case Opt_wfdno:
			opts->wfd = option;
			break;
		default:
			continue;
		}
	}
}

static int p9_fd_open(struct p9_trans *trans, int rfd, int wfd)
{
	struct p9_trans_fd *ts = kmalloc(sizeof(struct p9_trans_fd),
					   GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->rd = fget(rfd);
	ts->wr = fget(wfd);
	if (!ts->rd || !ts->wr) {
		if (ts->rd)
			fput(ts->rd);
		if (ts->wr)
			fput(ts->wr);
		kfree(ts);
		return -EIO;
	}

	trans->priv = ts;
	trans->status = Connected;

	return 0;
}

static int p9_socket_open(struct p9_trans *trans, struct socket *csocket)
{
	int fd, ret;

	csocket->sk->sk_allocation = GFP_NOIO;
	fd = sock_map_fd(csocket);
	if (fd < 0) {
		P9_EPRINTK(KERN_ERR, "p9_socket_open: failed to map fd\n");
		return fd;
	}

	ret = p9_fd_open(trans, fd, fd);
	if (ret < 0) {
		P9_EPRINTK(KERN_ERR, "p9_socket_open: failed to open fd\n");
		sockfd_put(csocket);
		return ret;
	}

	((struct p9_trans_fd *)trans->priv)->rd->f_flags |= O_NONBLOCK;

	return 0;
}

/**
 * p9_fd_read- read from a fd
 * @v9ses: session information
 * @v: buffer to receive data into
 * @len: size of receive buffer
 *
 */
static int p9_fd_read(struct p9_trans *trans, void *v, int len)
{
	int ret;
	struct p9_trans_fd *ts = NULL;

	if (trans && trans->status != Disconnected)
		ts = trans->priv;

	if (!ts)
		return -EREMOTEIO;

	if (!(ts->rd->f_flags & O_NONBLOCK))
		P9_DPRINTK(P9_DEBUG_ERROR, "blocking read ...\n");

	ret = kernel_read(ts->rd, ts->rd->f_pos, v, len);
	if (ret <= 0 && ret != -ERESTARTSYS && ret != -EAGAIN)
		trans->status = Disconnected;
	return ret;
}

/**
 * p9_fd_write - write to a socket
 * @v9ses: session information
 * @v: buffer to send data from
 * @len: size of send buffer
 *
 */
static int p9_fd_write(struct p9_trans *trans, void *v, int len)
{
	int ret;
	mm_segment_t oldfs;
	struct p9_trans_fd *ts = NULL;

	if (trans && trans->status != Disconnected)
		ts = trans->priv;

	if (!ts)
		return -EREMOTEIO;

	if (!(ts->wr->f_flags & O_NONBLOCK))
		P9_DPRINTK(P9_DEBUG_ERROR, "blocking write ...\n");

	oldfs = get_fs();
	set_fs(get_ds());
	/* The cast to a user pointer is valid due to the set_fs() */
	ret = vfs_write(ts->wr, (void __user *)v, len, &ts->wr->f_pos);
	set_fs(oldfs);

	if (ret <= 0 && ret != -ERESTARTSYS && ret != -EAGAIN)
		trans->status = Disconnected;
	return ret;
}

static unsigned int
p9_fd_poll(struct p9_trans *trans, struct poll_table_struct *pt)
{
	int ret, n;
	struct p9_trans_fd *ts = NULL;
	mm_segment_t oldfs;

	if (trans && trans->status == Connected)
		ts = trans->priv;

	if (!ts)
		return -EREMOTEIO;

	if (!ts->rd->f_op || !ts->rd->f_op->poll)
		return -EIO;

	if (!ts->wr->f_op || !ts->wr->f_op->poll)
		return -EIO;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = ts->rd->f_op->poll(ts->rd, pt);
	if (ret < 0)
		goto end;

	if (ts->rd != ts->wr) {
		n = ts->wr->f_op->poll(ts->wr, pt);
		if (n < 0) {
			ret = n;
			goto end;
		}
		ret = (ret & ~POLLOUT) | (n & ~POLLIN);
	}

end:
	set_fs(oldfs);
	return ret;
}

/**
 * p9_sock_close - shutdown socket
 * @trans: private socket structure
 *
 */
static void p9_fd_close(struct p9_trans *trans)
{
	struct p9_trans_fd *ts;

	if (!trans)
		return;

	ts = xchg(&trans->priv, NULL);

	if (!ts)
		return;

	trans->status = Disconnected;
	if (ts->rd)
		fput(ts->rd);
	if (ts->wr)
		fput(ts->wr);
	kfree(ts);
}

static struct p9_trans *p9_trans_create_tcp(const char *addr, char *args)
{
	int err;
	struct p9_trans *trans;
	struct socket *csocket;
	struct sockaddr_in sin_server;
	struct p9_fd_opts opts;

	parse_opts(args, &opts);

	csocket = NULL;
	trans = kmalloc(sizeof(struct p9_trans), GFP_KERNEL);
	if (!trans)
		return ERR_PTR(-ENOMEM);

	trans->write = p9_fd_write;
	trans->read = p9_fd_read;
	trans->close = p9_fd_close;
	trans->poll = p9_fd_poll;

	sin_server.sin_family = AF_INET;
	sin_server.sin_addr.s_addr = in_aton(addr);
	sin_server.sin_port = htons(opts.port);
	sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP, &csocket);

	if (!csocket) {
		P9_EPRINTK(KERN_ERR, "p9_trans_tcp: problem creating socket\n");
		err = -EIO;
		goto error;
	}

	err = csocket->ops->connect(csocket,
				    (struct sockaddr *)&sin_server,
				    sizeof(struct sockaddr_in), 0);
	if (err < 0) {
		P9_EPRINTK(KERN_ERR,
			"p9_trans_tcp: problem connecting socket to %s\n",
			addr);
		goto error;
	}

	err = p9_socket_open(trans, csocket);
	if (err < 0)
		goto error;

	return trans;

error:
	if (csocket)
		sock_release(csocket);

	kfree(trans);
	return ERR_PTR(err);
}

static struct p9_trans *p9_trans_create_unix(const char *addr, char *args)
{
	int err;
	struct socket *csocket;
	struct sockaddr_un sun_server;
	struct p9_trans *trans;

	csocket = NULL;
	trans = kmalloc(sizeof(struct p9_trans), GFP_KERNEL);
	if (!trans)
		return ERR_PTR(-ENOMEM);

	trans->write = p9_fd_write;
	trans->read = p9_fd_read;
	trans->close = p9_fd_close;
	trans->poll = p9_fd_poll;

	if (strlen(addr) > UNIX_PATH_MAX) {
		P9_EPRINTK(KERN_ERR, "p9_trans_unix: address too long: %s\n",
			addr);
		err = -ENAMETOOLONG;
		goto error;
	}

	sun_server.sun_family = PF_UNIX;
	strcpy(sun_server.sun_path, addr);
	sock_create_kern(PF_UNIX, SOCK_STREAM, 0, &csocket);
	err = csocket->ops->connect(csocket, (struct sockaddr *)&sun_server,
			sizeof(struct sockaddr_un) - 1, 0);
	if (err < 0) {
		P9_EPRINTK(KERN_ERR,
			"p9_trans_unix: problem connecting socket: %s: %d\n",
			addr, err);
		goto error;
	}

	err = p9_socket_open(trans, csocket);
	if (err < 0)
		goto error;

	return trans;

error:
	if (csocket)
		sock_release(csocket);

	kfree(trans);
	return ERR_PTR(err);
}

static struct p9_trans *p9_trans_create_fd(const char *name, char *args)
{
	int err;
	struct p9_trans *trans;
	struct p9_fd_opts opts;

	parse_opts(args, &opts);

	if (opts.rfd == ~0 || opts.wfd == ~0) {
		printk(KERN_ERR "v9fs: Insufficient options for proto=fd\n");
		return ERR_PTR(-ENOPROTOOPT);
	}

	trans = kmalloc(sizeof(struct p9_trans), GFP_KERNEL);
	if (!trans)
		return ERR_PTR(-ENOMEM);

	trans->write = p9_fd_write;
	trans->read = p9_fd_read;
	trans->close = p9_fd_close;
	trans->poll = p9_fd_poll;

	err = p9_fd_open(trans, opts.rfd, opts.wfd);
	if (err < 0)
		goto error;

	return trans;

error:
	kfree(trans);
	return ERR_PTR(err);
}

static struct p9_trans_module p9_tcp_trans = {
	.name = "tcp",
	.maxsize = MAX_SOCK_BUF,
	.def = 1,
	.create = p9_trans_create_tcp,
};

static struct p9_trans_module p9_unix_trans = {
	.name = "unix",
	.maxsize = MAX_SOCK_BUF,
	.def = 0,
	.create = p9_trans_create_unix,
};

static struct p9_trans_module p9_fd_trans = {
	.name = "fd",
	.maxsize = MAX_SOCK_BUF,
	.def = 0,
	.create = p9_trans_create_fd,
};

static int __init p9_trans_fd_init(void)
{
	v9fs_register_trans(&p9_tcp_trans);
	v9fs_register_trans(&p9_unix_trans);
	v9fs_register_trans(&p9_fd_trans);

	return 1;
}

static void __exit p9_trans_fd_exit(void) {
	printk(KERN_ERR "Removal of 9p transports not implemented\n");
	BUG();
}

module_init(p9_trans_fd_init);
module_exit(p9_trans_fd_exit);

MODULE_AUTHOR("Latchesar Ionkov <lucho@ionkov.net>");
MODULE_AUTHOR("Eric Van Hensbergen <ericvh@gmail.com>");
MODULE_LICENSE("GPL");

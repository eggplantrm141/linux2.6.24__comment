#
# Makefile for Linux kernel SUN RPC
#


obj-$(CONFIG_SUNRPC) += sunrpc.o
obj-$(CONFIG_SUNRPC_GSS) += auth_gss/
obj-$(CONFIG_SUNRPC_XPRT_RDMA) += xprtrdma/

sunrpc-y := clnt.o xprt.o socklib.o xprtsock.o sched.o \
	    auth.o auth_null.o auth_unix.o \
	    svc.o svcsock.o svcauth.o svcauth_unix.o \
	    rpcb_clnt.o timer.o xdr.o \
	    sunrpc_syms.o cache.o rpc_pipe.o
sunrpc-$(CONFIG_PROC_FS) += stats.o
sunrpc-$(CONFIG_SYSCTL) += sysctl.o

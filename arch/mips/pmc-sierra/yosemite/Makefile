#
# Makefile for the PMC-Sierra Titan
#

obj-y    += irq.o prom.o py-console.o setup.o

obj-$(CONFIG_KGDB)		+= dbg_io.o
obj-$(CONFIG_SMP)		+= smp.o

EXTRA_CFLAGS += -Werror

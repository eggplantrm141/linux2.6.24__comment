EXTRA_CFLAGS += -I$(TOPDIR)/drivers/net/cxgb3 \
		-I$(TOPDIR)/drivers/infiniband/hw/cxgb3/core

obj-$(CONFIG_INFINIBAND_CXGB3) += iw_cxgb3.o

iw_cxgb3-y :=  iwch_cm.o iwch_ev.o iwch_cq.o iwch_qp.o iwch_mem.o \
	       iwch_provider.o iwch.o cxio_hal.o cxio_resource.o

ifdef CONFIG_INFINIBAND_CXGB3_DEBUG
EXTRA_CFLAGS += -DDEBUG
endif

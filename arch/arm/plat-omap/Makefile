#
# Makefile for the linux kernel.
#

# Common support
obj-y := common.o sram.o sram-fn.o clock.o devices.o dma.o mux.o gpio.o \
	 usb.o fb.o
obj-m :=
obj-n :=
obj-  :=

obj-$(CONFIG_OMAP_32K_TIMER)	+= timer32k.o

# OCPI interconnect support for 1710, 1610 and 5912
obj-$(CONFIG_ARCH_OMAP16XX) += ocpi.o


obj-$(CONFIG_CPU_FREQ) += cpu-omap.o
obj-$(CONFIG_OMAP_DM_TIMER) += dmtimer.o
obj-$(CONFIG_OMAP_DEBUG_DEVICES) += debug-devices.o
obj-$(CONFIG_OMAP_DEBUG_LEDS) += debug-leds.o

#
# Makefile for the new OMAP framebuffer device driver
#

obj-$(CONFIG_FB_OMAP) += omapfb.o

objs-yy := omapfb_main.o

objs-y$(CONFIG_ARCH_OMAP1) += lcdc.o
objs-y$(CONFIG_ARCH_OMAP2) += dispc.o

objs-$(CONFIG_ARCH_OMAP1)$(CONFIG_FB_OMAP_LCDC_EXTERNAL) += sossi.o
objs-$(CONFIG_ARCH_OMAP2)$(CONFIG_FB_OMAP_LCDC_EXTERNAL) += rfbi.o

objs-y$(CONFIG_FB_OMAP_LCDC_HWA742) += hwa742.o
objs-y$(CONFIG_FB_OMAP_LCDC_BLIZZARD) += blizzard.o

objs-y$(CONFIG_MACH_OMAP_H4) += lcd_h4.o
objs-y$(CONFIG_MACH_OMAP_H3) += lcd_h3.o
objs-y$(CONFIG_MACH_OMAP_PALMTE) += lcd_palmte.o
objs-y$(CONFIG_MACH_OMAP_PALMTT) += lcd_palmtt.o
objs-y$(CONFIG_MACH_OMAP_PALMZ71) += lcd_palmz71.o
objs-$(CONFIG_ARCH_OMAP16XX)$(CONFIG_MACH_OMAP_INNOVATOR) += lcd_inn1610.o
objs-$(CONFIG_ARCH_OMAP15XX)$(CONFIG_MACH_OMAP_INNOVATOR) += lcd_inn1510.o
objs-y$(CONFIG_MACH_OMAP_OSK) += lcd_osk.o
objs-y$(CONFIG_MACH_SX1) += lcd_sx1.o

omapfb-objs := $(objs-yy)


#
# Makefile for the ipmi drivers.
#

ipmi_si-objs := ipmi_si_intf.o ipmi_kcs_sm.o ipmi_smic_sm.o ipmi_bt_sm.o

obj-$(CONFIG_IPMI_HANDLER) += ipmi_msghandler.o
obj-$(CONFIG_IPMI_DEVICE_INTERFACE) += ipmi_devintf.o
obj-$(CONFIG_IPMI_SI) += ipmi_si.o
obj-$(CONFIG_IPMI_WATCHDOG) += ipmi_watchdog.o
obj-$(CONFIG_IPMI_POWEROFF) += ipmi_poweroff.o

ipmi_si.o:	$(ipmi_si-objs)
	$(LD) -r -o $@ $(ipmi_si-objs)


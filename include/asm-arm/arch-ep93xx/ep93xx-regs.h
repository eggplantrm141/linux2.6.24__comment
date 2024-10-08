/*
 * linux/include/asm-arm/arch-ep93xx/ep93xx-regs.h
 */

#ifndef __ASM_ARCH_EP93XX_REGS_H
#define __ASM_ARCH_EP93XX_REGS_H

/*
 * EP93xx linux memory map:
 *
 * virt		phys		size
 * fe800000			5M		per-platform mappings
 * fed00000	80800000	2M		APB
 * fef00000	80000000	1M		AHB
 */

#define EP93XX_AHB_PHYS_BASE		0x80000000
#define EP93XX_AHB_VIRT_BASE		0xfef00000
#define EP93XX_AHB_SIZE			0x00100000

#define EP93XX_APB_PHYS_BASE		0x80800000
#define EP93XX_APB_VIRT_BASE		0xfed00000
#define EP93XX_APB_SIZE			0x00200000


/* AHB peripherals */
#define EP93XX_DMA_BASE			(EP93XX_AHB_VIRT_BASE + 0x00000000)

#define EP93XX_ETHERNET_BASE		(EP93XX_AHB_VIRT_BASE + 0x00010000)
#define EP93XX_ETHERNET_PHYS_BASE	(EP93XX_AHB_PHYS_BASE + 0x00010000)

#define EP93XX_USB_BASE			(EP93XX_AHB_VIRT_BASE + 0x00020000)
#define EP93XX_USB_PHYS_BASE		(EP93XX_AHB_PHYS_BASE + 0x00020000)

#define EP93XX_RASTER_BASE		(EP93XX_AHB_VIRT_BASE + 0x00030000)

#define EP93XX_GRAPHICS_ACCEL_BASE	(EP93XX_AHB_VIRT_BASE + 0x00040000)

#define EP93XX_SDRAM_CONTROLLER_BASE	(EP93XX_AHB_VIRT_BASE + 0x00060000)

#define EP93XX_PCMCIA_CONTROLLER_BASE	(EP93XX_AHB_VIRT_BASE + 0x00080000)

#define EP93XX_BOOT_ROM_BASE		(EP93XX_AHB_VIRT_BASE + 0x00090000)

#define EP93XX_IDE_BASE			(EP93XX_AHB_VIRT_BASE + 0x000a0000)

#define EP93XX_VIC1_BASE		(EP93XX_AHB_VIRT_BASE + 0x000b0000)

#define EP93XX_VIC2_BASE		(EP93XX_AHB_VIRT_BASE + 0x000c0000)


/* APB peripherals */
#define EP93XX_TIMER_BASE		(EP93XX_APB_VIRT_BASE + 0x00010000)
#define EP93XX_TIMER_REG(x)		(EP93XX_TIMER_BASE + (x))
#define EP93XX_TIMER1_LOAD		EP93XX_TIMER_REG(0x00)
#define EP93XX_TIMER1_VALUE		EP93XX_TIMER_REG(0x04)
#define EP93XX_TIMER1_CONTROL		EP93XX_TIMER_REG(0x08)
#define EP93XX_TIMER1_CLEAR		EP93XX_TIMER_REG(0x0c)
#define EP93XX_TIMER2_LOAD		EP93XX_TIMER_REG(0x20)
#define EP93XX_TIMER2_VALUE		EP93XX_TIMER_REG(0x24)
#define EP93XX_TIMER2_CONTROL		EP93XX_TIMER_REG(0x28)
#define EP93XX_TIMER2_CLEAR		EP93XX_TIMER_REG(0x2c)
#define EP93XX_TIMER4_VALUE_LOW		EP93XX_TIMER_REG(0x60)
#define EP93XX_TIMER4_VALUE_HIGH	EP93XX_TIMER_REG(0x64)
#define EP93XX_TIMER3_LOAD		EP93XX_TIMER_REG(0x80)
#define EP93XX_TIMER3_VALUE		EP93XX_TIMER_REG(0x84)
#define EP93XX_TIMER3_CONTROL		EP93XX_TIMER_REG(0x88)
#define EP93XX_TIMER3_CLEAR		EP93XX_TIMER_REG(0x8c)

#define EP93XX_I2S_BASE			(EP93XX_APB_VIRT_BASE + 0x00020000)

#define EP93XX_SECURITY_BASE		(EP93XX_APB_VIRT_BASE + 0x00030000)

#define EP93XX_GPIO_BASE		(EP93XX_APB_VIRT_BASE + 0x00040000)
#define EP93XX_GPIO_REG(x)		(EP93XX_GPIO_BASE + (x))
#define EP93XX_GPIO_F_INT_TYPE1		EP93XX_GPIO_REG(0x4c)
#define EP93XX_GPIO_F_INT_TYPE2		EP93XX_GPIO_REG(0x50)
#define EP93XX_GPIO_F_INT_ACK		EP93XX_GPIO_REG(0x54)
#define EP93XX_GPIO_F_INT_ENABLE	EP93XX_GPIO_REG(0x58)
#define EP93XX_GPIO_F_INT_STATUS	EP93XX_GPIO_REG(0x5c)
#define EP93XX_GPIO_A_INT_TYPE1		EP93XX_GPIO_REG(0x90)
#define EP93XX_GPIO_A_INT_TYPE2		EP93XX_GPIO_REG(0x94)
#define EP93XX_GPIO_A_INT_ACK		EP93XX_GPIO_REG(0x98)
#define EP93XX_GPIO_A_INT_ENABLE	EP93XX_GPIO_REG(0x9c)
#define EP93XX_GPIO_A_INT_STATUS	EP93XX_GPIO_REG(0xa0)
#define EP93XX_GPIO_B_INT_TYPE1		EP93XX_GPIO_REG(0xac)
#define EP93XX_GPIO_B_INT_TYPE2		EP93XX_GPIO_REG(0xb0)
#define EP93XX_GPIO_B_INT_ACK		EP93XX_GPIO_REG(0xb4)
#define EP93XX_GPIO_B_INT_ENABLE	EP93XX_GPIO_REG(0xb8)
#define EP93XX_GPIO_B_INT_STATUS	EP93XX_GPIO_REG(0xbc)

#define EP93XX_AAC_BASE			(EP93XX_APB_VIRT_BASE + 0x00080000)

#define EP93XX_SPI_BASE			(EP93XX_APB_VIRT_BASE + 0x000a0000)

#define EP93XX_IRDA_BASE		(EP93XX_APB_VIRT_BASE + 0x000b0000)

#define EP93XX_UART1_BASE		(EP93XX_APB_VIRT_BASE + 0x000c0000)
#define EP93XX_UART1_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000c0000)

#define EP93XX_UART2_BASE		(EP93XX_APB_VIRT_BASE + 0x000d0000)
#define EP93XX_UART2_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000d0000)

#define EP93XX_UART3_BASE		(EP93XX_APB_VIRT_BASE + 0x000e0000)
#define EP93XX_UART3_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000e0000)

#define EP93XX_KEY_MATRIX_BASE		(EP93XX_APB_VIRT_BASE + 0x000f0000)

#define EP93XX_ADC_BASE			(EP93XX_APB_VIRT_BASE + 0x00100000)
#define EP93XX_TOUCHSCREEN_BASE		(EP93XX_APB_VIRT_BASE + 0x00100000)

#define EP93XX_PWM_BASE			(EP93XX_APB_VIRT_BASE + 0x00110000)

#define EP93XX_RTC_BASE			(EP93XX_APB_VIRT_BASE + 0x00120000)

#define EP93XX_SYSCON_BASE		(EP93XX_APB_VIRT_BASE + 0x00130000)
#define EP93XX_SYSCON_REG(x)		(EP93XX_SYSCON_BASE + (x))
#define EP93XX_SYSCON_POWER_STATE	EP93XX_SYSCON_REG(0x00)
#define EP93XX_SYSCON_CLOCK_CONTROL	EP93XX_SYSCON_REG(0x04)
#define EP93XX_SYSCON_CLOCK_UARTBAUD	0x20000000
#define EP93XX_SYSCON_CLOCK_USH_EN	0x10000000
#define EP93XX_SYSCON_HALT		EP93XX_SYSCON_REG(0x08)
#define EP93XX_SYSCON_STANDBY		EP93XX_SYSCON_REG(0x0c)
#define EP93XX_SYSCON_CLOCK_SET1	EP93XX_SYSCON_REG(0x20)
#define EP93XX_SYSCON_CLOCK_SET2	EP93XX_SYSCON_REG(0x24)
#define EP93XX_SYSCON_DEVICE_CONFIG	EP93XX_SYSCON_REG(0x80)
#define EP93XX_SYSCON_DEVICE_CONFIG_CRUNCH_ENABLE	0x00800000
#define EP93XX_SYSCON_SWLOCK		EP93XX_SYSCON_REG(0xc0)

#define EP93XX_WATCHDOG_BASE		(EP93XX_APB_VIRT_BASE + 0x00140000)


#endif

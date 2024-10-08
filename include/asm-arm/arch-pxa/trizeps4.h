/************************************************************************
 * Include file for TRIZEPS4 SoM and ConXS eval-board
 * Copyright (c) Jürgen Schindele
 * 2006
 ************************************************************************/

/*
 * Includes/Defines
 */
#ifndef _TRIPEPS4_H_
#define _TRIPEPS4_H_

/* physical memory regions */
#define TRIZEPS4_FLASH_PHYS	(PXA_CS0_PHYS)  /* Flash region */
#define TRIZEPS4_DISK_PHYS	(PXA_CS1_PHYS)  /* Disk On Chip region */
#define TRIZEPS4_ETH_PHYS	(PXA_CS2_PHYS)  /* Ethernet DM9000 region */
#define TRIZEPS4_PIC_PHYS	(PXA_CS3_PHYS)	/* Logic chip on ConXS-Board */
#define TRIZEPS4_SDRAM_BASE	0xa0000000      /* SDRAM region */

#define TRIZEPS4_CFSR_PHYS	(PXA_CS3_PHYS)			/* Logic chip on ConXS-Board CSFR register */
#define TRIZEPS4_BOCR_PHYS	(PXA_CS3_PHYS+0x02000000)	/* Logic chip on ConXS-Board BOCR register */
#define TRIZEPS4_IRCR_PHYS	(PXA_CS3_PHYS+0x02400000)	/* Logic chip on ConXS-Board IRCR register*/
#define TRIZEPS4_UPSR_PHYS	(PXA_CS3_PHYS+0x02800000)	/* Logic chip on ConXS-Board UPSR register*/
#define TRIZEPS4_DICR_PHYS	(PXA_CS3_PHYS+0x03800000)	/* Logic chip on ConXS-Board DICR register*/

/* virtual memory regions */
#define TRIZEPS4_DISK_VIRT	0xF0000000	/* Disk On Chip region */

#define TRIZEPS4_PIC_VIRT	0xF0100000	/* not used */
#define TRIZEPS4_CFSR_VIRT	0xF0100000
#define TRIZEPS4_BOCR_VIRT	0xF0200000
#define TRIZEPS4_DICR_VIRT	0xF0300000
#define TRIZEPS4_IRCR_VIRT	0xF0400000
#define TRIZEPS4_UPSR_VIRT	0xF0500000

/* size of flash */
#define TRIZEPS4_FLASH_SIZE	0x02000000	/* Flash size 32 MB */

/* Ethernet Controller Davicom DM9000 */
#define GPIO_DM9000		101
#define TRIZEPS4_ETH_IRQ	IRQ_GPIO(GPIO_DM9000)

/* UCB1400 audio / TS-controller */
#define GPIO_UCB1400		1
#define TRIZEPS4_UCB1400_IRQ	IRQ_GPIO(GPIO_UCB1400)

/* PCMCIA socket Compact Flash */
#define GPIO_PCD		11		/* PCMCIA Card Detect */
#define TRIZEPS4_CD_IRQ		IRQ_GPIO(GPIO_PCD)
#define GPIO_PRDY		13		/* READY / nINT */
#define TRIZEPS4_READY_NINT	IRQ_GPIO(GPIO_PRDY)

/* MMC socket */
#define GPIO_MMC_DET		12
#define TRIZEPS4_MMC_IRQ	IRQ_GPIO(GPIO_MMC_DET)

/* LEDS using tx2 / rx2 */
#define GPIO_SYS_BUSY_LED	46
#define GPIO_HEARTBEAT_LED	47

/* Off-module PIC on ConXS board */
#define GPIO_PIC		0
#define TRIZEPS4_PIC_IRQ	IRQ_GPIO(GPIO_PIC)

#define CFSR_P2V(x)		((x) - TRIZEPS4_CFSR_PHYS + TRIZEPS4_CFSR_VIRT)
#define CFSR_V2P(x)		((x) - TRIZEPS4_CFSR_VIRT + TRIZEPS4_CFSR_PHYS)

#define BCR_P2V(x)		((x) - TRIZEPS4_BOCR_PHYS + TRIZEPS4_BOCR_VIRT)
#define BCR_V2P(x)		((x) - TRIZEPS4_BOCR_VIRT + TRIZEPS4_BOCR_PHYS)

#define DCR_P2V(x)		((x) - TRIZEPS4_DICR_PHYS + TRIZEPS4_DICR_VIRT)
#define DCR_V2P(x)		((x) - TRIZEPS4_DICR_VIRT + TRIZEPS4_DICR_PHYS)

#ifndef __ASSEMBLY__
#define ConXS_CFSR		(*((volatile unsigned short *)CFSR_P2V(0x0C000000)))
#define ConXS_BCR		(*((volatile unsigned short *)BCR_P2V(0x0E000000)))
#define ConXS_DCR		(*((volatile unsigned short *)DCR_P2V(0x0F800000)))
#else
#define ConXS_CFSR		CFSR_P2V(0x0C000000)
#define ConXS_BCR		BCR_P2V(0x0E000000)
#define ConXS_DCR		DCR_P2V(0x0F800000)
#endif

#define ConXS_CFSR_BVD_MASK	0x0003
#define ConXS_CFSR_BVD1		(1 << 0)
#define ConXS_CFSR_BVD2		(1 << 1)
#define ConXS_CFSR_VS_MASK	0x000C
#define ConXS_CFSR_VS1		(1 << 2)
#define ConXS_CFSR_VS2		(1 << 3)
#define ConXS_CFSR_VS_5V	(0x3 << 2)
#define ConXS_CFSR_VS_3V3	0x0

#define ConXS_BCR_S0_POW_EN0	(1 << 0)
#define ConXS_BCR_S0_POW_EN1	(1 << 1)
#define ConXS_BCR_L_DISP	(1 << 4)
#define ConXS_BCR_CF_BUF_EN	(1 << 5)
#define ConXS_BCR_CF_RESET	(1 << 7)
#define ConXS_BCR_S0_VCC_3V3	0x1
#define ConXS_BCR_S0_VCC_5V0	0x2
#define ConXS_BCR_S0_VPP_12V	0x4
#define ConXS_BCR_S0_VPP_3V3	0x8

#define ConXS_IRCR_MODE		(1 << 0)
#define ConXS_IRCR_SD		(1 << 1)

#endif /* _TRIPEPS4_H_ */

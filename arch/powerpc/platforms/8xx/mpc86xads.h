/*
 * A collection of structures, addresses, and values associated with
 * the Freescale MPC86xADS board.
 * Copied from the FADS stuff.
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2005 (c) MontaVista Software, Inc.  This file is licensed under the
 * terms of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

#ifdef __KERNEL__
#ifndef __ASM_MPC86XADS_H__
#define __ASM_MPC86XADS_H__

#include <sysdev/fsl_soc.h>

/* U-Boot maps BCSR to 0xff080000 */
#define BCSR_ADDR		((uint)0xff080000)
#define BCSR_SIZE		((uint)32)
#define BCSR0			((uint)(BCSR_ADDR + 0x00))
#define BCSR1			((uint)(BCSR_ADDR + 0x04))
#define BCSR2			((uint)(BCSR_ADDR + 0x08))
#define BCSR3			((uint)(BCSR_ADDR + 0x0c))
#define BCSR4			((uint)(BCSR_ADDR + 0x10))

#define CFG_PHYDEV_ADDR		((uint)0xff0a0000)
#define BCSR5			((uint)(CFG_PHYDEV_ADDR + 0x300))

#define MPC8xx_CPM_OFFSET	(0x9c0)
#define CPM_MAP_ADDR		(get_immrbase() + MPC8xx_CPM_OFFSET)
#define CPM_IRQ_OFFSET		16     // for compability with cpm_uart driver

#define PCMCIA_MEM_ADDR		((uint)0xff020000)
#define PCMCIA_MEM_SIZE		((uint)(64 * 1024))

/* Bits of interest in the BCSRs.
 */
#define BCSR1_ETHEN		((uint)0x20000000)
#define BCSR1_IRDAEN		((uint)0x10000000)
#define BCSR1_RS232EN_1		((uint)0x01000000)
#define BCSR1_PCCEN		((uint)0x00800000)
#define BCSR1_PCCVCC0		((uint)0x00400000)
#define BCSR1_PCCVPP0		((uint)0x00200000)
#define BCSR1_PCCVPP1		((uint)0x00100000)
#define BCSR1_PCCVPP_MASK	(BCSR1_PCCVPP0 | BCSR1_PCCVPP1)
#define BCSR1_RS232EN_2		((uint)0x00040000)
#define BCSR1_PCCVCC1		((uint)0x00010000)
#define BCSR1_PCCVCC_MASK	(BCSR1_PCCVCC0 | BCSR1_PCCVCC1)

#define BCSR4_ETH10_RST		((uint)0x80000000)	/* 10Base-T PHY reset*/
#define BCSR4_USB_LO_SPD	((uint)0x04000000)
#define BCSR4_USB_VCC		((uint)0x02000000)
#define BCSR4_USB_FULL_SPD	((uint)0x00040000)
#define BCSR4_USB_EN		((uint)0x00020000)

#define BCSR5_MII2_EN		0x40
#define BCSR5_MII2_RST		0x20
#define BCSR5_T1_RST		0x10
#define BCSR5_ATM155_RST	0x08
#define BCSR5_ATM25_RST		0x04
#define BCSR5_MII1_EN		0x02
#define BCSR5_MII1_RST		0x01

/* Interrupt level assignments */
#define PHY_INTERRUPT	SIU_IRQ7	/* PHY link change interrupt */
#define SIU_INT_FEC1	SIU_LEVEL1	/* FEC1 interrupt */
#define FEC_INTERRUPT	SIU_INT_FEC1	/* FEC interrupt */

/* We don't use the 8259 */
#define NR_8259_INTS	0

/* CPM Ethernet through SCC1 */
#define PA_ENET_RXD     ((ushort)0x0001)
#define PA_ENET_TXD     ((ushort)0x0002)
#define PA_ENET_TCLK    ((ushort)0x0100)
#define PA_ENET_RCLK    ((ushort)0x0200)
#define PB_ENET_TENA    ((uint)0x00001000)
#define PC_ENET_CLSN    ((ushort)0x0010)
#define PC_ENET_RENA    ((ushort)0x0020)

/* Control bits in the SICR to route TCLK (CLK1) and RCLK (CLK2) to
 * SCC1.  Also, make sure GR1 (bit 24) and SC1 (bit 25) are zero.
 */
#define SICR_ENET_MASK  ((uint)0x000000ff)
#define SICR_ENET_CLKRT ((uint)0x0000002c)

#endif /* __ASM_MPC86XADS_H__ */
#endif /* __KERNEL__ */

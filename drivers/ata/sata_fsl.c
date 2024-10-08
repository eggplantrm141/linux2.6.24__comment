/*
 * drivers/ata/sata_fsl.c
 *
 * Freescale 3.0Gbps SATA device driver
 *
 * Author: Ashish Kalra <ashish.kalra@freescale.com>
 * Li Yang <leoli@freescale.com>
 *
 * Copyright (c) 2006-2007 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/libata.h>
#include <asm/io.h>
#include <linux/of_platform.h>

/* Controller information */
enum {
	SATA_FSL_QUEUE_DEPTH	= 16,
	SATA_FSL_MAX_PRD	= 63,
	SATA_FSL_MAX_PRD_USABLE	= SATA_FSL_MAX_PRD - 1,
	SATA_FSL_MAX_PRD_DIRECT	= 16,	/* Direct PRDT entries */

	SATA_FSL_HOST_FLAGS	= (ATA_FLAG_SATA | ATA_FLAG_NO_LEGACY |
				ATA_FLAG_MMIO | ATA_FLAG_PIO_DMA |
				ATA_FLAG_NCQ),
	SATA_FSL_HOST_LFLAGS	= ATA_LFLAG_SKIP_D2H_BSY,

	SATA_FSL_MAX_CMDS	= SATA_FSL_QUEUE_DEPTH,
	SATA_FSL_CMD_HDR_SIZE	= 16,	/* 4 DWORDS */
	SATA_FSL_CMD_SLOT_SIZE  = (SATA_FSL_MAX_CMDS * SATA_FSL_CMD_HDR_SIZE),

	/*
	 * SATA-FSL host controller supports a max. of (15+1) direct PRDEs, and
	 * chained indirect PRDEs upto a max count of 63.
	 * We are allocating an array of 63 PRDEs contigiously, but PRDE#15 will
	 * be setup as an indirect descriptor, pointing to it's next
	 * (contigious) PRDE. Though chained indirect PRDE arrays are
	 * supported,it will be more efficient to use a direct PRDT and
	 * a single chain/link to indirect PRDE array/PRDT.
	 */

	SATA_FSL_CMD_DESC_CFIS_SZ	= 32,
	SATA_FSL_CMD_DESC_SFIS_SZ	= 32,
	SATA_FSL_CMD_DESC_ACMD_SZ	= 16,
	SATA_FSL_CMD_DESC_RSRVD		= 16,

	SATA_FSL_CMD_DESC_SIZE	= (SATA_FSL_CMD_DESC_CFIS_SZ +
				 SATA_FSL_CMD_DESC_SFIS_SZ +
				 SATA_FSL_CMD_DESC_ACMD_SZ +
				 SATA_FSL_CMD_DESC_RSRVD +
				 SATA_FSL_MAX_PRD * 16),

	SATA_FSL_CMD_DESC_OFFSET_TO_PRDT	=
				(SATA_FSL_CMD_DESC_CFIS_SZ +
				 SATA_FSL_CMD_DESC_SFIS_SZ +
				 SATA_FSL_CMD_DESC_ACMD_SZ +
				 SATA_FSL_CMD_DESC_RSRVD),

	SATA_FSL_CMD_DESC_AR_SZ	= (SATA_FSL_CMD_DESC_SIZE * SATA_FSL_MAX_CMDS),
	SATA_FSL_PORT_PRIV_DMA_SZ = (SATA_FSL_CMD_SLOT_SIZE +
					SATA_FSL_CMD_DESC_AR_SZ),

	/*
	 * MPC8315 has two SATA controllers, SATA1 & SATA2
	 * (one port per controller)
	 * MPC837x has 2/4 controllers, one port per controller
	 */

	SATA_FSL_MAX_PORTS	= 1,

	SATA_FSL_IRQ_FLAG	= IRQF_SHARED,
};

/*
* Host Controller command register set - per port
*/
enum {
	CQ = 0,
	CA = 8,
	CC = 0x10,
	CE = 0x18,
	DE = 0x20,
	CHBA = 0x24,
	HSTATUS = 0x28,
	HCONTROL = 0x2C,
	CQPMP = 0x30,
	SIGNATURE = 0x34,
	ICC = 0x38,

	/*
	 * Host Status Register (HStatus) bitdefs
	 */
	ONLINE = (1 << 31),
	GOING_OFFLINE = (1 << 30),
	BIST_ERR = (1 << 29),

	FATAL_ERR_HC_MASTER_ERR = (1 << 18),
	FATAL_ERR_PARITY_ERR_TX = (1 << 17),
	FATAL_ERR_PARITY_ERR_RX = (1 << 16),
	FATAL_ERR_DATA_UNDERRUN = (1 << 13),
	FATAL_ERR_DATA_OVERRUN = (1 << 12),
	FATAL_ERR_CRC_ERR_TX = (1 << 11),
	FATAL_ERR_CRC_ERR_RX = (1 << 10),
	FATAL_ERR_FIFO_OVRFL_TX = (1 << 9),
	FATAL_ERR_FIFO_OVRFL_RX = (1 << 8),

	FATAL_ERROR_DECODE = FATAL_ERR_HC_MASTER_ERR |
	    FATAL_ERR_PARITY_ERR_TX |
	    FATAL_ERR_PARITY_ERR_RX |
	    FATAL_ERR_DATA_UNDERRUN |
	    FATAL_ERR_DATA_OVERRUN |
	    FATAL_ERR_CRC_ERR_TX |
	    FATAL_ERR_CRC_ERR_RX |
	    FATAL_ERR_FIFO_OVRFL_TX | FATAL_ERR_FIFO_OVRFL_RX,

	INT_ON_FATAL_ERR = (1 << 5),
	INT_ON_PHYRDY_CHG = (1 << 4),

	INT_ON_SIGNATURE_UPDATE = (1 << 3),
	INT_ON_SNOTIFY_UPDATE = (1 << 2),
	INT_ON_SINGL_DEVICE_ERR = (1 << 1),
	INT_ON_CMD_COMPLETE = 1,

	INT_ON_ERROR = INT_ON_FATAL_ERR |
	    INT_ON_PHYRDY_CHG | INT_ON_SINGL_DEVICE_ERR,

	/*
	 * Host Control Register (HControl) bitdefs
	 */
	HCONTROL_ONLINE_PHY_RST = (1 << 31),
	HCONTROL_FORCE_OFFLINE = (1 << 30),
	HCONTROL_PARITY_PROT_MOD = (1 << 14),
	HCONTROL_DPATH_PARITY = (1 << 12),
	HCONTROL_SNOOP_ENABLE = (1 << 10),
	HCONTROL_PMP_ATTACHED = (1 << 9),
	HCONTROL_COPYOUT_STATFIS = (1 << 8),
	IE_ON_FATAL_ERR = (1 << 5),
	IE_ON_PHYRDY_CHG = (1 << 4),
	IE_ON_SIGNATURE_UPDATE = (1 << 3),
	IE_ON_SNOTIFY_UPDATE = (1 << 2),
	IE_ON_SINGL_DEVICE_ERR = (1 << 1),
	IE_ON_CMD_COMPLETE = 1,

	DEFAULT_PORT_IRQ_ENABLE_MASK = IE_ON_FATAL_ERR | IE_ON_PHYRDY_CHG |
	    IE_ON_SIGNATURE_UPDATE |
	    IE_ON_SINGL_DEVICE_ERR | IE_ON_CMD_COMPLETE,

	EXT_INDIRECT_SEG_PRD_FLAG = (1 << 31),
	DATA_SNOOP_ENABLE = (1 << 22),
};

/*
 * SATA Superset Registers
 */
enum {
	SSTATUS = 0,
	SERROR = 4,
	SCONTROL = 8,
	SNOTIFY = 0xC,
};

/*
 * Control Status Register Set
 */
enum {
	TRANSCFG = 0,
	TRANSSTATUS = 4,
	LINKCFG = 8,
	LINKCFG1 = 0xC,
	LINKCFG2 = 0x10,
	LINKSTATUS = 0x14,
	LINKSTATUS1 = 0x18,
	PHYCTRLCFG = 0x1C,
	COMMANDSTAT = 0x20,
};

/* PHY (link-layer) configuration control */
enum {
	PHY_BIST_ENABLE = 0x01,
};

/*
 * Command Header Table entry, i.e, command slot
 * 4 Dwords per command slot, command header size ==  64 Dwords.
 */
struct cmdhdr_tbl_entry {
	u32 cda;
	u32 prde_fis_len;
	u32 ttl;
	u32 desc_info;
};

/*
 * Description information bitdefs
 */
enum {
	VENDOR_SPECIFIC_BIST = (1 << 10),
	CMD_DESC_SNOOP_ENABLE = (1 << 9),
	FPDMA_QUEUED_CMD = (1 << 8),
	SRST_CMD = (1 << 7),
	BIST = (1 << 6),
	ATAPI_CMD = (1 << 5),
};

/*
 * Command Descriptor
 */
struct command_desc {
	u8 cfis[8 * 4];
	u8 sfis[8 * 4];
	u8 acmd[4 * 4];
	u8 fill[4 * 4];
	u32 prdt[SATA_FSL_MAX_PRD_DIRECT * 4];
	u32 prdt_indirect[(SATA_FSL_MAX_PRD - SATA_FSL_MAX_PRD_DIRECT) * 4];
};

/*
 * Physical region table descriptor(PRD)
 */

struct prde {
	u32 dba;
	u8 fill[2 * 4];
	u32 ddc_and_ext;
};

/*
 * ata_port private data
 * This is our per-port instance data.
 */
struct sata_fsl_port_priv {
	struct cmdhdr_tbl_entry *cmdslot;
	dma_addr_t cmdslot_paddr;
	struct command_desc *cmdentry;
	dma_addr_t cmdentry_paddr;

	/*
	 * SATA FSL controller has a Status FIS which should contain the
	 * received D2H FIS & taskfile registers. This SFIS is present in
	 * the command descriptor, and to have a ready reference to it,
	 * we are caching it here, quite similar to what is done in H/W on
	 * AHCI compliant devices by copying taskfile fields to a 32-bit
	 * register.
	 */

	struct ata_taskfile tf;
};

/*
 * ata_port->host_set private data
 */
struct sata_fsl_host_priv {
	void __iomem *hcr_base;
	void __iomem *ssr_base;
	void __iomem *csr_base;
	int irq;
};

static inline unsigned int sata_fsl_tag(unsigned int tag,
					void __iomem *hcr_base)
{
	/* We let libATA core do actual (queue) tag allocation */

	/* all non NCQ/queued commands should have tag#0 */
	if (ata_tag_internal(tag)) {
		DPRINTK("mapping internal cmds to tag#0\n");
		return 0;
	}

	if (unlikely(tag >= SATA_FSL_QUEUE_DEPTH)) {
		DPRINTK("tag %d invalid : out of range\n", tag);
		return 0;
	}

	if (unlikely((ioread32(hcr_base + CQ)) & (1 << tag))) {
		DPRINTK("tag %d invalid : in use!!\n", tag);
		return 0;
	}

	return tag;
}

static void sata_fsl_setup_cmd_hdr_entry(struct sata_fsl_port_priv *pp,
					 unsigned int tag, u32 desc_info,
					 u32 data_xfer_len, u8 num_prde,
					 u8 fis_len)
{
	dma_addr_t cmd_descriptor_address;

	cmd_descriptor_address = pp->cmdentry_paddr +
	    tag * SATA_FSL_CMD_DESC_SIZE;

	/* NOTE: both data_xfer_len & fis_len are Dword counts */

	pp->cmdslot[tag].cda = cpu_to_le32(cmd_descriptor_address);
	pp->cmdslot[tag].prde_fis_len =
	    cpu_to_le32((num_prde << 16) | (fis_len << 2));
	pp->cmdslot[tag].ttl = cpu_to_le32(data_xfer_len & ~0x03);
	pp->cmdslot[tag].desc_info = cpu_to_le32(desc_info | (tag & 0x1F));

	VPRINTK("cda=0x%x, prde_fis_len=0x%x, ttl=0x%x, di=0x%x\n",
		pp->cmdslot[tag].cda,
		pp->cmdslot[tag].prde_fis_len,
		pp->cmdslot[tag].ttl, pp->cmdslot[tag].desc_info);

}

static unsigned int sata_fsl_fill_sg(struct ata_queued_cmd *qc, void *cmd_desc,
				     u32 *ttl, dma_addr_t cmd_desc_paddr)
{
	struct scatterlist *sg;
	unsigned int num_prde = 0;
	u32 ttl_dwords = 0;

	/*
	 * NOTE : direct & indirect prdt's are contigiously allocated
	 */
	struct prde *prd = (struct prde *)&((struct command_desc *)
					    cmd_desc)->prdt;

	struct prde *prd_ptr_to_indirect_ext = NULL;
	unsigned indirect_ext_segment_sz = 0;
	dma_addr_t indirect_ext_segment_paddr;

	VPRINTK("SATA FSL : cd = 0x%x, prd = 0x%x\n", cmd_desc, prd);

	indirect_ext_segment_paddr = cmd_desc_paddr +
	    SATA_FSL_CMD_DESC_OFFSET_TO_PRDT + SATA_FSL_MAX_PRD_DIRECT * 16;

	ata_for_each_sg(sg, qc) {
		dma_addr_t sg_addr = sg_dma_address(sg);
		u32 sg_len = sg_dma_len(sg);

		VPRINTK("SATA FSL : fill_sg, sg_addr = 0x%x, sg_len = %d\n",
			sg_addr, sg_len);

		/* warn if each s/g element is not dword aligned */
		if (sg_addr & 0x03)
			ata_port_printk(qc->ap, KERN_ERR,
					"s/g addr unaligned : 0x%x\n", sg_addr);
		if (sg_len & 0x03)
			ata_port_printk(qc->ap, KERN_ERR,
					"s/g len unaligned : 0x%x\n", sg_len);

		if ((num_prde == (SATA_FSL_MAX_PRD_DIRECT - 1)) &&
		    (qc->n_iter + 1 != qc->n_elem)) {
			VPRINTK("setting indirect prde\n");
			prd_ptr_to_indirect_ext = prd;
			prd->dba = cpu_to_le32(indirect_ext_segment_paddr);
			indirect_ext_segment_sz = 0;
			++prd;
			++num_prde;
		}

		ttl_dwords += sg_len;
		prd->dba = cpu_to_le32(sg_addr);
		prd->ddc_and_ext =
		    cpu_to_le32(DATA_SNOOP_ENABLE | (sg_len & ~0x03));

		VPRINTK("sg_fill, ttl=%d, dba=0x%x, ddc=0x%x\n",
			ttl_dwords, prd->dba, prd->ddc_and_ext);

		++num_prde;
		++prd;
		if (prd_ptr_to_indirect_ext)
			indirect_ext_segment_sz += sg_len;
	}

	if (prd_ptr_to_indirect_ext) {
		/* set indirect extension flag along with indirect ext. size */
		prd_ptr_to_indirect_ext->ddc_and_ext =
		    cpu_to_le32((EXT_INDIRECT_SEG_PRD_FLAG |
				 DATA_SNOOP_ENABLE |
				 (indirect_ext_segment_sz & ~0x03)));
	}

	*ttl = ttl_dwords;
	return num_prde;
}

static void sata_fsl_qc_prep(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct sata_fsl_port_priv *pp = ap->private_data;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	unsigned int tag = sata_fsl_tag(qc->tag, hcr_base);
	struct command_desc *cd;
	u32 desc_info = CMD_DESC_SNOOP_ENABLE;
	u32 num_prde = 0;
	u32 ttl_dwords = 0;
	dma_addr_t cd_paddr;

	cd = (struct command_desc *)pp->cmdentry + tag;
	cd_paddr = pp->cmdentry_paddr + tag * SATA_FSL_CMD_DESC_SIZE;

	ata_tf_to_fis(&qc->tf, 0, 1, (u8 *) &cd->cfis);

	VPRINTK("Dumping cfis : 0x%x, 0x%x, 0x%x\n",
		cd->cfis[0], cd->cfis[1], cd->cfis[2]);

	if (qc->tf.protocol == ATA_PROT_NCQ) {
		VPRINTK("FPDMA xfer,Sctor cnt[0:7],[8:15] = %d,%d\n",
			cd->cfis[3], cd->cfis[11]);
	}

	/* setup "ACMD - atapi command" in cmd. desc. if this is ATAPI cmd */
	if (is_atapi_taskfile(&qc->tf)) {
		desc_info |= ATAPI_CMD;
		memset((void *)&cd->acmd, 0, 32);
		memcpy((void *)&cd->acmd, qc->cdb, qc->dev->cdb_len);
	}

	if (qc->flags & ATA_QCFLAG_DMAMAP)
		num_prde = sata_fsl_fill_sg(qc, (void *)cd,
					    &ttl_dwords, cd_paddr);

	if (qc->tf.protocol == ATA_PROT_NCQ)
		desc_info |= FPDMA_QUEUED_CMD;

	sata_fsl_setup_cmd_hdr_entry(pp, tag, desc_info, ttl_dwords,
				     num_prde, 5);

	VPRINTK("SATA FSL : xx_qc_prep, di = 0x%x, ttl = %d, num_prde = %d\n",
		desc_info, ttl_dwords, num_prde);
}

static unsigned int sata_fsl_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	unsigned int tag = sata_fsl_tag(qc->tag, hcr_base);

	VPRINTK("xx_qc_issue called,CQ=0x%x,CA=0x%x,CE=0x%x,CC=0x%x\n",
		ioread32(CQ + hcr_base),
		ioread32(CA + hcr_base),
		ioread32(CE + hcr_base), ioread32(CC + hcr_base));

	/* Simply queue command to the controller/device */
	iowrite32(1 << tag, CQ + hcr_base);

	VPRINTK("xx_qc_issue called, tag=%d, CQ=0x%x, CA=0x%x\n",
		tag, ioread32(CQ + hcr_base), ioread32(CA + hcr_base));

	VPRINTK("CE=0x%x, DE=0x%x, CC=0x%x, CmdStat = 0x%x\n",
		ioread32(CE + hcr_base),
		ioread32(DE + hcr_base),
		ioread32(CC + hcr_base), ioread32(COMMANDSTAT + csr_base));

	return 0;
}

static int sata_fsl_scr_write(struct ata_port *ap, unsigned int sc_reg_in,
			       u32 val)
{
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *ssr_base = host_priv->ssr_base;
	unsigned int sc_reg;

	switch (sc_reg_in) {
	case SCR_STATUS:
	case SCR_ERROR:
	case SCR_CONTROL:
	case SCR_ACTIVE:
		sc_reg = sc_reg_in;
		break;
	default:
		return -EINVAL;
	}

	VPRINTK("xx_scr_write, reg_in = %d\n", sc_reg);

	iowrite32(val, ssr_base + (sc_reg * 4));
	return 0;
}

static int sata_fsl_scr_read(struct ata_port *ap, unsigned int sc_reg_in,
			u32 *val)
{
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *ssr_base = host_priv->ssr_base;
	unsigned int sc_reg;

	switch (sc_reg_in) {
	case SCR_STATUS:
	case SCR_ERROR:
	case SCR_CONTROL:
	case SCR_ACTIVE:
		sc_reg = sc_reg_in;
		break;
	default:
		return -EINVAL;
	}

	VPRINTK("xx_scr_read, reg_in = %d\n", sc_reg);

	*val = ioread32(ssr_base + (sc_reg * 4));
	return 0;
}

static void sata_fsl_freeze(struct ata_port *ap)
{
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;

	VPRINTK("xx_freeze, CQ=0x%x, CA=0x%x, CE=0x%x, DE=0x%x\n",
		ioread32(CQ + hcr_base),
		ioread32(CA + hcr_base),
		ioread32(CE + hcr_base), ioread32(DE + hcr_base));
	VPRINTK("CmdStat = 0x%x\n", ioread32(csr_base + COMMANDSTAT));

	/* disable interrupts on the controller/port */
	temp = ioread32(hcr_base + HCONTROL);
	iowrite32((temp & ~0x3F), hcr_base + HCONTROL);

	VPRINTK("in xx_freeze : HControl = 0x%x, HStatus = 0x%x\n",
		ioread32(hcr_base + HCONTROL), ioread32(hcr_base + HSTATUS));
}

static void sata_fsl_thaw(struct ata_port *ap)
{
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;

	/* ack. any pending IRQs for this controller/port */
	temp = ioread32(hcr_base + HSTATUS);

	VPRINTK("xx_thaw, pending IRQs = 0x%x\n", (temp & 0x3F));

	if (temp & 0x3F)
		iowrite32((temp & 0x3F), hcr_base + HSTATUS);

	/* enable interrupts on the controller/port */
	temp = ioread32(hcr_base + HCONTROL);
	iowrite32((temp | DEFAULT_PORT_IRQ_ENABLE_MASK), hcr_base + HCONTROL);

	VPRINTK("xx_thaw : HControl = 0x%x, HStatus = 0x%x\n",
		ioread32(hcr_base + HCONTROL), ioread32(hcr_base + HSTATUS));
}

/*
 * NOTE : 1st D2H FIS from device does not update sfis in command descriptor.
 */
static inline void sata_fsl_cache_taskfile_from_d2h_fis(struct ata_queued_cmd
							*qc,
							struct ata_port *ap)
{
	struct sata_fsl_port_priv *pp = ap->private_data;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	unsigned int tag = sata_fsl_tag(qc->tag, hcr_base);
	struct command_desc *cd;

	cd = pp->cmdentry + tag;

	ata_tf_from_fis(cd->sfis, &pp->tf);
}

static u8 sata_fsl_check_status(struct ata_port *ap)
{
	struct sata_fsl_port_priv *pp = ap->private_data;

	return pp->tf.command;
}

static void sata_fsl_tf_read(struct ata_port *ap, struct ata_taskfile *tf)
{
	struct sata_fsl_port_priv *pp = ap->private_data;

	*tf = pp->tf;
}

static int sata_fsl_port_start(struct ata_port *ap)
{
	struct device *dev = ap->host->dev;
	struct sata_fsl_port_priv *pp;
	int retval;
	void *mem;
	dma_addr_t mem_dma;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;

	pp = kzalloc(sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;

	/*
	 * allocate per command dma alignment pad buffer, which is used
	 * internally by libATA to ensure that all transfers ending on
	 * unaligned boundaries are padded, to align on Dword boundaries
	 */
	retval = ata_pad_alloc(ap, dev);
	if (retval) {
		kfree(pp);
		return retval;
	}

	mem = dma_alloc_coherent(dev, SATA_FSL_PORT_PRIV_DMA_SZ, &mem_dma,
				 GFP_KERNEL);
	if (!mem) {
		ata_pad_free(ap, dev);
		kfree(pp);
		return -ENOMEM;
	}
	memset(mem, 0, SATA_FSL_PORT_PRIV_DMA_SZ);

	pp->cmdslot = mem;
	pp->cmdslot_paddr = mem_dma;

	mem += SATA_FSL_CMD_SLOT_SIZE;
	mem_dma += SATA_FSL_CMD_SLOT_SIZE;

	pp->cmdentry = mem;
	pp->cmdentry_paddr = mem_dma;

	ap->private_data = pp;

	VPRINTK("CHBA = 0x%x, cmdentry_phys = 0x%x\n",
		pp->cmdslot_paddr, pp->cmdentry_paddr);

	/* Now, update the CHBA register in host controller cmd register set */
	iowrite32(pp->cmdslot_paddr & 0xffffffff, hcr_base + CHBA);

	/*
	 * Now, we can bring the controller on-line & also initiate
	 * the COMINIT sequence, we simply return here and the boot-probing
	 * & device discovery process is re-initiated by libATA using a
	 * Softreset EH (dummy) session. Hence, boot probing and device
	 * discovey will be part of sata_fsl_softreset() callback.
	 */

	temp = ioread32(hcr_base + HCONTROL);
	iowrite32((temp | HCONTROL_ONLINE_PHY_RST), hcr_base + HCONTROL);

	VPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
	VPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));
	VPRINTK("CHBA  = 0x%x\n", ioread32(hcr_base + CHBA));

#ifdef CONFIG_MPC8315_DS
	/*
	 * Workaround for 8315DS board 3gbps link-up issue,
	 * currently limit SATA port to GEN1 speed
	 */
	sata_fsl_scr_read(ap, SCR_CONTROL, &temp);
	temp &= ~(0xF << 4);
	temp |= (0x1 << 4);
	sata_fsl_scr_write(ap, SCR_CONTROL, temp);

	sata_fsl_scr_read(ap, SCR_CONTROL, &temp);
	dev_printk(KERN_WARNING, dev, "scr_control, speed limited to %x\n",
			temp);
#endif

	return 0;
}

static void sata_fsl_port_stop(struct ata_port *ap)
{
	struct device *dev = ap->host->dev;
	struct sata_fsl_port_priv *pp = ap->private_data;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;

	/*
	 * Force host controller to go off-line, aborting current operations
	 */
	temp = ioread32(hcr_base + HCONTROL);
	temp &= ~HCONTROL_ONLINE_PHY_RST;
	temp |= HCONTROL_FORCE_OFFLINE;
	iowrite32(temp, hcr_base + HCONTROL);

	/* Poll for controller to go offline - should happen immediately */
	ata_wait_register(hcr_base + HSTATUS, ONLINE, ONLINE, 1, 1);

	ap->private_data = NULL;
	dma_free_coherent(dev, SATA_FSL_PORT_PRIV_DMA_SZ,
			  pp->cmdslot, pp->cmdslot_paddr);

	ata_pad_free(ap, dev);
	kfree(pp);
}

static unsigned int sata_fsl_dev_classify(struct ata_port *ap)
{
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	struct ata_taskfile tf;
	u32 temp;

	temp = ioread32(hcr_base + SIGNATURE);

	VPRINTK("raw sig = 0x%x\n", temp);
	VPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
	VPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));

	tf.lbah = (temp >> 24) & 0xff;
	tf.lbam = (temp >> 16) & 0xff;
	tf.lbal = (temp >> 8) & 0xff;
	tf.nsect = temp & 0xff;

	return ata_dev_classify(&tf);
}

static int sata_fsl_softreset(struct ata_link *link, unsigned int *class,
			      unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct sata_fsl_port_priv *pp = ap->private_data;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;
	struct ata_taskfile tf;
	u8 *cfis;
	u32 Serror;
	int i = 0;
	unsigned long start_jiffies;

	DPRINTK("in xx_softreset\n");

try_offline_again:
	/*
	 * Force host controller to go off-line, aborting current operations
	 */
	temp = ioread32(hcr_base + HCONTROL);
	temp &= ~HCONTROL_ONLINE_PHY_RST;
	iowrite32(temp, hcr_base + HCONTROL);

	/* Poll for controller to go offline */
	temp = ata_wait_register(hcr_base + HSTATUS, ONLINE, ONLINE, 1, 500);

	if (temp & ONLINE) {
		ata_port_printk(ap, KERN_ERR,
				"Softreset failed, not off-lined %d\n", i);

		/*
		 * Try to offline controller atleast twice
		 */
		i++;
		if (i == 2)
			goto err;
		else
			goto try_offline_again;
	}

	DPRINTK("softreset, controller off-lined\n");
	VPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
	VPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));

	/*
	 * PHY reset should remain asserted for atleast 1ms
	 */
	msleep(1);

	/*
	 * Now, bring the host controller online again, this can take time
	 * as PHY reset and communication establishment, 1st D2H FIS and
	 * device signature update is done, on safe side assume 500ms
	 * NOTE : Host online status may be indicated immediately!!
	 */

	temp = ioread32(hcr_base + HCONTROL);
	temp |= (HCONTROL_ONLINE_PHY_RST | HCONTROL_SNOOP_ENABLE);
	iowrite32(temp, hcr_base + HCONTROL);

	temp = ata_wait_register(hcr_base + HSTATUS, ONLINE, 0, 1, 500);

	if (!(temp & ONLINE)) {
		ata_port_printk(ap, KERN_ERR,
				"Softreset failed, not on-lined\n");
		goto err;
	}

	DPRINTK("softreset, controller off-lined & on-lined\n");
	VPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
	VPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));

	/*
	 * First, wait for the PHYRDY change to occur before waiting for
	 * the signature, and also verify if SStatus indicates device
	 * presence
	 */

	temp = ata_wait_register(hcr_base + HSTATUS, 0xFF, 0, 1, 500);
	if ((!(temp & 0x10)) || ata_link_offline(link)) {
		ata_port_printk(ap, KERN_WARNING,
				"No Device OR PHYRDY change,Hstatus = 0x%x\n",
				ioread32(hcr_base + HSTATUS));
		goto err;
	}

	/*
	 * Wait for the first D2H from device,i.e,signature update notification
	 */
	start_jiffies = jiffies;
	temp = ata_wait_register(hcr_base + HSTATUS, 0xFF, 0x10,
			500, jiffies_to_msecs(deadline - start_jiffies));

	if ((temp & 0xFF) != 0x18) {
		ata_port_printk(ap, KERN_WARNING, "No Signature Update\n");
		goto err;
	} else {
		ata_port_printk(ap, KERN_INFO,
				"Signature Update detected @ %d msecs\n",
				jiffies_to_msecs(jiffies - start_jiffies));
	}

	/*
	 * Send a device reset (SRST) explicitly on command slot #0
	 * Check : will the command queue (reg) be cleared during offlining ??
	 * Also we will be online only if Phy commn. has been established
	 * and device presence has been detected, therefore if we have
	 * reached here, we can send a command to the target device
	 */

	DPRINTK("Sending SRST/device reset\n");

	ata_tf_init(link->device, &tf);
	cfis = (u8 *) &pp->cmdentry->cfis;

	/* device reset/SRST is a control register update FIS, uses tag0 */
	sata_fsl_setup_cmd_hdr_entry(pp, 0,
				     SRST_CMD | CMD_DESC_SNOOP_ENABLE, 0, 0, 5);

	tf.ctl |= ATA_SRST;	/* setup SRST bit in taskfile control reg */
	ata_tf_to_fis(&tf, 0, 0, cfis);

	DPRINTK("Dumping cfis : 0x%x, 0x%x, 0x%x, 0x%x\n",
		cfis[0], cfis[1], cfis[2], cfis[3]);

	/*
	 * Queue SRST command to the controller/device, ensure that no
	 * other commands are active on the controller/device
	 */

	DPRINTK("@Softreset, CQ = 0x%x, CA = 0x%x, CC = 0x%x\n",
		ioread32(CQ + hcr_base),
		ioread32(CA + hcr_base), ioread32(CC + hcr_base));

	iowrite32(0xFFFF, CC + hcr_base);
	iowrite32(1, CQ + hcr_base);

	temp = ata_wait_register(CQ + hcr_base, 0x1, 0x1, 1, 5000);
	if (temp & 0x1) {
		ata_port_printk(ap, KERN_WARNING, "ATA_SRST issue failed\n");

		DPRINTK("Softreset@5000,CQ=0x%x,CA=0x%x,CC=0x%x\n",
			ioread32(CQ + hcr_base),
			ioread32(CA + hcr_base), ioread32(CC + hcr_base));

		sata_fsl_scr_read(ap, SCR_ERROR, &Serror);

		DPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
		DPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));
		DPRINTK("Serror = 0x%x\n", Serror);
		goto err;
	}

	msleep(1);

	/*
	 * SATA device enters reset state after receving a Control register
	 * FIS with SRST bit asserted and it awaits another H2D Control reg.
	 * FIS with SRST bit cleared, then the device does internal diags &
	 * initialization, followed by indicating it's initialization status
	 * using ATA signature D2H register FIS to the host controller.
	 */

	sata_fsl_setup_cmd_hdr_entry(pp, 0, CMD_DESC_SNOOP_ENABLE, 0, 0, 5);

	tf.ctl &= ~ATA_SRST;	/* 2nd H2D Ctl. register FIS */
	ata_tf_to_fis(&tf, 0, 0, cfis);

	iowrite32(1, CQ + hcr_base);
	msleep(150);		/* ?? */

	/*
	 * The above command would have signalled an interrupt on command
	 * complete, which needs special handling, by clearing the Nth
	 * command bit of the CCreg
	 */
	iowrite32(0x01, CC + hcr_base);	/* We know it will be cmd#0 always */

	DPRINTK("SATA FSL : Now checking device signature\n");

	*class = ATA_DEV_NONE;

	/* Verify if SStatus indicates device presence */
	if (ata_link_online(link)) {
		/*
		 * if we are here, device presence has been detected,
		 * 1st D2H FIS would have been received, but sfis in
		 * command desc. is not updated, but signature register
		 * would have been updated
		 */

		*class = sata_fsl_dev_classify(ap);

		DPRINTK("class = %d\n", *class);
		VPRINTK("ccreg = 0x%x\n", ioread32(hcr_base + CC));
		VPRINTK("cereg = 0x%x\n", ioread32(hcr_base + CE));
	}

	return 0;

err:
	return -EIO;
}

static void sata_fsl_error_handler(struct ata_port *ap)
{

	DPRINTK("in xx_error_handler\n");

	/* perform recovery */
	ata_do_eh(ap, ata_std_prereset, sata_fsl_softreset, sata_std_hardreset,
		  ata_std_postreset);
}

static void sata_fsl_post_internal_cmd(struct ata_queued_cmd *qc)
{
	if (qc->flags & ATA_QCFLAG_FAILED)
		qc->err_mask |= AC_ERR_OTHER;

	if (qc->err_mask) {
		/* make DMA engine forget about the failed command */

	}
}

static void sata_fsl_irq_clear(struct ata_port *ap)
{
	/* unused */
}

static void sata_fsl_error_intr(struct ata_port *ap)
{
	struct ata_link *link = &ap->link;
	struct ata_eh_info *ehi = &link->eh_info;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 hstatus, dereg, cereg = 0, SError = 0;
	unsigned int err_mask = 0, action = 0;
	struct ata_queued_cmd *qc;
	int freeze = 0;

	hstatus = ioread32(hcr_base + HSTATUS);
	cereg = ioread32(hcr_base + CE);

	ata_ehi_clear_desc(ehi);

	/*
	 * Handle & Clear SError
	 */

	sata_fsl_scr_read(ap, SCR_ERROR, &SError);
	if (unlikely(SError & 0xFFFF0000)) {
		sata_fsl_scr_write(ap, SCR_ERROR, SError);
		err_mask |= AC_ERR_ATA_BUS;
	}

	DPRINTK("error_intr,hStat=0x%x,CE=0x%x,DE =0x%x,SErr=0x%x\n",
		hstatus, cereg, ioread32(hcr_base + DE), SError);

	/* handle single device errors */
	if (cereg) {
		/*
		 * clear the command error, also clears queue to the device
		 * in error, and we can (re)issue commands to this device.
		 * When a device is in error all commands queued into the
		 * host controller and at the device are considered aborted
		 * and the queue for that device is stopped. Now, after
		 * clearing the device error, we can issue commands to the
		 * device to interrogate it to find the source of the error.
		 */
		dereg = ioread32(hcr_base + DE);
		iowrite32(dereg, hcr_base + DE);
		iowrite32(cereg, hcr_base + CE);

		DPRINTK("single device error, CE=0x%x, DE=0x%x\n",
			ioread32(hcr_base + CE), ioread32(hcr_base + DE));
		/*
		 * We should consider this as non fatal error, and TF must
		 * be updated as done below.
		 */

		err_mask |= AC_ERR_DEV;
	}

	/* handle fatal errors */
	if (hstatus & FATAL_ERROR_DECODE) {
		err_mask |= AC_ERR_ATA_BUS;
		action |= ATA_EH_SOFTRESET;
		/* how will fatal error interrupts be completed ?? */
		freeze = 1;
	}

	/* Handle PHYRDY change notification */
	if (hstatus & INT_ON_PHYRDY_CHG) {
		DPRINTK("SATA FSL: PHYRDY change indication\n");

		/* Setup a soft-reset EH action */
		ata_ehi_hotplugged(ehi);
		freeze = 1;
	}

	/* record error info */
	qc = ata_qc_from_tag(ap, link->active_tag);

	if (qc) {
		sata_fsl_cache_taskfile_from_d2h_fis(qc, qc->ap);
		qc->err_mask |= err_mask;
	} else
		ehi->err_mask |= err_mask;

	ehi->action |= action;
	ehi->serror |= SError;

	/* freeze or abort */
	if (freeze)
		ata_port_freeze(ap);
	else
		ata_port_abort(ap);
}

static void sata_fsl_qc_complete(struct ata_queued_cmd *qc)
{
	if (qc->flags & ATA_QCFLAG_RESULT_TF) {
		DPRINTK("xx_qc_complete called\n");
		sata_fsl_cache_taskfile_from_d2h_fis(qc, qc->ap);
	}
}

static void sata_fsl_host_intr(struct ata_port *ap)
{
	struct ata_link *link = &ap->link;
	struct sata_fsl_host_priv *host_priv = ap->host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 hstatus, qc_active = 0;
	struct ata_queued_cmd *qc;
	u32 SError;

	hstatus = ioread32(hcr_base + HSTATUS);

	sata_fsl_scr_read(ap, SCR_ERROR, &SError);

	if (unlikely(SError & 0xFFFF0000)) {
		DPRINTK("serror @host_intr : 0x%x\n", SError);
		sata_fsl_error_intr(ap);

	}

	if (unlikely(hstatus & INT_ON_ERROR)) {
		DPRINTK("error interrupt!!\n");
		sata_fsl_error_intr(ap);
		return;
	}

	if (link->sactive) {	/* only true for NCQ commands */
		int i;
		/* Read command completed register */
		qc_active = ioread32(hcr_base + CC);
		/* clear CC bit, this will also complete the interrupt */
		iowrite32(qc_active, hcr_base + CC);

		DPRINTK("Status of all queues :\n");
		DPRINTK("qc_active/CC = 0x%x, CA = 0x%x, CE=0x%x\n",
			qc_active, ioread32(hcr_base + CA),
			ioread32(hcr_base + CE));

		for (i = 0; i < SATA_FSL_QUEUE_DEPTH; i++) {
			if (qc_active & (1 << i)) {
				qc = ata_qc_from_tag(ap, i);
				if (qc) {
					sata_fsl_qc_complete(qc);
					ata_qc_complete(qc);
				}
				DPRINTK
				    ("completing ncq cmd,tag=%d,CC=0x%x,CA=0x%x\n",
				     i, ioread32(hcr_base + CC),
				     ioread32(hcr_base + CA));
			}
		}
		return;

	} else if (ap->qc_active) {
		iowrite32(1, hcr_base + CC);
		qc = ata_qc_from_tag(ap, link->active_tag);

		DPRINTK("completing non-ncq cmd, tag=%d,CC=0x%x\n",
			link->active_tag, ioread32(hcr_base + CC));

		if (qc) {
			sata_fsl_qc_complete(qc);
			ata_qc_complete(qc);
		}
	} else {
		/* Spurious Interrupt!! */
		DPRINTK("spurious interrupt!!, CC = 0x%x\n",
			ioread32(hcr_base + CC));
		return;
	}
}

static irqreturn_t sata_fsl_interrupt(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	struct sata_fsl_host_priv *host_priv = host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 interrupt_enables;
	unsigned handled = 0;
	struct ata_port *ap;

	/* ack. any pending IRQs for this controller/port */
	interrupt_enables = ioread32(hcr_base + HSTATUS);
	interrupt_enables &= 0x3F;

	DPRINTK("interrupt status 0x%x\n", interrupt_enables);

	if (!interrupt_enables)
		return IRQ_NONE;

	spin_lock(&host->lock);

	/* Assuming one port per host controller */

	ap = host->ports[0];
	if (ap) {
		sata_fsl_host_intr(ap);
	} else {
		dev_printk(KERN_WARNING, host->dev,
			   "interrupt on disabled port 0\n");
	}

	iowrite32(interrupt_enables, hcr_base + HSTATUS);
	handled = 1;

	spin_unlock(&host->lock);

	return IRQ_RETVAL(handled);
}

/*
 * Multiple ports are represented by multiple SATA controllers with
 * one port per controller
 */
static int sata_fsl_init_controller(struct ata_host *host)
{
	struct sata_fsl_host_priv *host_priv = host->private_data;
	void __iomem *hcr_base = host_priv->hcr_base;
	u32 temp;

	/*
	 * NOTE : We cannot bring the controller online before setting
	 * the CHBA, hence main controller initialization is done as
	 * part of the port_start() callback
	 */

	/* ack. any pending IRQs for this controller/port */
	temp = ioread32(hcr_base + HSTATUS);
	if (temp & 0x3F)
		iowrite32((temp & 0x3F), hcr_base + HSTATUS);

	/* Keep interrupts disabled on the controller */
	temp = ioread32(hcr_base + HCONTROL);
	iowrite32((temp & ~0x3F), hcr_base + HCONTROL);

	/* Disable interrupt coalescing control(icc), for the moment */
	DPRINTK("icc = 0x%x\n", ioread32(hcr_base + ICC));
	iowrite32(0x01000000, hcr_base + ICC);

	/* clear error registers, SError is cleared by libATA  */
	iowrite32(0x00000FFFF, hcr_base + CE);
	iowrite32(0x00000FFFF, hcr_base + DE);

	/* initially assuming no Port multiplier, set CQPMP to 0 */
	iowrite32(0x0, hcr_base + CQPMP);

	/*
	 * host controller will be brought on-line, during xx_port_start()
	 * callback, that should also initiate the OOB, COMINIT sequence
	 */

	DPRINTK("HStatus = 0x%x\n", ioread32(hcr_base + HSTATUS));
	DPRINTK("HControl = 0x%x\n", ioread32(hcr_base + HCONTROL));

	return 0;
}

/*
 * scsi mid-layer and libata interface structures
 */
static struct scsi_host_template sata_fsl_sht = {
	.module = THIS_MODULE,
	.name = "sata_fsl",
	.ioctl = ata_scsi_ioctl,
	.queuecommand = ata_scsi_queuecmd,
	.change_queue_depth = ata_scsi_change_queue_depth,
	.can_queue = SATA_FSL_QUEUE_DEPTH,
	.this_id = ATA_SHT_THIS_ID,
	.sg_tablesize = SATA_FSL_MAX_PRD_USABLE,
	.cmd_per_lun = ATA_SHT_CMD_PER_LUN,
	.emulated = ATA_SHT_EMULATED,
	.use_clustering = ATA_SHT_USE_CLUSTERING,
	.proc_name = "sata_fsl",
	.dma_boundary = ATA_DMA_BOUNDARY,
	.slave_configure = ata_scsi_slave_config,
	.slave_destroy = ata_scsi_slave_destroy,
	.bios_param = ata_std_bios_param,
};

static const struct ata_port_operations sata_fsl_ops = {
	.check_status = sata_fsl_check_status,
	.check_altstatus = sata_fsl_check_status,
	.dev_select = ata_noop_dev_select,

	.tf_read = sata_fsl_tf_read,

	.qc_prep = sata_fsl_qc_prep,
	.qc_issue = sata_fsl_qc_issue,
	.irq_clear = sata_fsl_irq_clear,

	.scr_read = sata_fsl_scr_read,
	.scr_write = sata_fsl_scr_write,

	.freeze = sata_fsl_freeze,
	.thaw = sata_fsl_thaw,
	.error_handler = sata_fsl_error_handler,
	.post_internal_cmd = sata_fsl_post_internal_cmd,

	.port_start = sata_fsl_port_start,
	.port_stop = sata_fsl_port_stop,
};

static const struct ata_port_info sata_fsl_port_info[] = {
	{
	 .flags = SATA_FSL_HOST_FLAGS,
	 .link_flags = SATA_FSL_HOST_LFLAGS,
	 .pio_mask = 0x1f,	/* pio 0-4 */
	 .udma_mask = 0x7f,	/* udma 0-6 */
	 .port_ops = &sata_fsl_ops,
	 },
};

static int sata_fsl_probe(struct of_device *ofdev,
			const struct of_device_id *match)
{
	int retval = 0;
	void __iomem *hcr_base = NULL;
	void __iomem *ssr_base = NULL;
	void __iomem *csr_base = NULL;
	struct sata_fsl_host_priv *host_priv = NULL;
	struct resource *r;
	int irq;
	struct ata_host *host;

	struct ata_port_info pi = sata_fsl_port_info[0];
	const struct ata_port_info *ppi[] = { &pi, NULL };

	dev_printk(KERN_INFO, &ofdev->dev,
		   "Sata FSL Platform/CSB Driver init\n");

	r = kmalloc(sizeof(struct resource), GFP_KERNEL);

	hcr_base = of_iomap(ofdev->node, 0);
	if (!hcr_base)
		goto error_exit_with_cleanup;

	ssr_base = hcr_base + 0x100;
	csr_base = hcr_base + 0x140;

	DPRINTK("@reset i/o = 0x%x\n", ioread32(csr_base + TRANSCFG));
	DPRINTK("sizeof(cmd_desc) = %d\n", sizeof(struct command_desc));
	DPRINTK("sizeof(#define cmd_desc) = %d\n", SATA_FSL_CMD_DESC_SIZE);

	host_priv = kzalloc(sizeof(struct sata_fsl_host_priv), GFP_KERNEL);
	if (!host_priv)
		goto error_exit_with_cleanup;

	host_priv->hcr_base = hcr_base;
	host_priv->ssr_base = ssr_base;
	host_priv->csr_base = csr_base;

	irq = irq_of_parse_and_map(ofdev->node, 0);
	if (irq < 0) {
		dev_printk(KERN_ERR, &ofdev->dev, "invalid irq from platform\n");
		goto error_exit_with_cleanup;
	}
	host_priv->irq = irq;

	/* allocate host structure */
	host = ata_host_alloc_pinfo(&ofdev->dev, ppi, SATA_FSL_MAX_PORTS);

	/* host->iomap is not used currently */
	host->private_data = host_priv;

	/* setup port(s) */

	host->ports[0]->ioaddr.cmd_addr = host_priv->hcr_base;
	host->ports[0]->ioaddr.scr_addr = host_priv->ssr_base;

	/* initialize host controller */
	sata_fsl_init_controller(host);

	/*
	 * Now, register with libATA core, this will also initiate the
	 * device discovery process, invoking our port_start() handler &
	 * error_handler() to execute a dummy Softreset EH session
	 */
	ata_host_activate(host, irq, sata_fsl_interrupt, SATA_FSL_IRQ_FLAG,
			  &sata_fsl_sht);

	dev_set_drvdata(&ofdev->dev, host);

	return 0;

error_exit_with_cleanup:

	if (hcr_base)
		iounmap(hcr_base);
	if (host_priv)
		kfree(host_priv);

	return retval;
}

static int sata_fsl_remove(struct of_device *ofdev)
{
	struct ata_host *host = dev_get_drvdata(&ofdev->dev);
	struct sata_fsl_host_priv *host_priv = host->private_data;

	ata_host_detach(host);

	dev_set_drvdata(&ofdev->dev, NULL);

	irq_dispose_mapping(host_priv->irq);
	iounmap(host_priv->hcr_base);
	kfree(host_priv);

	return 0;
}

static struct of_device_id fsl_sata_match[] = {
	{
		.compatible = "fsl,mpc8315-sata",
	},
	{
		.compatible = "fsl,mpc8379-sata",
	},
	{},
};

MODULE_DEVICE_TABLE(of, fsl_sata_match);

static struct of_platform_driver fsl_sata_driver = {
	.name		= "fsl-sata",
	.match_table	= fsl_sata_match,
	.probe		= sata_fsl_probe,
	.remove		= sata_fsl_remove,
};

static int __init sata_fsl_init(void)
{
	of_register_platform_driver(&fsl_sata_driver);
	return 0;
}

static void __exit sata_fsl_exit(void)
{
	of_unregister_platform_driver(&fsl_sata_driver);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ashish Kalra, Freescale Semiconductor");
MODULE_DESCRIPTION("Freescale 3.0Gbps SATA controller low level driver");
MODULE_VERSION("1.10");

module_init(sata_fsl_init);
module_exit(sata_fsl_exit);

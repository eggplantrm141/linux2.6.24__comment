/*
 * libata-acpi.c
 * Provides ACPI support for PATA/SATA.
 *
 * Copyright (C) 2006 Intel Corp.
 * Copyright (C) 2006 Randy Dunlap
 */

#include <linux/module.h>
#include <linux/ata.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/libata.h>
#include <linux/pci.h>
#include <scsi/scsi_device.h>
#include "libata.h"

#include <acpi/acpi_bus.h>
#include <acpi/acnames.h>
#include <acpi/acnamesp.h>
#include <acpi/acparser.h>
#include <acpi/acexcep.h>
#include <acpi/acmacros.h>
#include <acpi/actypes.h>

enum {
	ATA_ACPI_FILTER_SETXFER	= 1 << 0,
	ATA_ACPI_FILTER_LOCK	= 1 << 1,

	ATA_ACPI_FILTER_DEFAULT	= ATA_ACPI_FILTER_SETXFER |
				  ATA_ACPI_FILTER_LOCK,
};

static unsigned int ata_acpi_gtf_filter = ATA_ACPI_FILTER_DEFAULT;
module_param_named(acpi_gtf_filter, ata_acpi_gtf_filter, int, 0644);
MODULE_PARM_DESC(acpi_gtf_filter, "filter mask for ACPI _GTF commands, set to filter out (0x1=set xfermode, 0x2=lock/freeze lock)");

#define NO_PORT_MULT		0xffff
#define SATA_ADR(root, pmp)	(((root) << 16) | (pmp))

#define REGS_PER_GTF		7
struct ata_acpi_gtf {
	u8	tf[REGS_PER_GTF];	/* regs. 0x1f1 - 0x1f7 */
} __packed;

/*
 *	Helper - belongs in the PCI layer somewhere eventually
 */
static int is_pci_dev(struct device *dev)
{
	return (dev->bus == &pci_bus_type);
}

static void ata_acpi_clear_gtf(struct ata_device *dev)
{
	kfree(dev->gtf_cache);
	dev->gtf_cache = NULL;
}

/**
 * ata_acpi_associate_sata_port - associate SATA port with ACPI objects
 * @ap: target SATA port
 *
 * Look up ACPI objects associated with @ap and initialize acpi_handle
 * fields of @ap, the port and devices accordingly.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
void ata_acpi_associate_sata_port(struct ata_port *ap)
{
	WARN_ON(!(ap->flags & ATA_FLAG_ACPI_SATA));

	if (!ap->nr_pmp_links) {
		acpi_integer adr = SATA_ADR(ap->port_no, NO_PORT_MULT);

		ap->link.device->acpi_handle =
			acpi_get_child(ap->host->acpi_handle, adr);
	} else {
		struct ata_link *link;

		ap->link.device->acpi_handle = NULL;

		ata_port_for_each_link(link, ap) {
			acpi_integer adr = SATA_ADR(ap->port_no, link->pmp);

			link->device->acpi_handle =
				acpi_get_child(ap->host->acpi_handle, adr);
		}
	}
}

static void ata_acpi_associate_ide_port(struct ata_port *ap)
{
	int max_devices, i;

	ap->acpi_handle = acpi_get_child(ap->host->acpi_handle, ap->port_no);
	if (!ap->acpi_handle)
		return;

	max_devices = 1;
	if (ap->flags & ATA_FLAG_SLAVE_POSS)
		max_devices++;

	for (i = 0; i < max_devices; i++) {
		struct ata_device *dev = &ap->link.device[i];

		dev->acpi_handle = acpi_get_child(ap->acpi_handle, i);
	}

	if (ata_acpi_gtm(ap, &ap->__acpi_init_gtm) == 0)
		ap->pflags |= ATA_PFLAG_INIT_GTM_VALID;
}

static void ata_acpi_handle_hotplug(struct ata_port *ap, struct kobject *kobj,
				    u32 event)
{
	char event_string[12];
	char *envp[] = { event_string, NULL };
	struct ata_eh_info *ehi = &ap->link.eh_info;

	if (event == 0 || event == 1) {
	       unsigned long flags;
	       spin_lock_irqsave(ap->lock, flags);
	       ata_ehi_clear_desc(ehi);
	       ata_ehi_push_desc(ehi, "ACPI event");
	       ata_ehi_hotplugged(ehi);
	       ata_port_freeze(ap);
	       spin_unlock_irqrestore(ap->lock, flags);
	}

	if (kobj) {
		sprintf(event_string, "BAY_EVENT=%d", event);
		kobject_uevent_env(kobj, KOBJ_CHANGE, envp);
	}
}

static void ata_acpi_dev_notify(acpi_handle handle, u32 event, void *data)
{
	struct ata_device *dev = data;
	struct kobject *kobj = NULL;

	if (dev->sdev)
		kobj = &dev->sdev->sdev_gendev.kobj;

	ata_acpi_handle_hotplug(dev->link->ap, kobj, event);
}

static void ata_acpi_ap_notify(acpi_handle handle, u32 event, void *data)
{
	struct ata_port *ap = data;

	ata_acpi_handle_hotplug(ap, &ap->dev->kobj, event);
}

/**
 * ata_acpi_associate - associate ATA host with ACPI objects
 * @host: target ATA host
 *
 * Look up ACPI objects associated with @host and initialize
 * acpi_handle fields of @host, its ports and devices accordingly.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
void ata_acpi_associate(struct ata_host *host)
{
	int i, j;

	if (!is_pci_dev(host->dev) || libata_noacpi)
		return;

	host->acpi_handle = DEVICE_ACPI_HANDLE(host->dev);
	if (!host->acpi_handle)
		return;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		if (host->ports[0]->flags & ATA_FLAG_ACPI_SATA)
			ata_acpi_associate_sata_port(ap);
		else
			ata_acpi_associate_ide_port(ap);

		if (ap->acpi_handle)
			acpi_install_notify_handler (ap->acpi_handle,
						     ACPI_SYSTEM_NOTIFY,
						     ata_acpi_ap_notify,
						     ap);

		for (j = 0; j < ata_link_max_devices(&ap->link); j++) {
			struct ata_device *dev = &ap->link.device[j];

			if (dev->acpi_handle)
				acpi_install_notify_handler (dev->acpi_handle,
							     ACPI_SYSTEM_NOTIFY,
							     ata_acpi_dev_notify,
							     dev);
		}
	}
}

/**
 * ata_acpi_dissociate - dissociate ATA host from ACPI objects
 * @host: target ATA host
 *
 * This function is called during driver detach after the whole host
 * is shut down.
 *
 * LOCKING:
 * EH context.
 */
void ata_acpi_dissociate(struct ata_host *host)
{
	int i;

	/* Restore initial _GTM values so that driver which attaches
	 * afterward can use them too.
	 */
	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		const struct ata_acpi_gtm *gtm = ata_acpi_init_gtm(ap);

		if (ap->acpi_handle && gtm)
			ata_acpi_stm(ap, gtm);
	}
}

/**
 * ata_acpi_gtm - execute _GTM
 * @ap: target ATA port
 * @gtm: out parameter for _GTM result
 *
 * Evaluate _GTM and store the result in @gtm.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -ENOENT if _GTM doesn't exist, -errno on failure.
 */
int ata_acpi_gtm(struct ata_port *ap, struct ata_acpi_gtm *gtm)
{
	struct acpi_buffer output = { .length = ACPI_ALLOCATE_BUFFER };
	union acpi_object *out_obj;
	acpi_status status;
	int rc = 0;

	status = acpi_evaluate_object(ap->acpi_handle, "_GTM", NULL, &output);

	rc = -ENOENT;
	if (status == AE_NOT_FOUND)
		goto out_free;

	rc = -EINVAL;
	if (ACPI_FAILURE(status)) {
		ata_port_printk(ap, KERN_ERR,
				"ACPI get timing mode failed (AE 0x%x)\n",
				status);
		goto out_free;
	}

	out_obj = output.pointer;
	if (out_obj->type != ACPI_TYPE_BUFFER) {
		ata_port_printk(ap, KERN_WARNING,
				"_GTM returned unexpected object type 0x%x\n",
				out_obj->type);

		goto out_free;
	}

	if (out_obj->buffer.length != sizeof(struct ata_acpi_gtm)) {
		ata_port_printk(ap, KERN_ERR,
				"_GTM returned invalid length %d\n",
				out_obj->buffer.length);
		goto out_free;
	}

	memcpy(gtm, out_obj->buffer.pointer, sizeof(struct ata_acpi_gtm));
	rc = 0;
 out_free:
	kfree(output.pointer);
	return rc;
}

EXPORT_SYMBOL_GPL(ata_acpi_gtm);

/**
 * ata_acpi_stm - execute _STM
 * @ap: target ATA port
 * @stm: timing parameter to _STM
 *
 * Evaluate _STM with timing parameter @stm.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -ENOENT if _STM doesn't exist, -errno on failure.
 */
int ata_acpi_stm(struct ata_port *ap, const struct ata_acpi_gtm *stm)
{
	acpi_status status;
	struct ata_acpi_gtm		stm_buf = *stm;
	struct acpi_object_list         input;
	union acpi_object               in_params[3];

	in_params[0].type = ACPI_TYPE_BUFFER;
	in_params[0].buffer.length = sizeof(struct ata_acpi_gtm);
	in_params[0].buffer.pointer = (u8 *)&stm_buf;
	/* Buffers for id may need byteswapping ? */
	in_params[1].type = ACPI_TYPE_BUFFER;
	in_params[1].buffer.length = 512;
	in_params[1].buffer.pointer = (u8 *)ap->link.device[0].id;
	in_params[2].type = ACPI_TYPE_BUFFER;
	in_params[2].buffer.length = 512;
	in_params[2].buffer.pointer = (u8 *)ap->link.device[1].id;

	input.count = 3;
	input.pointer = in_params;

	status = acpi_evaluate_object(ap->acpi_handle, "_STM", &input, NULL);

	if (status == AE_NOT_FOUND)
		return -ENOENT;
	if (ACPI_FAILURE(status)) {
		ata_port_printk(ap, KERN_ERR,
			"ACPI set timing mode failed (status=0x%x)\n", status);
		return -EINVAL;
	}
	return 0;
}

EXPORT_SYMBOL_GPL(ata_acpi_stm);

/**
 * ata_dev_get_GTF - get the drive bootup default taskfile settings
 * @dev: target ATA device
 * @gtf: output parameter for buffer containing _GTF taskfile arrays
 *
 * This applies to both PATA and SATA drives.
 *
 * The _GTF method has no input parameters.
 * It returns a variable number of register set values (registers
 * hex 1F1..1F7, taskfiles).
 * The <variable number> is not known in advance, so have ACPI-CA
 * allocate the buffer as needed and return it, then free it later.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * Number of taskfiles on success, 0 if _GTF doesn't exist.  -EINVAL
 * if _GTF is invalid.
 */
static int ata_dev_get_GTF(struct ata_device *dev, struct ata_acpi_gtf **gtf)
{
	struct ata_port *ap = dev->link->ap;
	acpi_status status;
	struct acpi_buffer output;
	union acpi_object *out_obj;
	int rc = 0;

	/* if _GTF is cached, use the cached value */
	if (dev->gtf_cache) {
		out_obj = dev->gtf_cache;
		goto done;
	}

	/* set up output buffer */
	output.length = ACPI_ALLOCATE_BUFFER;
	output.pointer = NULL;	/* ACPI-CA sets this; save/free it later */

	if (ata_msg_probe(ap))
		ata_dev_printk(dev, KERN_DEBUG, "%s: ENTER: port#: %d\n",
			       __FUNCTION__, ap->port_no);

	/* _GTF has no input parameters */
	status = acpi_evaluate_object(dev->acpi_handle, "_GTF", NULL, &output);
	out_obj = dev->gtf_cache = output.pointer;

	if (ACPI_FAILURE(status)) {
		if (status != AE_NOT_FOUND) {
			ata_dev_printk(dev, KERN_WARNING,
				       "_GTF evaluation failed (AE 0x%x)\n",
				       status);
			rc = -EINVAL;
		}
		goto out_free;
	}

	if (!output.length || !output.pointer) {
		if (ata_msg_probe(ap))
			ata_dev_printk(dev, KERN_DEBUG, "%s: Run _GTF: "
				"length or ptr is NULL (0x%llx, 0x%p)\n",
				__FUNCTION__,
				(unsigned long long)output.length,
				output.pointer);
		rc = -EINVAL;
		goto out_free;
	}

	if (out_obj->type != ACPI_TYPE_BUFFER) {
		ata_dev_printk(dev, KERN_WARNING,
			       "_GTF unexpected object type 0x%x\n",
			       out_obj->type);
		rc = -EINVAL;
		goto out_free;
	}

	if (out_obj->buffer.length % REGS_PER_GTF) {
		ata_dev_printk(dev, KERN_WARNING,
			       "unexpected _GTF length (%d)\n",
			       out_obj->buffer.length);
		rc = -EINVAL;
		goto out_free;
	}

 done:
	rc = out_obj->buffer.length / REGS_PER_GTF;
	if (gtf) {
		*gtf = (void *)out_obj->buffer.pointer;
		if (ata_msg_probe(ap))
			ata_dev_printk(dev, KERN_DEBUG,
				       "%s: returning gtf=%p, gtf_count=%d\n",
				       __FUNCTION__, *gtf, rc);
	}
	return rc;

 out_free:
	ata_acpi_clear_gtf(dev);
	return rc;
}

/**
 * ata_acpi_cbl_80wire		-	Check for 80 wire cable
 * @ap: Port to check
 *
 * Return 1 if the ACPI mode data for this port indicates the BIOS selected
 * an 80wire mode.
 */

int ata_acpi_cbl_80wire(struct ata_port *ap)
{
	const struct ata_acpi_gtm *gtm = ata_acpi_init_gtm(ap);
	int valid = 0;

	if (!gtm)
		return 0;

	/* Split timing, DMA enabled */
	if ((gtm->flags & 0x11) == 0x11 && gtm->drive[0].dma < 55)
		valid |= 1;
	if ((gtm->flags & 0x14) == 0x14 && gtm->drive[1].dma < 55)
		valid |= 2;
	/* Shared timing, DMA enabled */
	if ((gtm->flags & 0x11) == 0x01 && gtm->drive[0].dma < 55)
		valid |= 1;
	if ((gtm->flags & 0x14) == 0x04 && gtm->drive[0].dma < 55)
		valid |= 2;

	/* Drive check */
	if ((valid & 1) && ata_dev_enabled(&ap->link.device[0]))
		return 1;
	if ((valid & 2) && ata_dev_enabled(&ap->link.device[1]))
		return 1;
	return 0;
}

EXPORT_SYMBOL_GPL(ata_acpi_cbl_80wire);

static void ata_acpi_gtf_to_tf(struct ata_device *dev,
			       const struct ata_acpi_gtf *gtf,
			       struct ata_taskfile *tf)
{
	ata_tf_init(dev, tf);

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf->protocol = ATA_PROT_NODATA;
	tf->feature = gtf->tf[0];	/* 0x1f1 */
	tf->nsect   = gtf->tf[1];	/* 0x1f2 */
	tf->lbal    = gtf->tf[2];	/* 0x1f3 */
	tf->lbam    = gtf->tf[3];	/* 0x1f4 */
	tf->lbah    = gtf->tf[4];	/* 0x1f5 */
	tf->device  = gtf->tf[5];	/* 0x1f6 */
	tf->command = gtf->tf[6];	/* 0x1f7 */
}

static int ata_acpi_filter_tf(const struct ata_taskfile *tf,
			      const struct ata_taskfile *ptf)
{
	if (ata_acpi_gtf_filter & ATA_ACPI_FILTER_SETXFER) {
		/* libata doesn't use ACPI to configure transfer mode.
		 * It will only confuse device configuration.  Skip.
		 */
		if (tf->command == ATA_CMD_SET_FEATURES &&
		    tf->feature == SETFEATURES_XFER)
			return 1;
	}

	if (ata_acpi_gtf_filter & ATA_ACPI_FILTER_LOCK) {
		/* BIOS writers, sorry but we don't wanna lock
		 * features unless the user explicitly said so.
		 */

		/* DEVICE CONFIGURATION FREEZE LOCK */
		if (tf->command == ATA_CMD_CONF_OVERLAY &&
		    tf->feature == ATA_DCO_FREEZE_LOCK)
			return 1;

		/* SECURITY FREEZE LOCK */
		if (tf->command == ATA_CMD_SEC_FREEZE_LOCK)
			return 1;

		/* SET MAX LOCK and SET MAX FREEZE LOCK */
		if ((!ptf || ptf->command != ATA_CMD_READ_NATIVE_MAX) &&
		    tf->command == ATA_CMD_SET_MAX &&
		    (tf->feature == ATA_SET_MAX_LOCK ||
		     tf->feature == ATA_SET_MAX_FREEZE_LOCK))
			return 1;
	}

	return 0;
}

/**
 * ata_acpi_run_tf - send taskfile registers to host controller
 * @dev: target ATA device
 * @gtf: raw ATA taskfile register set (0x1f1 - 0x1f7)
 *
 * Outputs ATA taskfile to standard ATA host controller using MMIO
 * or PIO as indicated by the ATA_FLAG_MMIO flag.
 * Writes the control, feature, nsect, lbal, lbam, and lbah registers.
 * Optionally (ATA_TFLAG_LBA48) writes hob_feature, hob_nsect,
 * hob_lbal, hob_lbam, and hob_lbah.
 *
 * This function waits for idle (!BUSY and !DRQ) after writing
 * registers.  If the control register has a new value, this
 * function also waits for idle after writing control and before
 * writing the remaining registers.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 1 if command is executed successfully.  0 if ignored, rejected or
 * filtered out, -errno on other errors.
 */
static int ata_acpi_run_tf(struct ata_device *dev,
			   const struct ata_acpi_gtf *gtf,
			   const struct ata_acpi_gtf *prev_gtf)
{
	struct ata_taskfile *pptf = NULL;
	struct ata_taskfile tf, ptf, rtf;
	unsigned int err_mask;
	const char *level;
	char msg[60];
	int rc;

	if ((gtf->tf[0] == 0) && (gtf->tf[1] == 0) && (gtf->tf[2] == 0)
	    && (gtf->tf[3] == 0) && (gtf->tf[4] == 0) && (gtf->tf[5] == 0)
	    && (gtf->tf[6] == 0))
		return 0;

	ata_acpi_gtf_to_tf(dev, gtf, &tf);
	if (prev_gtf) {
		ata_acpi_gtf_to_tf(dev, prev_gtf, &ptf);
		pptf = &ptf;
	}

	if (!ata_acpi_filter_tf(&tf, pptf)) {
		rtf = tf;
		err_mask = ata_exec_internal(dev, &rtf, NULL,
					     DMA_NONE, NULL, 0, 0);

		switch (err_mask) {
		case 0:
			level = KERN_DEBUG;
			snprintf(msg, sizeof(msg), "succeeded");
			rc = 1;
			break;

		case AC_ERR_DEV:
			level = KERN_INFO;
			snprintf(msg, sizeof(msg),
				 "rejected by device (Stat=0x%02x Err=0x%02x)",
				 rtf.command, rtf.feature);
			rc = 0;
			break;

		default:
			level = KERN_ERR;
			snprintf(msg, sizeof(msg),
				 "failed (Emask=0x%x Stat=0x%02x Err=0x%02x)",
				 err_mask, rtf.command, rtf.feature);
			rc = -EIO;
			break;
		}
	} else {
		level = KERN_INFO;
		snprintf(msg, sizeof(msg), "filtered out");
		rc = 0;
	}

	ata_dev_printk(dev, level,
		       "ACPI cmd %02x/%02x:%02x:%02x:%02x:%02x:%02x %s\n",
		       tf.command, tf.feature, tf.nsect, tf.lbal,
		       tf.lbam, tf.lbah, tf.device, msg);

	return rc;
}

/**
 * ata_acpi_exec_tfs - get then write drive taskfile settings
 * @dev: target ATA device
 * @nr_executed: out paramter for the number of executed commands
 *
 * Evaluate _GTF and excute returned taskfiles.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * Number of executed taskfiles on success, 0 if _GTF doesn't exist.
 * -errno on other errors.
 */
static int ata_acpi_exec_tfs(struct ata_device *dev, int *nr_executed)
{
	struct ata_acpi_gtf *gtf = NULL, *pgtf = NULL;
	int gtf_count, i, rc;

	/* get taskfiles */
	rc = ata_dev_get_GTF(dev, &gtf);
	if (rc < 0)
		return rc;
	gtf_count = rc;

	/* execute them */
	for (i = 0; i < gtf_count; i++, gtf++) {
		rc = ata_acpi_run_tf(dev, gtf, pgtf);
		if (rc < 0)
			break;
		if (rc) {
			(*nr_executed)++;
			pgtf = gtf;
		}
	}

	ata_acpi_clear_gtf(dev);

	if (rc < 0)
		return rc;
	return 0;
}

/**
 * ata_acpi_push_id - send Identify data to drive
 * @dev: target ATA device
 *
 * _SDD ACPI object: for SATA mode only
 * Must be after Identify (Packet) Device -- uses its data
 * ATM this function never returns a failure.  It is an optional
 * method and if it fails for whatever reason, we should still
 * just keep going.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
static int ata_acpi_push_id(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	int err;
	acpi_status status;
	struct acpi_object_list input;
	union acpi_object in_params[1];

	if (ata_msg_probe(ap))
		ata_dev_printk(dev, KERN_DEBUG, "%s: ix = %d, port#: %d\n",
			       __FUNCTION__, dev->devno, ap->port_no);

	/* Give the drive Identify data to the drive via the _SDD method */
	/* _SDD: set up input parameters */
	input.count = 1;
	input.pointer = in_params;
	in_params[0].type = ACPI_TYPE_BUFFER;
	in_params[0].buffer.length = sizeof(dev->id[0]) * ATA_ID_WORDS;
	in_params[0].buffer.pointer = (u8 *)dev->id;
	/* Output buffer: _SDD has no output */

	/* It's OK for _SDD to be missing too. */
	swap_buf_le16(dev->id, ATA_ID_WORDS);
	status = acpi_evaluate_object(dev->acpi_handle, "_SDD", &input, NULL);
	swap_buf_le16(dev->id, ATA_ID_WORDS);

	err = ACPI_FAILURE(status) ? -EIO : 0;
	if (err < 0)
		ata_dev_printk(dev, KERN_WARNING,
			       "ACPI _SDD failed (AE 0x%x)\n", status);

	return err;
}

/**
 * ata_acpi_on_suspend - ATA ACPI hook called on suspend
 * @ap: target ATA port
 *
 * This function is called when @ap is about to be suspended.  All
 * devices are already put to sleep but the port_suspend() callback
 * hasn't been executed yet.  Error return from this function aborts
 * suspend.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int ata_acpi_on_suspend(struct ata_port *ap)
{
	/* nada */
	return 0;
}

/**
 * ata_acpi_on_resume - ATA ACPI hook called on resume
 * @ap: target ATA port
 *
 * This function is called when @ap is resumed - right after port
 * itself is resumed but before any EH action is taken.
 *
 * LOCKING:
 * EH context.
 */
void ata_acpi_on_resume(struct ata_port *ap)
{
	const struct ata_acpi_gtm *gtm = ata_acpi_init_gtm(ap);
	struct ata_device *dev;

	if (ap->acpi_handle && gtm) {
		/* _GTM valid */

		/* restore timing parameters */
		ata_acpi_stm(ap, gtm);

		/* _GTF should immediately follow _STM so that it can
		 * use values set by _STM.  Cache _GTF result and
		 * schedule _GTF.
		 */
		ata_link_for_each_dev(dev, &ap->link) {
			ata_acpi_clear_gtf(dev);
			if (ata_dev_get_GTF(dev, NULL) >= 0)
				dev->flags |= ATA_DFLAG_ACPI_PENDING;
		}
	} else {
		/* SATA _GTF needs to be evaulated after _SDD and
		 * there's no reason to evaluate IDE _GTF early
		 * without _STM.  Clear cache and schedule _GTF.
		 */
		ata_link_for_each_dev(dev, &ap->link) {
			ata_acpi_clear_gtf(dev);
			dev->flags |= ATA_DFLAG_ACPI_PENDING;
		}
	}
}

/**
 * ata_acpi_on_devcfg - ATA ACPI hook called on device donfiguration
 * @dev: target ATA device
 *
 * This function is called when @dev is about to be configured.
 * IDENTIFY data might have been modified after this hook is run.
 *
 * LOCKING:
 * EH context.
 *
 * RETURNS:
 * Positive number if IDENTIFY data needs to be refreshed, 0 if not,
 * -errno on failure.
 */
int ata_acpi_on_devcfg(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_eh_context *ehc = &ap->link.eh_context;
	int acpi_sata = ap->flags & ATA_FLAG_ACPI_SATA;
	int nr_executed = 0;
	int rc;

	if (!dev->acpi_handle)
		return 0;

	/* do we need to do _GTF? */
	if (!(dev->flags & ATA_DFLAG_ACPI_PENDING) &&
	    !(acpi_sata && (ehc->i.flags & ATA_EHI_DID_HARDRESET)))
		return 0;

	/* do _SDD if SATA */
	if (acpi_sata) {
		rc = ata_acpi_push_id(dev);
		if (rc)
			goto acpi_err;
	}

	/* do _GTF */
	rc = ata_acpi_exec_tfs(dev, &nr_executed);
	if (rc)
		goto acpi_err;

	dev->flags &= ~ATA_DFLAG_ACPI_PENDING;

	/* refresh IDENTIFY page if any _GTF command has been executed */
	if (nr_executed) {
		rc = ata_dev_reread_id(dev, 0);
		if (rc < 0) {
			ata_dev_printk(dev, KERN_ERR, "failed to IDENTIFY "
				       "after ACPI commands\n");
			return rc;
		}
	}

	return 0;

 acpi_err:
	/* ignore evaluation failure if we can continue safely */
	if (rc == -EINVAL && !nr_executed && !(ap->pflags & ATA_PFLAG_FROZEN))
		return 0;

	/* fail and let EH retry once more for unknown IO errors */
	if (!(dev->flags & ATA_DFLAG_ACPI_FAILED)) {
		dev->flags |= ATA_DFLAG_ACPI_FAILED;
		return rc;
	}

	ata_dev_printk(dev, KERN_WARNING,
		       "ACPI: failed the second time, disabled\n");
	dev->acpi_handle = NULL;

	/* We can safely continue if no _GTF command has been executed
	 * and port is not frozen.
	 */
	if (!nr_executed && !(ap->pflags & ATA_PFLAG_FROZEN))
		return 0;

	return rc;
}

/**
 * ata_acpi_on_disable - ATA ACPI hook called when a device is disabled
 * @dev: target ATA device
 *
 * This function is called when @dev is about to be disabled.
 *
 * LOCKING:
 * EH context.
 */
void ata_acpi_on_disable(struct ata_device *dev)
{
	ata_acpi_clear_gtf(dev);
}

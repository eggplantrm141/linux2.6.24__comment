/*
 *  ec.c - ACPI Embedded Controller Driver (v2.0)
 *
 *  Copyright (C) 2006, 2007 Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>
 *  Copyright (C) 2006 Denis Sadykov <denis.m.sadykov@intel.com>
 *  Copyright (C) 2004 Luming Yu <luming.yu@intel.com>
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/* Uncomment next line to get verbose print outs*/
/* #define DEBUG */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <asm/io.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <acpi/actypes.h>

#define ACPI_EC_CLASS			"embedded_controller"
#define ACPI_EC_DEVICE_NAME		"Embedded Controller"
#define ACPI_EC_FILE_INFO		"info"

#undef PREFIX
#define PREFIX				"ACPI: EC: "

/* EC status register */
#define ACPI_EC_FLAG_OBF	0x01	/* Output buffer full */
#define ACPI_EC_FLAG_IBF	0x02	/* Input buffer full */
#define ACPI_EC_FLAG_BURST	0x10	/* burst mode */
#define ACPI_EC_FLAG_SCI	0x20	/* EC-SCI occurred */

/* EC commands */
enum ec_command {
	ACPI_EC_COMMAND_READ = 0x80,
	ACPI_EC_COMMAND_WRITE = 0x81,
	ACPI_EC_BURST_ENABLE = 0x82,
	ACPI_EC_BURST_DISABLE = 0x83,
	ACPI_EC_COMMAND_QUERY = 0x84,
};

/* EC events */
enum ec_event {
	ACPI_EC_EVENT_OBF_1 = 1,	/* Output buffer full */
	ACPI_EC_EVENT_IBF_0,		/* Input buffer empty */
};

#define ACPI_EC_DELAY		500	/* Wait 500ms max. during EC ops */
#define ACPI_EC_UDELAY_GLK	1000	/* Wait 1ms max. to get global lock */

enum {
	EC_FLAGS_WAIT_GPE = 0,		/* Don't check status until GPE arrives */
	EC_FLAGS_QUERY_PENDING,		/* Query is pending */
	EC_FLAGS_GPE_MODE,		/* Expect GPE to be sent for status change */
	EC_FLAGS_NO_ADDRESS_GPE,	/* Expect GPE only for non-address event */
	EC_FLAGS_ADDRESS,		/* Address is being written */
	EC_FLAGS_NO_WDATA_GPE,		/* Don't expect WDATA GPE event */
	EC_FLAGS_WDATA,			/* Data is being written */
	EC_FLAGS_NO_OBF1_GPE,		/* Don't expect GPE before read */
};

static int acpi_ec_remove(struct acpi_device *device, int type);
static int acpi_ec_start(struct acpi_device *device);
static int acpi_ec_stop(struct acpi_device *device, int type);
static int acpi_ec_add(struct acpi_device *device);

static const struct acpi_device_id ec_device_ids[] = {
	{"PNP0C09", 0},
	{"", 0},
};

static struct acpi_driver acpi_ec_driver = {
	.name = "ec",
	.class = ACPI_EC_CLASS,
	.ids = ec_device_ids,
	.ops = {
		.add = acpi_ec_add,
		.remove = acpi_ec_remove,
		.start = acpi_ec_start,
		.stop = acpi_ec_stop,
		},
};

/* If we find an EC via the ECDT, we need to keep a ptr to its context */
/* External interfaces use first EC only, so remember */
typedef int (*acpi_ec_query_func) (void *data);

struct acpi_ec_query_handler {
	struct list_head node;
	acpi_ec_query_func func;
	acpi_handle handle;
	void *data;
	u8 query_bit;
};

static struct acpi_ec {
	acpi_handle handle;
	unsigned long gpe;
	unsigned long command_addr;
	unsigned long data_addr;
	unsigned long global_lock;
	unsigned long flags;
	struct mutex lock;
	wait_queue_head_t wait;
	struct list_head list;
	u8 handlers_installed;
} *boot_ec, *first_ec;

/* --------------------------------------------------------------------------
                             Transaction Management
   -------------------------------------------------------------------------- */

static inline u8 acpi_ec_read_status(struct acpi_ec *ec)
{
	u8 x = inb(ec->command_addr);
	pr_debug(PREFIX "---> status = 0x%2.2x\n", x);
	return x;
}

static inline u8 acpi_ec_read_data(struct acpi_ec *ec)
{
	u8 x = inb(ec->data_addr);
	pr_debug(PREFIX "---> data = 0x%2.2x\n", x);
	return inb(ec->data_addr);
}

static inline void acpi_ec_write_cmd(struct acpi_ec *ec, u8 command)
{
	pr_debug(PREFIX "<--- command = 0x%2.2x\n", command);
	outb(command, ec->command_addr);
}

static inline void acpi_ec_write_data(struct acpi_ec *ec, u8 data)
{
	pr_debug(PREFIX "<--- data = 0x%2.2x\n", data);
	outb(data, ec->data_addr);
}

static inline int acpi_ec_check_status(struct acpi_ec *ec, enum ec_event event)
{
	if (test_bit(EC_FLAGS_WAIT_GPE, &ec->flags))
		return 0;
	if (event == ACPI_EC_EVENT_OBF_1) {
		if (acpi_ec_read_status(ec) & ACPI_EC_FLAG_OBF)
			return 1;
	} else if (event == ACPI_EC_EVENT_IBF_0) {
		if (!(acpi_ec_read_status(ec) & ACPI_EC_FLAG_IBF))
			return 1;
	}

	return 0;
}

static int acpi_ec_wait(struct acpi_ec *ec, enum ec_event event, int force_poll)
{
	int ret = 0;

	if (unlikely(event == ACPI_EC_EVENT_OBF_1 &&
		     test_bit(EC_FLAGS_NO_OBF1_GPE, &ec->flags)))
		force_poll = 1;
	if (unlikely(test_bit(EC_FLAGS_ADDRESS, &ec->flags) &&
		     test_bit(EC_FLAGS_NO_ADDRESS_GPE, &ec->flags)))
		force_poll = 1;
	if (unlikely(test_bit(EC_FLAGS_WDATA, &ec->flags) &&
		     test_bit(EC_FLAGS_NO_WDATA_GPE, &ec->flags)))
		force_poll = 1;
	if (likely(test_bit(EC_FLAGS_GPE_MODE, &ec->flags)) &&
	    likely(!force_poll)) {
		if (wait_event_timeout(ec->wait, acpi_ec_check_status(ec, event),
				       msecs_to_jiffies(ACPI_EC_DELAY)))
			goto end;
		clear_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
		if (acpi_ec_check_status(ec, event)) {
			if (event == ACPI_EC_EVENT_OBF_1) {
				/* miss OBF_1 GPE, don't expect it */
				pr_info(PREFIX "missing OBF confirmation, "
					"don't expect it any longer.\n");
				set_bit(EC_FLAGS_NO_OBF1_GPE, &ec->flags);
			} else if (test_bit(EC_FLAGS_ADDRESS, &ec->flags)) {
				/* miss address GPE, don't expect it anymore */
				pr_info(PREFIX "missing address confirmation, "
					"don't expect it any longer.\n");
				set_bit(EC_FLAGS_NO_ADDRESS_GPE, &ec->flags);
			} else if (test_bit(EC_FLAGS_WDATA, &ec->flags)) {
				/* miss write data GPE, don't expect it */
				pr_info(PREFIX "missing write data confirmation, "
					"don't expect it any longer.\n");
				set_bit(EC_FLAGS_NO_WDATA_GPE, &ec->flags);
			} else {
				/* missing GPEs, switch back to poll mode */
				if (printk_ratelimit())
					pr_info(PREFIX "missing confirmations, "
						"switch off interrupt mode.\n");
				clear_bit(EC_FLAGS_GPE_MODE, &ec->flags);
			}
			goto end;
		}
	} else {
		unsigned long delay = jiffies + msecs_to_jiffies(ACPI_EC_DELAY);
		clear_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
		while (time_before(jiffies, delay)) {
			if (acpi_ec_check_status(ec, event))
				goto end;
		}
	}
	pr_err(PREFIX "acpi_ec_wait timeout,"
			       " status = %d, expect_event = %d\n",
			       acpi_ec_read_status(ec), event);
	ret = -ETIME;
      end:
	clear_bit(EC_FLAGS_ADDRESS, &ec->flags);
	return ret;
}

static int acpi_ec_transaction_unlocked(struct acpi_ec *ec, u8 command,
					const u8 * wdata, unsigned wdata_len,
					u8 * rdata, unsigned rdata_len,
					int force_poll)
{
	int result = 0;
	set_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
	acpi_ec_write_cmd(ec, command);
	pr_debug(PREFIX "transaction start\n");
	for (; wdata_len > 0; --wdata_len) {
		result = acpi_ec_wait(ec, ACPI_EC_EVENT_IBF_0, force_poll);
		if (result) {
			pr_err(PREFIX
			       "write_cmd timeout, command = %d\n", command);
			goto end;
		}
		/* mark the address byte written to EC */
		if (rdata_len + wdata_len > 1)
			set_bit(EC_FLAGS_ADDRESS, &ec->flags);
		set_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
		acpi_ec_write_data(ec, *(wdata++));
	}

	if (!rdata_len) {
		set_bit(EC_FLAGS_WDATA, &ec->flags);
		result = acpi_ec_wait(ec, ACPI_EC_EVENT_IBF_0, force_poll);
		if (result) {
			pr_err(PREFIX
			       "finish-write timeout, command = %d\n", command);
			goto end;
		}
	} else if (command == ACPI_EC_COMMAND_QUERY)
		clear_bit(EC_FLAGS_QUERY_PENDING, &ec->flags);

	for (; rdata_len > 0; --rdata_len) {
		result = acpi_ec_wait(ec, ACPI_EC_EVENT_OBF_1, force_poll);
		if (result) {
			pr_err(PREFIX "read timeout, command = %d\n", command);
			goto end;
		}
		/* Don't expect GPE after last read */
		if (rdata_len > 1)
			set_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
		*(rdata++) = acpi_ec_read_data(ec);
	}
      end:
	pr_debug(PREFIX "transaction end\n");
	return result;
}

static int acpi_ec_transaction(struct acpi_ec *ec, u8 command,
			       const u8 * wdata, unsigned wdata_len,
			       u8 * rdata, unsigned rdata_len,
			       int force_poll)
{
	int status;
	u32 glk;

	if (!ec || (wdata_len && !wdata) || (rdata_len && !rdata))
		return -EINVAL;

	if (rdata)
		memset(rdata, 0, rdata_len);

	mutex_lock(&ec->lock);
	if (ec->global_lock) {
		status = acpi_acquire_global_lock(ACPI_EC_UDELAY_GLK, &glk);
		if (ACPI_FAILURE(status)) {
			mutex_unlock(&ec->lock);
			return -ENODEV;
		}
	}

	status = acpi_ec_wait(ec, ACPI_EC_EVENT_IBF_0, 0);
	if (status) {
		pr_err(PREFIX "input buffer is not empty, "
				"aborting transaction\n");
		goto end;
	}

	status = acpi_ec_transaction_unlocked(ec, command,
					      wdata, wdata_len,
					      rdata, rdata_len,
					      force_poll);

      end:

	if (ec->global_lock)
		acpi_release_global_lock(glk);
	mutex_unlock(&ec->lock);

	return status;
}

/*
 * Note: samsung nv5000 doesn't work with ec burst mode.
 * http://bugzilla.kernel.org/show_bug.cgi?id=4980
 */
int acpi_ec_burst_enable(struct acpi_ec *ec)
{
	u8 d;
	return acpi_ec_transaction(ec, ACPI_EC_BURST_ENABLE, NULL, 0, &d, 1, 0);
}

int acpi_ec_burst_disable(struct acpi_ec *ec)
{
	return acpi_ec_transaction(ec, ACPI_EC_BURST_DISABLE, NULL, 0, NULL, 0, 0);
}

static int acpi_ec_read(struct acpi_ec *ec, u8 address, u8 * data)
{
	int result;
	u8 d;

	result = acpi_ec_transaction(ec, ACPI_EC_COMMAND_READ,
				     &address, 1, &d, 1, 0);
	*data = d;
	return result;
}

static int acpi_ec_write(struct acpi_ec *ec, u8 address, u8 data)
{
	u8 wdata[2] = { address, data };
	return acpi_ec_transaction(ec, ACPI_EC_COMMAND_WRITE,
				   wdata, 2, NULL, 0, 0);
}

/*
 * Externally callable EC access functions. For now, assume 1 EC only
 */
int ec_burst_enable(void)
{
	if (!first_ec)
		return -ENODEV;
	return acpi_ec_burst_enable(first_ec);
}

EXPORT_SYMBOL(ec_burst_enable);

int ec_burst_disable(void)
{
	if (!first_ec)
		return -ENODEV;
	return acpi_ec_burst_disable(first_ec);
}

EXPORT_SYMBOL(ec_burst_disable);

int ec_read(u8 addr, u8 * val)
{
	int err;
	u8 temp_data;

	if (!first_ec)
		return -ENODEV;

	err = acpi_ec_read(first_ec, addr, &temp_data);

	if (!err) {
		*val = temp_data;
		return 0;
	} else
		return err;
}

EXPORT_SYMBOL(ec_read);

int ec_write(u8 addr, u8 val)
{
	int err;

	if (!first_ec)
		return -ENODEV;

	err = acpi_ec_write(first_ec, addr, val);

	return err;
}

EXPORT_SYMBOL(ec_write);

int ec_transaction(u8 command,
		   const u8 * wdata, unsigned wdata_len,
		   u8 * rdata, unsigned rdata_len,
		   int force_poll)
{
	if (!first_ec)
		return -ENODEV;

	return acpi_ec_transaction(first_ec, command, wdata,
				   wdata_len, rdata, rdata_len,
				   force_poll);
}

EXPORT_SYMBOL(ec_transaction);

static int acpi_ec_query(struct acpi_ec *ec, u8 * data)
{
	int result;
	u8 d;

	if (!ec || !data)
		return -EINVAL;

	/*
	 * Query the EC to find out which _Qxx method we need to evaluate.
	 * Note that successful completion of the query causes the ACPI_EC_SCI
	 * bit to be cleared (and thus clearing the interrupt source).
	 */

	result = acpi_ec_transaction(ec, ACPI_EC_COMMAND_QUERY, NULL, 0, &d, 1, 0);
	if (result)
		return result;

	if (!d)
		return -ENODATA;

	*data = d;
	return 0;
}

/* --------------------------------------------------------------------------
                                Event Management
   -------------------------------------------------------------------------- */
int acpi_ec_add_query_handler(struct acpi_ec *ec, u8 query_bit,
			      acpi_handle handle, acpi_ec_query_func func,
			      void *data)
{
	struct acpi_ec_query_handler *handler =
	    kzalloc(sizeof(struct acpi_ec_query_handler), GFP_KERNEL);
	if (!handler)
		return -ENOMEM;

	handler->query_bit = query_bit;
	handler->handle = handle;
	handler->func = func;
	handler->data = data;
	mutex_lock(&ec->lock);
	list_add(&handler->node, &ec->list);
	mutex_unlock(&ec->lock);
	return 0;
}

EXPORT_SYMBOL_GPL(acpi_ec_add_query_handler);

void acpi_ec_remove_query_handler(struct acpi_ec *ec, u8 query_bit)
{
	struct acpi_ec_query_handler *handler, *tmp;
	mutex_lock(&ec->lock);
	list_for_each_entry_safe(handler, tmp, &ec->list, node) {
		if (query_bit == handler->query_bit) {
			list_del(&handler->node);
			kfree(handler);
		}
	}
	mutex_unlock(&ec->lock);
}

EXPORT_SYMBOL_GPL(acpi_ec_remove_query_handler);

static void acpi_ec_gpe_query(void *ec_cxt)
{
	struct acpi_ec *ec = ec_cxt;
	u8 value = 0;
	struct acpi_ec_query_handler *handler, copy;

	if (!ec || acpi_ec_query(ec, &value))
		return;
	mutex_lock(&ec->lock);
	list_for_each_entry(handler, &ec->list, node) {
		if (value == handler->query_bit) {
			/* have custom handler for this bit */
			memcpy(&copy, handler, sizeof(copy));
			mutex_unlock(&ec->lock);
			if (copy.func) {
				copy.func(copy.data);
			} else if (copy.handle) {
				acpi_evaluate_object(copy.handle, NULL, NULL, NULL);
			}
			return;
		}
	}
	mutex_unlock(&ec->lock);
}

static u32 acpi_ec_gpe_handler(void *data)
{
	acpi_status status = AE_OK;
	struct acpi_ec *ec = data;

	pr_debug(PREFIX "~~~> interrupt\n");
	clear_bit(EC_FLAGS_WAIT_GPE, &ec->flags);
	if (test_bit(EC_FLAGS_GPE_MODE, &ec->flags))
		wake_up(&ec->wait);

	if (acpi_ec_read_status(ec) & ACPI_EC_FLAG_SCI) {
		if (!test_and_set_bit(EC_FLAGS_QUERY_PENDING, &ec->flags))
			status = acpi_os_execute(OSL_EC_BURST_HANDLER,
				acpi_ec_gpe_query, ec);
	} else if (unlikely(!test_bit(EC_FLAGS_GPE_MODE, &ec->flags))) {
		/* this is non-query, must be confirmation */
		if (printk_ratelimit())
			pr_info(PREFIX "non-query interrupt received,"
				" switching to interrupt mode\n");
		set_bit(EC_FLAGS_GPE_MODE, &ec->flags);
	}

	return ACPI_SUCCESS(status) ?
	    ACPI_INTERRUPT_HANDLED : ACPI_INTERRUPT_NOT_HANDLED;
}

/* --------------------------------------------------------------------------
                             Address Space Management
   -------------------------------------------------------------------------- */

static acpi_status
acpi_ec_space_setup(acpi_handle region_handle,
		    u32 function, void *handler_context, void **return_context)
{
	/*
	 * The EC object is in the handler context and is needed
	 * when calling the acpi_ec_space_handler.
	 */
	*return_context = (function != ACPI_REGION_DEACTIVATE) ?
	    handler_context : NULL;

	return AE_OK;
}

static acpi_status
acpi_ec_space_handler(u32 function, acpi_physical_address address,
		      u32 bits, acpi_integer *value,
		      void *handler_context, void *region_context)
{
	struct acpi_ec *ec = handler_context;
	int result = 0, i = 0;
	u8 temp = 0;

	if ((address > 0xFF) || !value || !handler_context)
		return AE_BAD_PARAMETER;

	if (function != ACPI_READ && function != ACPI_WRITE)
		return AE_BAD_PARAMETER;

	if (bits != 8 && acpi_strict)
		return AE_BAD_PARAMETER;

	while (bits - i > 0) {
		if (function == ACPI_READ) {
			result = acpi_ec_read(ec, address, &temp);
			(*value) |= ((acpi_integer)temp) << i;
		} else {
			temp = 0xff & ((*value) >> i);
			result = acpi_ec_write(ec, address, temp);
		}
		i += 8;
		++address;
	}

	switch (result) {
	case -EINVAL:
		return AE_BAD_PARAMETER;
		break;
	case -ENODEV:
		return AE_NOT_FOUND;
		break;
	case -ETIME:
		return AE_TIME;
		break;
	default:
		return AE_OK;
	}
}

/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

static struct proc_dir_entry *acpi_ec_dir;

static int acpi_ec_read_info(struct seq_file *seq, void *offset)
{
	struct acpi_ec *ec = seq->private;

	if (!ec)
		goto end;

	seq_printf(seq, "gpe:\t\t\t0x%02x\n", (u32) ec->gpe);
	seq_printf(seq, "ports:\t\t\t0x%02x, 0x%02x\n",
		   (unsigned)ec->command_addr, (unsigned)ec->data_addr);
	seq_printf(seq, "use global lock:\t%s\n",
		   ec->global_lock ? "yes" : "no");
      end:
	return 0;
}

static int acpi_ec_info_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, acpi_ec_read_info, PDE(inode)->data);
}

static struct file_operations acpi_ec_info_ops = {
	.open = acpi_ec_info_open_fs,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static int acpi_ec_add_fs(struct acpi_device *device)
{
	struct proc_dir_entry *entry = NULL;

	if (!acpi_device_dir(device)) {
		acpi_device_dir(device) = proc_mkdir(acpi_device_bid(device),
						     acpi_ec_dir);
		if (!acpi_device_dir(device))
			return -ENODEV;
	}

	entry = create_proc_entry(ACPI_EC_FILE_INFO, S_IRUGO,
				  acpi_device_dir(device));
	if (!entry)
		return -ENODEV;
	else {
		entry->proc_fops = &acpi_ec_info_ops;
		entry->data = acpi_driver_data(device);
		entry->owner = THIS_MODULE;
	}

	return 0;
}

static int acpi_ec_remove_fs(struct acpi_device *device)
{

	if (acpi_device_dir(device)) {
		remove_proc_entry(ACPI_EC_FILE_INFO, acpi_device_dir(device));
		remove_proc_entry(acpi_device_bid(device), acpi_ec_dir);
		acpi_device_dir(device) = NULL;
	}

	return 0;
}

/* --------------------------------------------------------------------------
                               Driver Interface
   -------------------------------------------------------------------------- */
static acpi_status
ec_parse_io_ports(struct acpi_resource *resource, void *context);

static struct acpi_ec *make_acpi_ec(void)
{
	struct acpi_ec *ec = kzalloc(sizeof(struct acpi_ec), GFP_KERNEL);
	if (!ec)
		return NULL;
	ec->flags = 1 << EC_FLAGS_QUERY_PENDING;
	mutex_init(&ec->lock);
	init_waitqueue_head(&ec->wait);
	INIT_LIST_HEAD(&ec->list);
	return ec;
}

static acpi_status
acpi_ec_register_query_methods(acpi_handle handle, u32 level,
			       void *context, void **return_value)
{
	struct acpi_namespace_node *node = handle;
	struct acpi_ec *ec = context;
	int value = 0;
	if (sscanf(node->name.ascii, "_Q%x", &value) == 1) {
		acpi_ec_add_query_handler(ec, value, handle, NULL, NULL);
	}
	return AE_OK;
}

static acpi_status
ec_parse_device(acpi_handle handle, u32 Level, void *context, void **retval)
{
	acpi_status status;

	struct acpi_ec *ec = context;
	status = acpi_walk_resources(handle, METHOD_NAME__CRS,
				     ec_parse_io_ports, ec);
	if (ACPI_FAILURE(status))
		return status;

	/* Get GPE bit assignment (EC events). */
	/* TODO: Add support for _GPE returning a package */
	status = acpi_evaluate_integer(handle, "_GPE", NULL, &ec->gpe);
	if (ACPI_FAILURE(status))
		return status;
	/* Find and register all query methods */
	acpi_walk_namespace(ACPI_TYPE_METHOD, handle, 1,
			    acpi_ec_register_query_methods, ec, NULL);
	/* Use the global lock for all EC transactions? */
	acpi_evaluate_integer(handle, "_GLK", NULL, &ec->global_lock);
	ec->handle = handle;
	return AE_CTRL_TERMINATE;
}

static void ec_remove_handlers(struct acpi_ec *ec)
{
	if (ACPI_FAILURE(acpi_remove_address_space_handler(ec->handle,
				ACPI_ADR_SPACE_EC, &acpi_ec_space_handler)))
		pr_err(PREFIX "failed to remove space handler\n");
	if (ACPI_FAILURE(acpi_remove_gpe_handler(NULL, ec->gpe,
				&acpi_ec_gpe_handler)))
		pr_err(PREFIX "failed to remove gpe handler\n");
	ec->handlers_installed = 0;
}

static int acpi_ec_add(struct acpi_device *device)
{
	struct acpi_ec *ec = NULL;

	if (!device)
		return -EINVAL;
	strcpy(acpi_device_name(device), ACPI_EC_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_EC_CLASS);

	/* Check for boot EC */
	if (boot_ec) {
		if (boot_ec->handle == device->handle) {
			/* Pre-loaded EC from DSDT, just move pointer */
			ec = boot_ec;
			boot_ec = NULL;
			goto end;
		} else if (boot_ec->handle == ACPI_ROOT_OBJECT) {
			/* ECDT-based EC, time to shut it down */
			ec_remove_handlers(boot_ec);
			kfree(boot_ec);
			first_ec = boot_ec = NULL;
		}
	}

	ec = make_acpi_ec();
	if (!ec)
		return -ENOMEM;

	if (ec_parse_device(device->handle, 0, ec, NULL) !=
	    AE_CTRL_TERMINATE) {
		kfree(ec);
		return -EINVAL;
	}
	ec->handle = device->handle;
      end:
	if (!first_ec)
		first_ec = ec;
	acpi_driver_data(device) = ec;
	acpi_ec_add_fs(device);
	pr_info(PREFIX "GPE = 0x%lx, I/O: command/status = 0x%lx, data = 0x%lx\n",
			  ec->gpe, ec->command_addr, ec->data_addr);
	pr_info(PREFIX "driver started in %s mode\n",
		(test_bit(EC_FLAGS_GPE_MODE, &ec->flags))?"interrupt":"poll");
	return 0;
}

static int acpi_ec_remove(struct acpi_device *device, int type)
{
	struct acpi_ec *ec;
	struct acpi_ec_query_handler *handler, *tmp;

	if (!device)
		return -EINVAL;

	ec = acpi_driver_data(device);
	mutex_lock(&ec->lock);
	list_for_each_entry_safe(handler, tmp, &ec->list, node) {
		list_del(&handler->node);
		kfree(handler);
	}
	mutex_unlock(&ec->lock);
	acpi_ec_remove_fs(device);
	acpi_driver_data(device) = NULL;
	if (ec == first_ec)
		first_ec = NULL;
	kfree(ec);
	return 0;
}

static acpi_status
ec_parse_io_ports(struct acpi_resource *resource, void *context)
{
	struct acpi_ec *ec = context;

	if (resource->type != ACPI_RESOURCE_TYPE_IO)
		return AE_OK;

	/*
	 * The first address region returned is the data port, and
	 * the second address region returned is the status/command
	 * port.
	 */
	if (ec->data_addr == 0)
		ec->data_addr = resource->data.io.minimum;
	else if (ec->command_addr == 0)
		ec->command_addr = resource->data.io.minimum;
	else
		return AE_CTRL_TERMINATE;

	return AE_OK;
}

static int ec_install_handlers(struct acpi_ec *ec)
{
	acpi_status status;
	if (ec->handlers_installed)
		return 0;
	status = acpi_install_gpe_handler(NULL, ec->gpe,
					  ACPI_GPE_EDGE_TRIGGERED,
					  &acpi_ec_gpe_handler, ec);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	acpi_set_gpe_type(NULL, ec->gpe, ACPI_GPE_TYPE_RUNTIME);
	acpi_enable_gpe(NULL, ec->gpe, ACPI_NOT_ISR);

	status = acpi_install_address_space_handler(ec->handle,
						    ACPI_ADR_SPACE_EC,
						    &acpi_ec_space_handler,
						    &acpi_ec_space_setup, ec);
	if (ACPI_FAILURE(status)) {
		acpi_remove_gpe_handler(NULL, ec->gpe, &acpi_ec_gpe_handler);
		return -ENODEV;
	}

	ec->handlers_installed = 1;
	return 0;
}

static int acpi_ec_start(struct acpi_device *device)
{
	struct acpi_ec *ec;
	int ret = 0;

	if (!device)
		return -EINVAL;

	ec = acpi_driver_data(device);

	if (!ec)
		return -EINVAL;

	ret = ec_install_handlers(ec);

	/* EC is fully operational, allow queries */
	clear_bit(EC_FLAGS_QUERY_PENDING, &ec->flags);
	return ret;
}

static int acpi_ec_stop(struct acpi_device *device, int type)
{
	struct acpi_ec *ec;
	if (!device)
		return -EINVAL;
	ec = acpi_driver_data(device);
	if (!ec)
		return -EINVAL;
	ec_remove_handlers(ec);

	return 0;
}

int __init acpi_boot_ec_enable(void)
{
	if (!boot_ec || boot_ec->handlers_installed)
		return 0;
	if (!ec_install_handlers(boot_ec)) {
		first_ec = boot_ec;
		return 0;
	}
	return -EFAULT;
}

int __init acpi_ec_ecdt_probe(void)
{
	int ret;
	acpi_status status;
	struct acpi_table_ecdt *ecdt_ptr;

	boot_ec = make_acpi_ec();
	if (!boot_ec)
		return -ENOMEM;
	/*
	 * Generate a boot ec context
	 */
	status = acpi_get_table(ACPI_SIG_ECDT, 1,
				(struct acpi_table_header **)&ecdt_ptr);
	if (ACPI_SUCCESS(status)) {
		pr_info(PREFIX "EC description table is found, configuring boot EC\n");
		boot_ec->command_addr = ecdt_ptr->control.address;
		boot_ec->data_addr = ecdt_ptr->data.address;
		boot_ec->gpe = ecdt_ptr->gpe;
		boot_ec->handle = ACPI_ROOT_OBJECT;
	} else {
		/* This workaround is needed only on some broken machines,
		 * which require early EC, but fail to provide ECDT */
		acpi_handle x;
		printk(KERN_DEBUG PREFIX "Look up EC in DSDT\n");
		status = acpi_get_devices(ec_device_ids[0].id, ec_parse_device,
						boot_ec, NULL);
		/* Check that acpi_get_devices actually find something */
		if (ACPI_FAILURE(status) || !boot_ec->handle)
			goto error;
		/* We really need to limit this workaround, the only ASUS,
		 * which needs it, has fake EC._INI method, so use it as flag.
		 * Keep boot_ec struct as it will be needed soon.
		 */
		if (ACPI_FAILURE(acpi_get_handle(boot_ec->handle, "_INI", &x)))
			return -ENODEV;
	}

	ret = ec_install_handlers(boot_ec);
	if (!ret) {
		first_ec = boot_ec;
		return 0;
	}
      error:
	kfree(boot_ec);
	boot_ec = NULL;
	return -ENODEV;
}

static int __init acpi_ec_init(void)
{
	int result = 0;

	if (acpi_disabled)
		return 0;

	acpi_ec_dir = proc_mkdir(ACPI_EC_CLASS, acpi_root_dir);
	if (!acpi_ec_dir)
		return -ENODEV;

	/* Now register the driver for the EC */
	result = acpi_bus_register_driver(&acpi_ec_driver);
	if (result < 0) {
		remove_proc_entry(ACPI_EC_CLASS, acpi_root_dir);
		return -ENODEV;
	}

	return result;
}

subsys_initcall(acpi_ec_init);

/* EC driver currently not unloadable */
#if 0
static void __exit acpi_ec_exit(void)
{

	acpi_bus_unregister_driver(&acpi_ec_driver);

	remove_proc_entry(ACPI_EC_CLASS, acpi_root_dir);

	return;
}
#endif	/* 0 */

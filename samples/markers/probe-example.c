/* probe-example.c
 *
 * Connects two functions to marker call sites.
 *
 * (C) Copyright 2007 Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 *
 * This file is released under the GPLv2.
 * See the file COPYING for more details.
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/marker.h>
#include <asm/atomic.h>

struct probe_data {
	const char *name;
	const char *format;
	marker_probe_func *probe_func;
};

void probe_subsystem_event(const struct marker *mdata, void *private,
	const char *format, ...)
{
	va_list ap;
	/* Declare args */
	unsigned int value;
	const char *mystr;

	/* Assign args */
	va_start(ap, format);
	value = va_arg(ap, typeof(value));
	mystr = va_arg(ap, typeof(mystr));

	/* Call printk */
	printk(KERN_DEBUG "Value %u, string %s\n", value, mystr);

	/* or count, check rights, serialize data in a buffer */

	va_end(ap);
}

atomic_t eventb_count = ATOMIC_INIT(0);

void probe_subsystem_eventb(const struct marker *mdata, void *private,
	const char *format, ...)
{
	/* Increment counter */
	atomic_inc(&eventb_count);
}

static struct probe_data probe_array[] =
{
	{	.name = "subsystem_event",
		.format = "integer %d string %s",
		.probe_func = probe_subsystem_event },
	{	.name = "subsystem_eventb",
		.format = MARK_NOARGS,
		.probe_func = probe_subsystem_eventb },
};

static int __init probe_init(void)
{
	int result;
	int i;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++) {
		result = marker_probe_register(probe_array[i].name,
				probe_array[i].format,
				probe_array[i].probe_func, &probe_array[i]);
		if (result)
			printk(KERN_INFO "Unable to register probe %s\n",
				probe_array[i].name);
		result = marker_arm(probe_array[i].name);
		if (result)
			printk(KERN_INFO "Unable to arm probe %s\n",
				probe_array[i].name);
	}
	return 0;
}

static void __exit probe_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(probe_array); i++)
		marker_probe_unregister(probe_array[i].name);
	printk(KERN_INFO "Number of event b : %u\n",
			atomic_read(&eventb_count));
}

module_init(probe_init);
module_exit(probe_fini);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("SUBSYSTEM Probe");

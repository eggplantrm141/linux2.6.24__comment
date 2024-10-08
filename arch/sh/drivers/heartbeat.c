/*
 * Generic heartbeat driver for regular LED banks
 *
 * Copyright (C) 2007  Paul Mundt
 *
 * Most SH reference boards include a number of individual LEDs that can
 * be independently controlled (either via a pre-defined hardware
 * function or via the LED class, if desired -- the hardware tends to
 * encapsulate some of the same "triggers" that the LED class supports,
 * so there's not too much value in it).
 *
 * Additionally, most of these boards also have a LED bank that we've
 * traditionally used for strobing the load average. This use case is
 * handled by this driver, rather than giving each LED bit position its
 * own struct device.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <asm/heartbeat.h>

#define DRV_NAME "heartbeat"
#define DRV_VERSION "0.1.1"

static unsigned char default_bit_pos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static inline void heartbeat_toggle_bit(struct heartbeat_data *hd,
					unsigned bit, unsigned int inverted)
{
	unsigned int new;

	new = (1 << hd->bit_pos[bit]);
	if (inverted)
		new = ~new;

	switch (hd->regsize) {
	case 32:
		iowrite32(new, hd->base);
		break;
	case 16:
		iowrite16(new, hd->base);
		break;
	default:
		iowrite8(new, hd->base);
		break;
	}
}

static void heartbeat_timer(unsigned long data)
{
	struct heartbeat_data *hd = (struct heartbeat_data *)data;
	static unsigned bit = 0, up = 1;

	heartbeat_toggle_bit(hd, bit, hd->flags & HEARTBEAT_INVERTED);

	bit += up;
	if ((bit == 0) || (bit == (hd->nr_bits)-1))
		up = -up;

	mod_timer(&hd->timer, jiffies + (110 - ((300 << FSHIFT) /
			((avenrun[0] / 5) + (3 << FSHIFT)))));
}

static int heartbeat_drv_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct heartbeat_data *hd;

	if (unlikely(pdev->num_resources != 1)) {
		dev_err(&pdev->dev, "invalid number of resources\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(res == NULL)) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	if (pdev->dev.platform_data) {
		hd = pdev->dev.platform_data;
	} else {
		hd = kzalloc(sizeof(struct heartbeat_data), GFP_KERNEL);
		if (unlikely(!hd))
			return -ENOMEM;
	}

	hd->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!unlikely(hd->base)) {
		dev_err(&pdev->dev, "ioremap failed\n");

		if (!pdev->dev.platform_data)
			kfree(hd);

		return -ENXIO;
	}

	if (!hd->nr_bits) {
		hd->bit_pos = default_bit_pos;
		hd->nr_bits = ARRAY_SIZE(default_bit_pos);
	}

	if (!hd->regsize)
		hd->regsize = 8;	/* default access size */

	setup_timer(&hd->timer, heartbeat_timer, (unsigned long)hd);
	platform_set_drvdata(pdev, hd);

	return mod_timer(&hd->timer, jiffies + 1);
}

static int heartbeat_drv_remove(struct platform_device *pdev)
{
	struct heartbeat_data *hd = platform_get_drvdata(pdev);

	del_timer_sync(&hd->timer);
	iounmap(hd->base);

	platform_set_drvdata(pdev, NULL);

	if (!pdev->dev.platform_data)
		kfree(hd);

	return 0;
}

static struct platform_driver heartbeat_driver = {
	.probe		= heartbeat_drv_probe,
	.remove		= heartbeat_drv_remove,
	.driver		= {
		.name	= DRV_NAME,
	},
};

static int __init heartbeat_init(void)
{
	printk(KERN_NOTICE DRV_NAME ": version %s loaded\n", DRV_VERSION);
	return platform_driver_register(&heartbeat_driver);
}

static void __exit heartbeat_exit(void)
{
	platform_driver_unregister(&heartbeat_driver);
}
module_init(heartbeat_init);
module_exit(heartbeat_exit);

MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Paul Mundt");
MODULE_LICENSE("GPLv2");

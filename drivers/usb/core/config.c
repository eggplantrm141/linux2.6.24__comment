#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm/byteorder.h>
#include "usb.h"
#include "hcd.h"

#define USB_MAXALTSETTING		128	/* Hard limit */
#define USB_MAXENDPOINTS		30	/* Hard limit */

#define USB_MAXCONFIG			8	/* Arbitrary limit */


static inline const char *plural(int n)
{
	return (n == 1 ? "" : "s");
}

/**
 * 查找下一个描述符。
 */
static int find_next_descriptor(unsigned char *buffer, int size,
    int dt1, int dt2, int *num_skipped)
{
	struct usb_descriptor_header *h;
	int n = 0;
	unsigned char *buffer0 = buffer;

	/* Find the next descriptor of type dt1 or dt2 */
	while (size > 0) {
		h = (struct usb_descriptor_header *) buffer;
		if (h->bDescriptorType == dt1 || h->bDescriptorType == dt2)
			break;
		buffer += h->bLength;
		size -= h->bLength;
		++n;
	}

	/* Store the number of descriptors skipped and return the
	 * number of bytes skipped */
	if (num_skipped)
		*num_skipped = n;
	return buffer - buffer0;
}

/**
 * 解析端点描述符。
 */
static int usb_parse_endpoint(struct device *ddev, int cfgno, int inum,
    int asnum, struct usb_host_interface *ifp, int num_ep,
    unsigned char *buffer, int size)
{
	unsigned char *buffer0 = buffer;
	struct usb_endpoint_descriptor *d;
	struct usb_host_endpoint *endpoint;
	int n, i, j;

	d = (struct usb_endpoint_descriptor *) buffer;
	buffer += d->bLength;
	size -= d->bLength;

	/**
	 * 两种不同类型的端点，其长度不太一样。
	 */
	if (d->bLength >= USB_DT_ENDPOINT_AUDIO_SIZE)
		n = USB_DT_ENDPOINT_AUDIO_SIZE;
	else if (d->bLength >= USB_DT_ENDPOINT_SIZE)
		n = USB_DT_ENDPOINT_SIZE;
	else {
		dev_warn(ddev, "config %d interface %d altsetting %d has an "
		    "invalid endpoint descriptor of length %d, skipping\n",
		    cfgno, inum, asnum, d->bLength);
		goto skip_to_next_endpoint_or_interface_descriptor;
	}

	i = d->bEndpointAddress & ~USB_ENDPOINT_DIR_MASK;
	/**
	 * 端点号不能为0，因为端点0比较特殊，没有描述符。
	 * 也不能大于等于16，因为最多只能有16个端点。
	 */
	if (i >= 16 || i == 0) {
		dev_warn(ddev, "config %d interface %d altsetting %d has an "
		    "invalid endpoint with address 0x%X, skipping\n",
		    cfgno, inum, asnum, d->bEndpointAddress);
		goto skip_to_next_endpoint_or_interface_descriptor;
	}

	/* Only store as many endpoints as we have room for */
	/**
	 * 最初bNumEndpoints为0，这里是所有已经解析的端点。
	 */
	if (ifp->desc.bNumEndpoints >= num_ep)
		goto skip_to_next_endpoint_or_interface_descriptor;

	/**
	 * 端点是早就申请好的，这里直接使用。
	 */
	endpoint = &ifp->endpoint[ifp->desc.bNumEndpoints];
	++ifp->desc.bNumEndpoints;

	memcpy(&endpoint->desc, d, n);
	/**
	 * 初始化队列的urb_list链表。
	 */
	INIT_LIST_HEAD(&endpoint->urb_list);

	/* Fix up bInterval values outside the legal range. Use 32 ms if no
	 * proper value can be guessed. */
	/**
	 * i,j代表合理的范围。
	 */
	i = 0;		/* i = min, j = max, n = default */
	j = 255;
	/**
	 * 处理端点的间隔时间。
	 */
	if (usb_endpoint_xfer_int(d)) {/* 中断端点。 */
		i = 1;
		switch (to_usb_device(ddev)->speed) {
		case USB_SPEED_HIGH:
			/* Many device manufacturers are using full-speed
			 * bInterval values in high-speed interrupt endpoint
			 * descriptors. Try to fix those and fall back to a
			 * 32 ms default value otherwise. */
			n = fls(d->bInterval*8);
			/**
			 * n是默认值。
			 */
			if (n == 0)
				n = 9;	/* 32 ms = 2^(9-1) uframes */
			j = 16;
			break;
		default:		/* USB_SPEED_FULL or _LOW */
			/* For low-speed, 10 ms is the official minimum.
			 * But some "overclocked" devices might want faster
			 * polling so we'll allow it. */
			n = 32;
			break;
		}
	} else if (usb_endpoint_xfer_isoc(d)) {/* 等时端点 */
		i = 1;
		j = 16;
		switch (to_usb_device(ddev)->speed) {
		case USB_SPEED_HIGH:
			n = 9;		/* 32 ms = 2^(9-1) uframes */
			break;
		default:		/* USB_SPEED_FULL */
			n = 6;		/* 32 ms = 2^(6-1) frames */
			break;
		}
	}
	/**
	 * 描述符中的值不正常，修复它。
	 */
	if (d->bInterval < i || d->bInterval > j) {
		dev_warn(ddev, "config %d interface %d altsetting %d "
		    "endpoint 0x%X has an invalid bInterval %d, "
		    "changing to %d\n",
		    cfgno, inum, asnum,
		    d->bEndpointAddress, d->bInterval, n);
		endpoint->desc.bInterval = n;
	}

	/* Some buggy low-speed devices have Bulk endpoints, which is
	 * explicitly forbidden by the USB spec.  In an attempt to make
	 * them usable, we will try treating them as Interrupt endpoints.
	 */
	if (to_usb_device(ddev)->speed == USB_SPEED_LOW &&
			usb_endpoint_xfer_bulk(d)) {
		dev_warn(ddev, "config %d interface %d altsetting %d "
		    "endpoint 0x%X is Bulk; changing to Interrupt\n",
		    cfgno, inum, asnum, d->bEndpointAddress);
		endpoint->desc.bmAttributes = USB_ENDPOINT_XFER_INT;
		endpoint->desc.bInterval = 1;
		if (le16_to_cpu(endpoint->desc.wMaxPacketSize) > 8)
			endpoint->desc.wMaxPacketSize = cpu_to_le16(8);
	}

	/* Skip over any Class Specific or Vendor Specific descriptors;
	 * find the next endpoint or interface descriptor */
	/**
	 * 找下一个端点描述符，顺便计算扩展描述符的大小。
	 */
	endpoint->extra = buffer;
	i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT,
	    USB_DT_INTERFACE, &n);
	endpoint->extralen = i;
	if (n > 0)
		dev_dbg(ddev, "skipped %d descriptor%s after %s\n",
		    n, plural(n), "endpoint");
	return buffer - buffer0 + i;

skip_to_next_endpoint_or_interface_descriptor:
	i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT,
	    USB_DT_INTERFACE, NULL);
	return buffer - buffer0 + i;
}

void usb_release_interface_cache(struct kref *ref)
{
	struct usb_interface_cache *intfc = ref_to_usb_interface_cache(ref);
	int j;

	for (j = 0; j < intfc->num_altsetting; j++) {
		struct usb_host_interface *alt = &intfc->altsetting[j];

		kfree(alt->endpoint);
		kfree(alt->string);
	}
	kfree(intfc);
}

/**
 * 解析配置描述符中的某个接口。
 */
static int usb_parse_interface(struct device *ddev, int cfgno,
    struct usb_host_config *config, unsigned char *buffer, int size,
    u8 inums[], u8 nalts[])
{
	unsigned char *buffer0 = buffer;
	struct usb_interface_descriptor	*d;
	int inum, asnum;
	struct usb_interface_cache *intfc;
	struct usb_host_interface *alt;
	int i, n;
	int len, retval;
	int num_ep, num_ep_orig;

	/**
	 * 传递的缓冲区是一个接口描述符指针。
	 */
	d = (struct usb_interface_descriptor *) buffer;
	/**
	 * 调整缓冲区位置和长度。
	 */
	buffer += d->bLength;
	size -= d->bLength;

	/**
	 * 健康检查。
	 */
	if (d->bLength < USB_DT_INTERFACE_SIZE)
		goto skip_to_next_interface_descriptor;

	/* Which interface entry is this? */
	intfc = NULL;
	inum = d->bInterfaceNumber;
	/**
	 * 在数组中查找当前接口的序号。
	 */
	for (i = 0; i < config->desc.bNumInterfaces; ++i) {
		if (inums[i] == inum) {
			intfc = config->intf_cache[i];
			break;
		}
	}
	if (!intfc || intfc->num_altsetting >= nalts[i])
		goto skip_to_next_interface_descriptor;

	/* Check for duplicate altsetting entries */
	asnum = d->bAlternateSetting;
	/**
	 * 获得这个接口描述符对应的设置编号。
	 */
	for ((i = 0, alt = &intfc->altsetting[0]);
	      i < intfc->num_altsetting;
	     (++i, ++alt)) {
		/**
		 * 如果这个设置已经遇到过了，就没有必要再对这个接口描述符进行处理。
		 */
		if (alt->desc.bAlternateSetting == asnum) {
			dev_warn(ddev, "Duplicate descriptor for config %d "
			    "interface %d altsetting %d, skipping\n",
			    cfgno, inum, asnum);
			goto skip_to_next_interface_descriptor;
		}
	}

	/**
	 * 发现了新的设置，将它加到cache里。
	 */
	++intfc->num_altsetting;
	memcpy(&alt->desc, d, USB_DT_INTERFACE_SIZE);

	/* Skip over any Class Specific or Vendor Specific descriptors;
	 * find the first endpoint or interface descriptor */
	alt->extra = buffer;
	/**
	 * 继续找下一个接口。如果find_next_descriptor>0，说明在本接口与下一个接口之间存在class或vendor-specific描述符。
	 */
	i = find_next_descriptor(buffer, size, USB_DT_ENDPOINT,
	    USB_DT_INTERFACE, &n);
	alt->extralen = i;
	if (n > 0)
		dev_dbg(ddev, "skipped %d descriptor%s after %s\n",
		    n, plural(n), "interface");
	/**
	 * 调整缓冲区位置。
	 */
	buffer += i;
	size -= i;

	/* Allocate space for the right(?) number of endpoints */
	/**
	 * 获得端点数。
	 */
	num_ep = num_ep_orig = alt->desc.bNumEndpoints;
	alt->desc.bNumEndpoints = 0;		// Use as a counter
	/**
	 * 端点数量超过协议规定的数量。
	 */
	if (num_ep > USB_MAXENDPOINTS) {
		dev_warn(ddev, "too many endpoints for config %d interface %d "
		    "altsetting %d: %d, using maximum allowed: %d\n",
		    cfgno, inum, asnum, num_ep, USB_MAXENDPOINTS);
		num_ep = USB_MAXENDPOINTS;
	}

	/**
	 * 为端点分配内存。
	 */
	if (num_ep > 0) {	/* Can't allocate 0 bytes */
		len = sizeof(struct usb_host_endpoint) * num_ep;
		alt->endpoint = kzalloc(len, GFP_KERNEL);
		if (!alt->endpoint)
			return -ENOMEM;
	}

	/* Parse all the endpoint descriptors */
	n = 0;
	/**
	 * 遍历本接口的所有端点描述符
	 */
	while (size > 0) {
		if (((struct usb_descriptor_header *) buffer)->bDescriptorType
		     == USB_DT_INTERFACE)/* 当前描述符类型是接口描述符，退出循环。 */
			break;
		/**
		 * 是端点描述符，解析它，将得到这个端点描述符的长度。
		 */
		retval = usb_parse_endpoint(ddev, cfgno, inum, asnum, alt,
		    num_ep, buffer, size);
		if (retval < 0)
			return retval;
		++n;

		/**
		 * 调整缓冲区，解析下一个描述符。
		 */
		buffer += retval;
		size -= retval;
	}

	if (n != num_ep_orig)
		dev_warn(ddev, "config %d interface %d altsetting %d has %d "
		    "endpoint descriptor%s, different from the interface "
		    "descriptor's value: %d\n",
		    cfgno, inum, asnum, n, plural(n), num_ep_orig);
	return buffer - buffer0;

skip_to_next_interface_descriptor:
	i = find_next_descriptor(buffer, size, USB_DT_INTERFACE,
	    USB_DT_INTERFACE, NULL);
	return buffer - buffer0 + i;
}

/**
 * 解析原始配置信息。
 */
static int usb_parse_configuration(struct device *ddev, int cfgidx,
    struct usb_host_config *config, unsigned char *buffer, int size)
{
	/**
	 * 需要对缓冲区进行解析，首先备份缓冲区指针。
	 */
	unsigned char *buffer0 = buffer;
	int cfgno;
	int nintf, nintf_orig;
	int i, j, n;
	struct usb_interface_cache *intfc;
	unsigned char *buffer2;
	int size2;
	struct usb_descriptor_header *header;
	int len, retval;
	u8 inums[USB_MAXINTERFACES], nalts[USB_MAXINTERFACES];
	unsigned iad_num = 0;

	memcpy(&config->desc, buffer, USB_DT_CONFIG_SIZE);
	/**
	 * 缓冲区的描述符类型不是配置描述符，其长度是否小于最小的配置长度。
	 */
	if (config->desc.bDescriptorType != USB_DT_CONFIG ||
	    config->desc.bLength < USB_DT_CONFIG_SIZE) {
		dev_err(ddev, "invalid descriptor for config index %d: "
		    "type = 0x%X, length = %d\n", cfgidx,
		    config->desc.bDescriptorType, config->desc.bLength);
		return -EINVAL;
	}
	cfgno = config->desc.bConfigurationValue;

	/**
	 * 修正buffer和size。
	 */
	buffer += config->desc.bLength;
	size -= config->desc.bLength;

	/**
	 * 获取配置的接口数。
	 */
	nintf = nintf_orig = config->desc.bNumInterfaces;
	/**
	 * 判断接口数是否超过限制。
	 */
	if (nintf > USB_MAXINTERFACES) {
		dev_warn(ddev, "config %d has too many interfaces: %d, "
		    "using maximum allowed: %d\n",
		    cfgno, nintf, USB_MAXINTERFACES);
		nintf = USB_MAXINTERFACES;
	}

	/* Go through the descriptors, checking their length and counting the
	 * number of altsettings for each interface */
	n = 0;
	/**
	 * 记录配置里面每个接口所拥有的设置数目。
	 */
	for ((buffer2 = buffer, size2 = size);
	      size2 > 0;
	     (buffer2 += header->bLength, size2 -= header->bLength)) {

		/**
		 * 明显不合法。
		 */
		if (size2 < sizeof(struct usb_descriptor_header)) {
			dev_warn(ddev, "config %d descriptor has %d excess "
			    "byte%s, ignoring\n",
			    cfgno, size2, plural(size2));
			break;
		}

		header = (struct usb_descriptor_header *) buffer2;
		/**
		 * 明显也不合法。
		 */
		if ((header->bLength > size2) || (header->bLength < 2)) {
			dev_warn(ddev, "config %d has an invalid descriptor "
			    "of length %d, skipping remainder of the config\n",
			    cfgno, header->bLength);
			break;
		}

		/**
		 * 接口描述符。
		 */
		if (header->bDescriptorType == USB_DT_INTERFACE) {
			struct usb_interface_descriptor *d;
			int inum;

			d = (struct usb_interface_descriptor *) header;
			/**
			 * 该描述符长度不合法。
			 */
			if (d->bLength < USB_DT_INTERFACE_SIZE) {
				dev_warn(ddev, "config %d has an invalid "
				    "interface descriptor of length %d, "
				    "skipping\n", cfgno, d->bLength);
				continue;
			}

			/**
			 * 接口序号。
			 */
			inum = d->bInterfaceNumber;
			/**
			 * 接口序号大于设备接口数目，也不合法。
			 */
			if (inum >= nintf_orig)
				dev_warn(ddev, "config %d has an invalid "
				    "interface number: %d but max is %d\n",
				    cfgno, inum, nintf_orig - 1);

			/* Have we already encountered this interface?
			 * Count its altsettings */
			/**
			 * n是接口数目，inums里面每一项都表示一个接口号，nalts的每一项记录的是每个接口拥有的设置数目。
			 */
			for (i = 0; i < n; ++i) {
				if (inums[i] == inum)/* 接口已经被解析过 */
					break;
			}
			if (i < n) {
				if (nalts[i] < 255)/* 增加已经存在的接口的设置数目。 */
					++nalts[i];
			} else if (n < USB_MAXINTERFACES) {/* 新接口 */
				inums[n] = inum;
				nalts[n] = 1;
				++n;
			}

		} else if (header->bDescriptorType ==
				USB_DT_INTERFACE_ASSOCIATION) {
			if (iad_num == USB_MAXIADS) {
				dev_warn(ddev, "found more Interface "
					       "Association Descriptors "
					       "than allocated for in "
					       "configuration %d\n", cfgno);
			} else {
				config->intf_assoc[iad_num] =
					(struct usb_interface_assoc_descriptor
					*)header;
				iad_num++;
			}

		} else if (header->bDescriptorType == USB_DT_DEVICE ||
			    header->bDescriptorType == USB_DT_CONFIG)/* 配置描述符里面不应该有配置描述符和设备描述符 */
			dev_warn(ddev, "config %d contains an unexpected "
			    "descriptor of type 0x%X, skipping\n",
			    cfgno, header->bDescriptorType);

	}	/* for ((buffer2 = buffer, size2 = size); ...) */
	size = buffer2 - buffer;
	/**
	 * 描述符的有效长度。
	 */
	config->desc.wTotalLength = cpu_to_le16(buffer2 - buffer0);

	/**
	 * 计算出的接口数目与配置描述符中的接口数目不符，或者没有发现任何接口，就警告。
	 */
	if (n != nintf)
		dev_warn(ddev, "config %d has %d interface%s, different from "
		    "the descriptor's value: %d\n",
		    cfgno, n, plural(n), nintf_orig);
	else if (n == 0)
		dev_warn(ddev, "config %d has no interfaces?\n", cfgno);
	config->desc.bNumInterfaces = nintf = n;

	/* Check for missing interface numbers */
	/**
	 * 遍历inums数组，确定0-n都在这个数组里面。
	 */
	for (i = 0; i < nintf; ++i) {
		for (j = 0; j < nintf; ++j) {
			if (inums[j] == i)
				break;
		}
		/**
		 * 某个接口序号不在数组里面，警告。
		 */
		if (j >= nintf)
			dev_warn(ddev, "config %d has no interface number "
			    "%d\n", cfgno, i);
	}

	/* Allocate the usb_interface_caches and altsetting arrays */
	/**
	 * 遍历每一个接口，为接口的设置分配内存。
	 */
	for (i = 0; i < nintf; ++i) {
		j = nalts[i];
		if (j > USB_MAXALTSETTING) {
			dev_warn(ddev, "too many alternate settings for "
			    "config %d interface %d: %d, "
			    "using maximum allowed: %d\n",
			    cfgno, inums[i], j, USB_MAXALTSETTING);
			nalts[i] = j = USB_MAXALTSETTING;
		}

		len = sizeof(*intfc) + sizeof(struct usb_host_interface) * j;
		config->intf_cache[i] = intfc = kzalloc(len, GFP_KERNEL);
		if (!intfc)
			return -ENOMEM;
		kref_init(&intfc->ref);
	}

	/* Skip over any Class Specific or Vendor Specific descriptors;
	 * find the first interface descriptor */
	config->extra = buffer;
	/**
	 * 找第一个接口描述符，如果后面还有class和vendor描述符，是i是其长度。
	 */
	i = find_next_descriptor(buffer, size, USB_DT_INTERFACE,
	    USB_DT_INTERFACE, &n);
	config->extralen = i;
	if (n > 0)
		dev_dbg(ddev, "skipped %d descriptor%s after %s\n",
		    n, plural(n), "configuration");
	/**
	 * 根据查找结果修正buffer和size。
	 */
	buffer += i;
	size -= i;

	/* Parse all the interface/altsetting descriptors */
	/**
	 * size是本配置描述符的长度，这里解析完所有的接口才退出循环。
	 */
	while (size > 0) {
		/**
		 * 解析接口、设置描述符。
		 */
		retval = usb_parse_interface(ddev, cfgno, config,
		    buffer, size, inums, nalts);
		if (retval < 0)
			return retval;

		buffer += retval;
		size -= retval;
	}

	/* Check for missing altsettings */
	/**
	 * 遍历当前配置的每个接口，看是否某个接口的设置编号不正常。
	 */
	for (i = 0; i < nintf; ++i) {
		intfc = config->intf_cache[i];
		for (j = 0; j < intfc->num_altsetting; ++j) {
			for (n = 0; n < intfc->num_altsetting; ++n) {
				if (intfc->altsetting[n].desc.
				    bAlternateSetting == j)
					break;
			}
			if (n >= intfc->num_altsetting)
				dev_warn(ddev, "config %d interface %d has no "
				    "altsetting %d\n", cfgno, inums[i], j);
		}
	}

	return 0;
}

// hub-only!! ... and only exported for reset/reinit path.
// otherwise used internally on disconnect/destroy path
void usb_destroy_configuration(struct usb_device *dev)
{
	int c, i;

	if (!dev->config)
		return;

	if (dev->rawdescriptors) {
		for (i = 0; i < dev->descriptor.bNumConfigurations; i++)
			kfree(dev->rawdescriptors[i]);

		kfree(dev->rawdescriptors);
		dev->rawdescriptors = NULL;
	}

	for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
		struct usb_host_config *cf = &dev->config[c];

		kfree(cf->string);
		for (i = 0; i < cf->desc.bNumInterfaces; i++) {
			if (cf->intf_cache[i])
				kref_put(&cf->intf_cache[i]->ref, 
					  usb_release_interface_cache);
		}
	}
	kfree(dev->config);
	dev->config = NULL;
}


/*
 * Get the USB config descriptors, cache and parse'em
 *
 * hub-only!! ... and only in reset path, or usb_new_device()
 * (used by real hubs and virtual root hubs)
 *
 * NOTE: if this is a WUSB device and is not authorized, we skip the
 *       whole thing. A non-authorized USB device has no
 *       configurations.
 */
/**
 * 获得设备的配置。
 */
int usb_get_configuration(struct usb_device *dev)
{
	struct device *ddev = &dev->dev;
	/**
	 * 获得设备描述符中指示的配置描述符数目。
	 */
	int ncfg = dev->descriptor.bNumConfigurations;
	int result = 0;
	unsigned int cfgno, length;
	unsigned char *buffer;
	unsigned char *bigbuffer;
 	struct usb_config_descriptor *desc;

	cfgno = 0;
	if (dev->authorized == 0)	/* Not really an error */
		goto out_not_authorized;
	result = -ENOMEM;
	/**
	 * 一个设备最多支持8种配置，如果配置数超过8个，强制将其设置为8。
	 */
	if (ncfg > USB_MAXCONFIG) {
		dev_warn(ddev, "too many configurations: %d, "
		    "using maximum allowed: %d\n", ncfg, USB_MAXCONFIG);
		dev->descriptor.bNumConfigurations = ncfg = USB_MAXCONFIG;
	}

	/**
	 * 无配置??
	 */
	if (ncfg < 1) {
		dev_err(ddev, "no configurations\n");
		return -EINVAL;
	}

	/**
	 * 为所有配置分配空间。
	 */
	length = ncfg * sizeof(struct usb_host_config);
	dev->config = kzalloc(length, GFP_KERNEL);
	if (!dev->config)
		goto err2;

	length = ncfg * sizeof(char *);
	/**
	 * 为配置描述符分配空间。
	 */
	dev->rawdescriptors = kzalloc(length, GFP_KERNEL);
	if (!dev->rawdescriptors)
		goto err2;

	/**
	 * 准备一个大小为USB_DT_CONFIG_SIZE的缓冲区，第一次发送GET_DESCRIPTOR 请求所用。
	 */
	buffer = kmalloc(USB_DT_CONFIG_SIZE, GFP_KERNEL);
	if (!buffer)
		goto err2;
	desc = (struct usb_config_descriptor *)buffer;

	/**
	 * 循环处理每一个配置。
	 */
	for (cfgno = 0; cfgno < ncfg; cfgno++) {
		/* We grab just the first descriptor so we know how long
		 * the whole configuration is */
		/**
		 * 首先发送USB_DT_CONFIG_SIZE 个字节请求，获得配置描述符的内容。
		 */
		result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno,
		    buffer, USB_DT_CONFIG_SIZE);
		if (result < 0) {/* 获取错误 */
			dev_err(ddev, "unable to read config index %d "
			    "descriptor/%s: %d\n", cfgno, "start", result);
			dev_err(ddev, "chopping to %d config(s)\n", cfgno);
			/**
			 * 重新设置配置数目。
			 */
			dev->descriptor.bNumConfigurations = cfgno;
			break;
		} else if (result < 4) {/* 长度获取不完整。 */
			dev_err(ddev, "config index %d descriptor too short "
			    "(expected %i, got %i)\n", cfgno,
			    USB_DT_CONFIG_SIZE, result);
			result = -EINVAL;
			goto err;
		}
		/**
		 * 第3，4字节即为wTotalLength，因此只要长度大于3就可以得到长度了。
		 */
		length = max((int) le16_to_cpu(desc->wTotalLength),
		    USB_DT_CONFIG_SIZE);

		/* Now that we know the length, get the whole thing */
		/**
		 * 知道了配置长度，即可以为它分配一个缓冲区，保存此配置了。
		 */
		bigbuffer = kmalloc(length, GFP_KERNEL);
		if (!bigbuffer) {
			result = -ENOMEM;
			goto err;
		}
		/**
		 * 重新获取完整的配置描述符。
		 */
		result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno,
		    bigbuffer, length);
		if (result < 0) {
			dev_err(ddev, "unable to read config index %d "
			    "descriptor/%s\n", cfgno, "all");
			kfree(bigbuffer);
			goto err;
		}
		if (result < length) {
			dev_warn(ddev, "config index %d descriptor too short "
			    "(expected %i, got %i)\n", cfgno, length, result);
			length = result;
		}

		/**
		 * 正常的得到配置描述符，调用usb_parse_configuration解析此配置。
		 */
		dev->rawdescriptors[cfgno] = bigbuffer;

		result = usb_parse_configuration(&dev->dev, cfgno,
		    &dev->config[cfgno], bigbuffer, length);
		if (result < 0) {
			++cfgno;
			goto err;
		}
	}
	result = 0;

err:
	kfree(buffer);
out_not_authorized:
	dev->descriptor.bNumConfigurations = cfgno;
err2:
	if (result == -ENOMEM)
		dev_err(ddev, "out of memory\n");
	return result;
}

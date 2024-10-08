/*
 * This file holds the definitions of quirks found in USB devices.
 * Only quirks that affect the whole device, not an interface,
 * belong here.
 */

/* string descriptors must not be fetched using a 255-byte read */
#define USB_QUIRK_STRING_FETCH_255	0x00000001

/* device can't resume correctly so reset it instead */
#define USB_QUIRK_RESET_RESUME		0x00000002

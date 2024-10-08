/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Artem Bityutskiy (Битюцкий Артём)
 */

#ifndef __UBI_DEBUG_H__
#define __UBI_DEBUG_H__

#ifdef CONFIG_MTD_UBI_DEBUG
#include <linux/random.h>

#define ubi_assert(expr)  BUG_ON(!(expr))
#define dbg_err(fmt, ...) ubi_err(fmt, ##__VA_ARGS__)
#else
#define ubi_assert(expr)  ({})
#define dbg_err(fmt, ...) ({})
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_DISABLE_BGT
#define DBG_DISABLE_BGT 1
#else
#define DBG_DISABLE_BGT 0
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_MSG
/* Generic debugging message */
#define dbg_msg(fmt, ...) \
	printk(KERN_DEBUG "UBI DBG: %s: " fmt "\n", __FUNCTION__, ##__VA_ARGS__)

#define ubi_dbg_dump_stack() dump_stack()

struct ubi_ec_hdr;
struct ubi_vid_hdr;
struct ubi_volume;
struct ubi_vtbl_record;
struct ubi_scan_volume;
struct ubi_scan_leb;
struct ubi_mkvol_req;

void ubi_dbg_dump_ec_hdr(const struct ubi_ec_hdr *ec_hdr);
void ubi_dbg_dump_vid_hdr(const struct ubi_vid_hdr *vid_hdr);
void ubi_dbg_dump_vol_info(const struct ubi_volume *vol);
void ubi_dbg_dump_vtbl_record(const struct ubi_vtbl_record *r, int idx);
void ubi_dbg_dump_sv(const struct ubi_scan_volume *sv);
void ubi_dbg_dump_seb(const struct ubi_scan_leb *seb, int type);
void ubi_dbg_dump_mkvol_req(const struct ubi_mkvol_req *req);

#else

#define dbg_msg(fmt, ...)    ({})
#define ubi_dbg_dump_stack() ({})
#define ubi_dbg_dump_ec_hdr(ec_hdr)      ({})
#define ubi_dbg_dump_vid_hdr(vid_hdr)    ({})
#define ubi_dbg_dump_vol_info(vol)       ({})
#define ubi_dbg_dump_vtbl_record(r, idx) ({})
#define ubi_dbg_dump_sv(sv)              ({})
#define ubi_dbg_dump_seb(seb, type)      ({})
#define ubi_dbg_dump_mkvol_req(req)      ({})

#endif /* CONFIG_MTD_UBI_DEBUG_MSG */

#ifdef CONFIG_MTD_UBI_DEBUG_MSG_EBA
/* Messages from the eraseblock association unit */
#define dbg_eba(fmt, ...) \
	printk(KERN_DEBUG "UBI DBG eba: %s: " fmt "\n", __FUNCTION__, \
	       ##__VA_ARGS__)
#else
#define dbg_eba(fmt, ...) ({})
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_MSG_WL
/* Messages from the wear-leveling unit */
#define dbg_wl(fmt, ...) \
	printk(KERN_DEBUG "UBI DBG wl: %s: " fmt "\n", __FUNCTION__, \
	       ##__VA_ARGS__)
#else
#define dbg_wl(fmt, ...) ({})
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_MSG_IO
/* Messages from the input/output unit */
#define dbg_io(fmt, ...) \
	printk(KERN_DEBUG "UBI DBG io: %s: " fmt "\n", __FUNCTION__, \
	       ##__VA_ARGS__)
#else
#define dbg_io(fmt, ...) ({})
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_MSG_BLD
/* Initialization and build messages */
#define dbg_bld(fmt, ...) \
	printk(KERN_DEBUG "UBI DBG bld: %s: " fmt "\n", __FUNCTION__, \
	       ##__VA_ARGS__)
#else
#define dbg_bld(fmt, ...) ({})
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_EMULATE_BITFLIPS
/**
 * ubi_dbg_is_bitflip - if it is time to emulate a bit-flip.
 *
 * Returns non-zero if a bit-flip should be emulated, otherwise returns zero.
 */
static inline int ubi_dbg_is_bitflip(void)
{
	return !(random32() % 200);
}
#else
#define ubi_dbg_is_bitflip() 0
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_EMULATE_WRITE_FAILURES
/**
 * ubi_dbg_is_write_failure - if it is time to emulate a write failure.
 *
 * Returns non-zero if a write failure should be emulated, otherwise returns
 * zero.
 */
static inline int ubi_dbg_is_write_failure(void)
{
	return !(random32() % 500);
}
#else
#define ubi_dbg_is_write_failure() 0
#endif

#ifdef CONFIG_MTD_UBI_DEBUG_EMULATE_ERASE_FAILURES
/**
 * ubi_dbg_is_erase_failure - if its time to emulate an erase failure.
 *
 * Returns non-zero if an erase failure should be emulated, otherwise returns
 * zero.
 */
static inline int ubi_dbg_is_erase_failure(void)
{
		return !(random32() % 400);
}
#else
#define ubi_dbg_is_erase_failure() 0
#endif

#endif /* !__UBI_DEBUG_H__ */

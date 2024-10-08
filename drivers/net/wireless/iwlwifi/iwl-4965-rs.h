/******************************************************************************
 *
 * Copyright(c) 2003 - 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * James P. Ketrenos <ipw2100-admin@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *****************************************************************************/

#ifndef __iwl_4965_rs_h__
#define __iwl_4965_rs_h__

#include "iwl-4965.h"

struct iwl_rate_info {
	u8 plcp;
	u8 plcp_siso;
	u8 plcp_mimo;
	u8 ieee;
	u8 prev_ieee;    /* previous rate in IEEE speeds */
	u8 next_ieee;    /* next rate in IEEE speeds */
	u8 prev_rs;      /* previous rate used in rs algo */
	u8 next_rs;      /* next rate used in rs algo */
	u8 prev_rs_tgg;  /* previous rate used in TGG rs algo */
	u8 next_rs_tgg;  /* next rate used in TGG rs algo */
};

enum {
	IWL_RATE_1M_INDEX = 0,
	IWL_RATE_2M_INDEX,
	IWL_RATE_5M_INDEX,
	IWL_RATE_11M_INDEX,
	IWL_RATE_6M_INDEX,
	IWL_RATE_9M_INDEX,
	IWL_RATE_12M_INDEX,
	IWL_RATE_18M_INDEX,
	IWL_RATE_24M_INDEX,
	IWL_RATE_36M_INDEX,
	IWL_RATE_48M_INDEX,
	IWL_RATE_54M_INDEX,
	IWL_RATE_60M_INDEX,
	IWL_RATE_COUNT,
	IWL_RATE_INVM_INDEX = IWL_RATE_COUNT,
	IWL_RATE_INVALID = IWL_RATE_INVM_INDEX
};

enum {
	IWL_FIRST_OFDM_RATE = IWL_RATE_6M_INDEX,
	IWL_LAST_OFDM_RATE = IWL_RATE_60M_INDEX,
	IWL_FIRST_CCK_RATE = IWL_RATE_1M_INDEX,
	IWL_LAST_CCK_RATE = IWL_RATE_11M_INDEX,
};

/* #define vs. enum to keep from defaulting to 'large integer' */
#define	IWL_RATE_6M_MASK   (1<<IWL_RATE_6M_INDEX)
#define	IWL_RATE_9M_MASK   (1<<IWL_RATE_9M_INDEX)
#define	IWL_RATE_12M_MASK  (1<<IWL_RATE_12M_INDEX)
#define	IWL_RATE_18M_MASK  (1<<IWL_RATE_18M_INDEX)
#define	IWL_RATE_24M_MASK  (1<<IWL_RATE_24M_INDEX)
#define	IWL_RATE_36M_MASK  (1<<IWL_RATE_36M_INDEX)
#define	IWL_RATE_48M_MASK  (1<<IWL_RATE_48M_INDEX)
#define	IWL_RATE_54M_MASK  (1<<IWL_RATE_54M_INDEX)
#define IWL_RATE_60M_MASK  (1<<IWL_RATE_60M_INDEX)
#define	IWL_RATE_1M_MASK   (1<<IWL_RATE_1M_INDEX)
#define	IWL_RATE_2M_MASK   (1<<IWL_RATE_2M_INDEX)
#define	IWL_RATE_5M_MASK   (1<<IWL_RATE_5M_INDEX)
#define	IWL_RATE_11M_MASK  (1<<IWL_RATE_11M_INDEX)

enum {
	IWL_RATE_6M_PLCP  = 13,
	IWL_RATE_9M_PLCP  = 15,
	IWL_RATE_12M_PLCP = 5,
	IWL_RATE_18M_PLCP = 7,
	IWL_RATE_24M_PLCP = 9,
	IWL_RATE_36M_PLCP = 11,
	IWL_RATE_48M_PLCP = 1,
	IWL_RATE_54M_PLCP = 3,
	IWL_RATE_60M_PLCP = 3,
	IWL_RATE_1M_PLCP  = 10,
	IWL_RATE_2M_PLCP  = 20,
	IWL_RATE_5M_PLCP  = 55,
	IWL_RATE_11M_PLCP = 110,
};

/* OFDM HT rate plcp */
enum {
	IWL_RATE_SISO_6M_PLCP = 0,
	IWL_RATE_SISO_12M_PLCP = 1,
	IWL_RATE_SISO_18M_PLCP = 2,
	IWL_RATE_SISO_24M_PLCP = 3,
	IWL_RATE_SISO_36M_PLCP = 4,
	IWL_RATE_SISO_48M_PLCP = 5,
	IWL_RATE_SISO_54M_PLCP = 6,
	IWL_RATE_SISO_60M_PLCP = 7,
	IWL_RATE_MIMO_6M_PLCP  = 0x8,
	IWL_RATE_MIMO_12M_PLCP = 0x9,
	IWL_RATE_MIMO_18M_PLCP = 0xa,
	IWL_RATE_MIMO_24M_PLCP = 0xb,
	IWL_RATE_MIMO_36M_PLCP = 0xc,
	IWL_RATE_MIMO_48M_PLCP = 0xd,
	IWL_RATE_MIMO_54M_PLCP = 0xe,
	IWL_RATE_MIMO_60M_PLCP = 0xf,
	IWL_RATE_SISO_INVM_PLCP,
	IWL_RATE_MIMO_INVM_PLCP = IWL_RATE_SISO_INVM_PLCP,
};

enum {
	IWL_RATE_6M_IEEE  = 12,
	IWL_RATE_9M_IEEE  = 18,
	IWL_RATE_12M_IEEE = 24,
	IWL_RATE_18M_IEEE = 36,
	IWL_RATE_24M_IEEE = 48,
	IWL_RATE_36M_IEEE = 72,
	IWL_RATE_48M_IEEE = 96,
	IWL_RATE_54M_IEEE = 108,
	IWL_RATE_60M_IEEE = 120,
	IWL_RATE_1M_IEEE  = 2,
	IWL_RATE_2M_IEEE  = 4,
	IWL_RATE_5M_IEEE  = 11,
	IWL_RATE_11M_IEEE = 22,
};

#define IWL_CCK_BASIC_RATES_MASK    \
       (IWL_RATE_1M_MASK          | \
	IWL_RATE_2M_MASK)

#define IWL_CCK_RATES_MASK          \
       (IWL_BASIC_RATES_MASK      | \
	IWL_RATE_5M_MASK          | \
	IWL_RATE_11M_MASK)

#define IWL_OFDM_BASIC_RATES_MASK   \
	(IWL_RATE_6M_MASK         | \
	IWL_RATE_12M_MASK         | \
	IWL_RATE_24M_MASK)

#define IWL_OFDM_RATES_MASK         \
       (IWL_OFDM_BASIC_RATES_MASK | \
	IWL_RATE_9M_MASK          | \
	IWL_RATE_18M_MASK         | \
	IWL_RATE_36M_MASK         | \
	IWL_RATE_48M_MASK         | \
	IWL_RATE_54M_MASK)

#define IWL_BASIC_RATES_MASK         \
	(IWL_OFDM_BASIC_RATES_MASK | \
	 IWL_CCK_BASIC_RATES_MASK)

#define IWL_RATES_MASK ((1<<IWL_RATE_COUNT)-1)

#define IWL_INVALID_VALUE    -1

#define IWL_MIN_RSSI_VAL                 -100
#define IWL_MAX_RSSI_VAL                    0

#define IWL_LEGACY_SWITCH_ANTENNA	0
#define IWL_LEGACY_SWITCH_SISO		1
#define IWL_LEGACY_SWITCH_MIMO	        2

#define IWL_RS_GOOD_RATIO		12800

#define IWL_ACTION_LIMIT		3
#define IWL_LEGACY_FAILURE_LIMIT	160
#define IWL_LEGACY_SUCCESS_LIMIT	480
#define IWL_LEGACY_TABLE_COUNT		160

#define IWL_NONE_LEGACY_FAILURE_LIMIT	400
#define IWL_NONE_LEGACY_SUCCESS_LIMIT	4500
#define IWL_NONE_LEGACY_TABLE_COUNT	1500

#define IWL_RATE_SCALE_SWITCH		(10880)

#define IWL_SISO_SWITCH_ANTENNA		0
#define IWL_SISO_SWITCH_MIMO		1
#define IWL_SISO_SWITCH_GI		2

#define IWL_MIMO_SWITCH_ANTENNA_A	0
#define IWL_MIMO_SWITCH_ANTENNA_B	1
#define IWL_MIMO_SWITCH_GI		2

#define LQ_SIZE		2

extern const struct iwl_rate_info iwl_rates[IWL_RATE_COUNT];

enum iwl_table_type {
	LQ_NONE,
	LQ_G,
	LQ_A,
	LQ_SISO,
	LQ_MIMO,
	LQ_MAX,
};

enum iwl_antenna_type {
	ANT_NONE,
	ANT_MAIN,
	ANT_AUX,
	ANT_BOTH,
};

static inline u8 iwl_get_prev_ieee_rate(u8 rate_index)
{
	u8 rate = iwl_rates[rate_index].prev_ieee;

	if (rate == IWL_RATE_INVALID)
		rate = rate_index;
	return rate;
}

extern int iwl_rate_index_from_plcp(int plcp);

/**
 * iwl_fill_rs_info - Fill an output text buffer with the rate representation
 *
 * NOTE:  This is provided as a quick mechanism for a user to visualize
 * the performance of the rate control alogirthm and is not meant to be
 * parsed software.
 */
extern int iwl_fill_rs_info(struct ieee80211_hw *, char *buf, u8 sta_id);

/**
 * iwl_rate_scale_init - Initialize the rate scale table based on assoc info
 *
 * The specific througput table used is based on the type of network
 * the associated with, including A, B, G, and G w/ TGG protection
 */
extern void iwl_rate_scale_init(struct ieee80211_hw *hw, s32 sta_id);

/**
 * iwl_rate_control_register - Register the rate control algorithm callbacks
 *
 * Since the rate control algorithm is hardware specific, there is no need
 * or reason to place it as a stand alone module.  The driver can call
 * iwl_rate_control_register in order to register the rate control callbacks
 * with the mac80211 subsystem.  This should be performed prior to calling
 * ieee80211_register_hw
 *
 */
extern void iwl_rate_control_register(struct ieee80211_hw *hw);

/**
 * iwl_rate_control_unregister - Unregister the rate control callbacks
 *
 * This should be called after calling ieee80211_unregister_hw, but before
 * the driver is unloaded.
 */
extern void iwl_rate_control_unregister(struct ieee80211_hw *hw);

#endif

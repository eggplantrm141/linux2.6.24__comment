/*
    Conexant cx24123/cx24109 - DVB QPSK Satellite demod/tuner driver

    Copyright (C) 2005 Steven Toth <stoth@hauppauge.com>

    Support for KWorld DVB-S 100 by Vadim Catana <skystar@moldova.cc>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "dvb_frontend.h"
#include "cx24123.h"

#define XTAL 10111000

static int force_band;
static int debug;
#define dprintk(args...) \
	do { \
		if (debug) printk (KERN_DEBUG "cx24123: " args); \
	} while (0)

struct cx24123_state
{
	struct i2c_adapter* i2c;
	const struct cx24123_config* config;

	struct dvb_frontend frontend;

	/* Some PLL specifics for tuning */
	u32 VCAarg;
	u32 VGAarg;
	u32 bandselectarg;
	u32 pllarg;
	u32 FILTune;

	/* The Demod/Tuner can't easily provide these, we cache them */
	u32 currentfreq;
	u32 currentsymbolrate;
};

/* Various tuner defaults need to be established for a given symbol rate Sps */
static struct
{
	u32 symbolrate_low;
	u32 symbolrate_high;
	u32 VCAprogdata;
	u32 VGAprogdata;
	u32 FILTune;
} cx24123_AGC_vals[] =
{
	{
		.symbolrate_low		= 1000000,
		.symbolrate_high	= 4999999,
		/* the specs recommend other values for VGA offsets,
		   but tests show they are wrong */
		.VGAprogdata		= (1 << 19) | (0x180 << 9) | 0x1e0,
		.VCAprogdata		= (2 << 19) | (0x07 << 9) | 0x07,
		.FILTune		= 0x27f /* 0.41 V */
	},
	{
		.symbolrate_low		=  5000000,
		.symbolrate_high	= 14999999,
		.VGAprogdata		= (1 << 19) | (0x180 << 9) | 0x1e0,
		.VCAprogdata		= (2 << 19) | (0x07 << 9) | 0x1f,
		.FILTune		= 0x317 /* 0.90 V */
	},
	{
		.symbolrate_low		= 15000000,
		.symbolrate_high	= 45000000,
		.VGAprogdata		= (1 << 19) | (0x100 << 9) | 0x180,
		.VCAprogdata		= (2 << 19) | (0x07 << 9) | 0x3f,
		.FILTune		= 0x145 /* 2.70 V */
	},
};

/*
 * Various tuner defaults need to be established for a given frequency kHz.
 * fixme: The bounds on the bands do not match the doc in real life.
 * fixme: Some of them have been moved, other might need adjustment.
 */
static struct
{
	u32 freq_low;
	u32 freq_high;
	u32 VCOdivider;
	u32 progdata;
} cx24123_bandselect_vals[] =
{
	/* band 1 */
	{
		.freq_low	= 950000,
		.freq_high	= 1074999,
		.VCOdivider	= 4,
		.progdata	= (0 << 19) | (0 << 9) | 0x40,
	},

	/* band 2 */
	{
		.freq_low	= 1075000,
		.freq_high	= 1177999,
		.VCOdivider	= 4,
		.progdata	= (0 << 19) | (0 << 9) | 0x80,
	},

	/* band 3 */
	{
		.freq_low	= 1178000,
		.freq_high	= 1295999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x01,
	},

	/* band 4 */
	{
		.freq_low	= 1296000,
		.freq_high	= 1431999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x02,
	},

	/* band 5 */
	{
		.freq_low	= 1432000,
		.freq_high	= 1575999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x04,
	},

	/* band 6 */
	{
		.freq_low	= 1576000,
		.freq_high	= 1717999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x08,
	},

	/* band 7 */
	{
		.freq_low	= 1718000,
		.freq_high	= 1855999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x10,
	},

	/* band 8 */
	{
		.freq_low	= 1856000,
		.freq_high	= 2035999,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x20,
	},

	/* band 9 */
	{
		.freq_low	= 2036000,
		.freq_high	= 2150000,
		.VCOdivider	= 2,
		.progdata	= (0 << 19) | (1 << 9) | 0x40,
	},
};

static struct {
	u8 reg;
	u8 data;
} cx24123_regdata[] =
{
	{0x00, 0x03}, /* Reset system */
	{0x00, 0x00}, /* Clear reset */
	{0x03, 0x07}, /* QPSK, DVB, Auto Acquisition (default) */
	{0x04, 0x10}, /* MPEG */
	{0x05, 0x04}, /* MPEG */
	{0x06, 0x31}, /* MPEG (default) */
	{0x0b, 0x00}, /* Freq search start point (default) */
	{0x0c, 0x00}, /* Demodulator sample gain (default) */
	{0x0d, 0x7f}, /* Force driver to shift until the maximum (+-10 MHz) */
	{0x0e, 0x03}, /* Default non-inverted, FEC 3/4 (default) */
	{0x0f, 0xfe}, /* FEC search mask (all supported codes) */
	{0x10, 0x01}, /* Default search inversion, no repeat (default) */
	{0x16, 0x00}, /* Enable reading of frequency */
	{0x17, 0x01}, /* Enable EsNO Ready Counter */
	{0x1c, 0x80}, /* Enable error counter */
	{0x20, 0x00}, /* Tuner burst clock rate = 500KHz */
	{0x21, 0x15}, /* Tuner burst mode, word length = 0x15 */
	{0x28, 0x00}, /* Enable FILTERV with positive pol., DiSEqC 2.x off */
	{0x29, 0x00}, /* DiSEqC LNB_DC off */
	{0x2a, 0xb0}, /* DiSEqC Parameters (default) */
	{0x2b, 0x73}, /* DiSEqC Tone Frequency (default) */
	{0x2c, 0x00}, /* DiSEqC Message (0x2c - 0x31) */
	{0x2d, 0x00},
	{0x2e, 0x00},
	{0x2f, 0x00},
	{0x30, 0x00},
	{0x31, 0x00},
	{0x32, 0x8c}, /* DiSEqC Parameters (default) */
	{0x33, 0x00}, /* Interrupts off (0x33 - 0x34) */
	{0x34, 0x00},
	{0x35, 0x03}, /* DiSEqC Tone Amplitude (default) */
	{0x36, 0x02}, /* DiSEqC Parameters (default) */
	{0x37, 0x3a}, /* DiSEqC Parameters (default) */
	{0x3a, 0x00}, /* Enable AGC accumulator (for signal strength) */
	{0x44, 0x00}, /* Constellation (default) */
	{0x45, 0x00}, /* Symbol count (default) */
	{0x46, 0x0d}, /* Symbol rate estimator on (default) */
	{0x56, 0xc1}, /* Error Counter = Viterbi BER */
	{0x57, 0xff}, /* Error Counter Window (default) */
	{0x5c, 0x20}, /* Acquisition AFC Expiration window (default is 0x10) */
	{0x67, 0x83}, /* Non-DCII symbol clock */
};

static int cx24123_writereg(struct cx24123_state* state, int reg, int data)
{
	u8 buf[] = { reg, data };
	struct i2c_msg msg = { .addr = state->config->demod_address, .flags = 0, .buf = buf, .len = 2 };
	int err;

	if (debug>1)
		printk("cx24123: %s:  write reg 0x%02x, value 0x%02x\n",
						__FUNCTION__,reg, data);

	if ((err = i2c_transfer(state->i2c, &msg, 1)) != 1) {
		printk("%s: writereg error(err == %i, reg == 0x%02x,"
			 " data == 0x%02x)\n", __FUNCTION__, err, reg, data);
		return -EREMOTEIO;
	}

	return 0;
}

static int cx24123_readreg(struct cx24123_state* state, u8 reg)
{
	int ret;
	u8 b0[] = { reg };
	u8 b1[] = { 0 };
	struct i2c_msg msg[] = {
		{ .addr = state->config->demod_address, .flags = 0, .buf = b0, .len = 1 },
		{ .addr = state->config->demod_address, .flags = I2C_M_RD, .buf = b1, .len = 1 }
	};

	ret = i2c_transfer(state->i2c, msg, 2);

	if (ret != 2) {
		printk("%s: reg=0x%x (error=%d)\n", __FUNCTION__, reg, ret);
		return ret;
	}

	if (debug>1)
		printk("cx24123: read reg 0x%02x, value 0x%02x\n",reg, ret);

	return b1[0];
}

static int cx24123_set_inversion(struct cx24123_state* state, fe_spectral_inversion_t inversion)
{
	u8 nom_reg = cx24123_readreg(state, 0x0e);
	u8 auto_reg = cx24123_readreg(state, 0x10);

	switch (inversion) {
	case INVERSION_OFF:
		dprintk("%s:  inversion off\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg & ~0x80);
		cx24123_writereg(state, 0x10, auto_reg | 0x80);
		break;
	case INVERSION_ON:
		dprintk("%s:  inversion on\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x80);
		cx24123_writereg(state, 0x10, auto_reg | 0x80);
		break;
	case INVERSION_AUTO:
		dprintk("%s:  inversion auto\n",__FUNCTION__);
		cx24123_writereg(state, 0x10, auto_reg & ~0x80);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cx24123_get_inversion(struct cx24123_state* state, fe_spectral_inversion_t *inversion)
{
	u8 val;

	val = cx24123_readreg(state, 0x1b) >> 7;

	if (val == 0) {
		dprintk("%s:  read inversion off\n",__FUNCTION__);
		*inversion = INVERSION_OFF;
	} else {
		dprintk("%s:  read inversion on\n",__FUNCTION__);
		*inversion = INVERSION_ON;
	}

	return 0;
}

static int cx24123_set_fec(struct cx24123_state* state, fe_code_rate_t fec)
{
	u8 nom_reg = cx24123_readreg(state, 0x0e) & ~0x07;

	if ( (fec < FEC_NONE) || (fec > FEC_AUTO) )
		fec = FEC_AUTO;

	/* Set the soft decision threshold */
	if(fec == FEC_1_2)
		cx24123_writereg(state, 0x43, cx24123_readreg(state, 0x43) | 0x01);
	else
		cx24123_writereg(state, 0x43, cx24123_readreg(state, 0x43) & ~0x01);

	switch (fec) {
	case FEC_1_2:
		dprintk("%s:  set FEC to 1/2\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x01);
		cx24123_writereg(state, 0x0f, 0x02);
		break;
	case FEC_2_3:
		dprintk("%s:  set FEC to 2/3\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x02);
		cx24123_writereg(state, 0x0f, 0x04);
		break;
	case FEC_3_4:
		dprintk("%s:  set FEC to 3/4\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x03);
		cx24123_writereg(state, 0x0f, 0x08);
		break;
	case FEC_4_5:
		dprintk("%s:  set FEC to 4/5\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x04);
		cx24123_writereg(state, 0x0f, 0x10);
		break;
	case FEC_5_6:
		dprintk("%s:  set FEC to 5/6\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x05);
		cx24123_writereg(state, 0x0f, 0x20);
		break;
	case FEC_6_7:
		dprintk("%s:  set FEC to 6/7\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x06);
		cx24123_writereg(state, 0x0f, 0x40);
		break;
	case FEC_7_8:
		dprintk("%s:  set FEC to 7/8\n",__FUNCTION__);
		cx24123_writereg(state, 0x0e, nom_reg | 0x07);
		cx24123_writereg(state, 0x0f, 0x80);
		break;
	case FEC_AUTO:
		dprintk("%s:  set FEC to auto\n",__FUNCTION__);
		cx24123_writereg(state, 0x0f, 0xfe);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int cx24123_get_fec(struct cx24123_state* state, fe_code_rate_t *fec)
{
	int ret;

	ret = cx24123_readreg (state, 0x1b);
	if (ret < 0)
		return ret;
	ret = ret & 0x07;

	switch (ret) {
	case 1:
		*fec = FEC_1_2;
		break;
	case 2:
		*fec = FEC_2_3;
		break;
	case 3:
		*fec = FEC_3_4;
		break;
	case 4:
		*fec = FEC_4_5;
		break;
	case 5:
		*fec = FEC_5_6;
		break;
	case 6:
		*fec = FEC_6_7;
		break;
	case 7:
		*fec = FEC_7_8;
		break;
	default:
		/* this can happen when there's no lock */
		*fec = FEC_NONE;
	}

	return 0;
}

/* Approximation of closest integer of log2(a/b). It actually gives the
   lowest integer i such that 2^i >= round(a/b) */
static u32 cx24123_int_log2(u32 a, u32 b)
{
	u32 exp, nearest = 0;
	u32 div = a / b;
	if(a % b >= b / 2) ++div;
	if(div < (1 << 31))
	{
		for(exp = 1; div > exp; nearest++)
			exp += exp;
	}
	return nearest;
}

static int cx24123_set_symbolrate(struct cx24123_state* state, u32 srate)
{
	u32 tmp, sample_rate, ratio, sample_gain;
	u8 pll_mult;

	/*  check if symbol rate is within limits */
	if ((srate > state->frontend.ops.info.symbol_rate_max) ||
	    (srate < state->frontend.ops.info.symbol_rate_min))
		return -EOPNOTSUPP;;

	/* choose the sampling rate high enough for the required operation,
	   while optimizing the power consumed by the demodulator */
	if (srate < (XTAL*2)/2)
		pll_mult = 2;
	else if (srate < (XTAL*3)/2)
		pll_mult = 3;
	else if (srate < (XTAL*4)/2)
		pll_mult = 4;
	else if (srate < (XTAL*5)/2)
		pll_mult = 5;
	else if (srate < (XTAL*6)/2)
		pll_mult = 6;
	else if (srate < (XTAL*7)/2)
		pll_mult = 7;
	else if (srate < (XTAL*8)/2)
		pll_mult = 8;
	else
		pll_mult = 9;


	sample_rate = pll_mult * XTAL;

	/*
	    SYSSymbolRate[21:0] = (srate << 23) / sample_rate

	    We have to use 32 bit unsigned arithmetic without precision loss.
	    The maximum srate is 45000000 or 0x02AEA540. This number has
	    only 6 clear bits on top, hence we can shift it left only 6 bits
	    at a time. Borrowed from cx24110.c
	*/

	tmp = srate << 6;
	ratio = tmp / sample_rate;

	tmp = (tmp % sample_rate) << 6;
	ratio = (ratio << 6) + (tmp / sample_rate);

	tmp = (tmp % sample_rate) << 6;
	ratio = (ratio << 6) + (tmp / sample_rate);

	tmp = (tmp % sample_rate) << 5;
	ratio = (ratio << 5) + (tmp / sample_rate);


	cx24123_writereg(state, 0x01, pll_mult * 6);

	cx24123_writereg(state, 0x08, (ratio >> 16) & 0x3f );
	cx24123_writereg(state, 0x09, (ratio >>  8) & 0xff );
	cx24123_writereg(state, 0x0a, (ratio      ) & 0xff );

	/* also set the demodulator sample gain */
	sample_gain = cx24123_int_log2(sample_rate, srate);
	tmp = cx24123_readreg(state, 0x0c) & ~0xe0;
	cx24123_writereg(state, 0x0c, tmp | sample_gain << 5);

	dprintk("%s: srate=%d, ratio=0x%08x, sample_rate=%i sample_gain=%d\n", __FUNCTION__, srate, ratio, sample_rate, sample_gain);

	return 0;
}

/*
 * Based on the required frequency and symbolrate, the tuner AGC has to be configured
 * and the correct band selected. Calculate those values
 */
static int cx24123_pll_calculate(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct cx24123_state *state = fe->demodulator_priv;
	u32 ndiv = 0, adiv = 0, vco_div = 0;
	int i = 0;
	int pump = 2;
	int band = 0;
	int num_bands = ARRAY_SIZE(cx24123_bandselect_vals);

	/* Defaults for low freq, low rate */
	state->VCAarg = cx24123_AGC_vals[0].VCAprogdata;
	state->VGAarg = cx24123_AGC_vals[0].VGAprogdata;
	state->bandselectarg = cx24123_bandselect_vals[0].progdata;
	vco_div = cx24123_bandselect_vals[0].VCOdivider;

	/* For the given symbol rate, determine the VCA, VGA and FILTUNE programming bits */
	for (i = 0; i < ARRAY_SIZE(cx24123_AGC_vals); i++)
	{
		if ((cx24123_AGC_vals[i].symbolrate_low <= p->u.qpsk.symbol_rate) &&
		    (cx24123_AGC_vals[i].symbolrate_high >= p->u.qpsk.symbol_rate) ) {
			state->VCAarg = cx24123_AGC_vals[i].VCAprogdata;
			state->VGAarg = cx24123_AGC_vals[i].VGAprogdata;
			state->FILTune = cx24123_AGC_vals[i].FILTune;
		}
	}

	/* determine the band to use */
	if(force_band < 1 || force_band > num_bands)
	{
		for (i = 0; i < num_bands; i++)
		{
			if ((cx24123_bandselect_vals[i].freq_low <= p->frequency) &&
			    (cx24123_bandselect_vals[i].freq_high >= p->frequency) )
				band = i;
		}
	}
	else
		band = force_band - 1;

	state->bandselectarg = cx24123_bandselect_vals[band].progdata;
	vco_div = cx24123_bandselect_vals[band].VCOdivider;

	/* determine the charge pump current */
	if ( p->frequency < (cx24123_bandselect_vals[band].freq_low + cx24123_bandselect_vals[band].freq_high)/2 )
		pump = 0x01;
	else
		pump = 0x02;

	/* Determine the N/A dividers for the requested lband freq (in kHz). */
	/* Note: the reference divider R=10, frequency is in KHz, XTAL is in Hz */
	ndiv = ( ((p->frequency * vco_div * 10) / (2 * XTAL / 1000)) / 32) & 0x1ff;
	adiv = ( ((p->frequency * vco_div * 10) / (2 * XTAL / 1000)) % 32) & 0x1f;

	if (adiv == 0 && ndiv > 0)
		ndiv--;

	/* control bits 11, refdiv 11, charge pump polarity 1, charge pump current, ndiv, adiv */
	state->pllarg = (3 << 19) | (3 << 17) | (1 << 16) | (pump << 14) | (ndiv << 5) | adiv;

	return 0;
}

/*
 * Tuner data is 21 bits long, must be left-aligned in data.
 * Tuner cx24109 is written through a dedicated 3wire interface on the demod chip.
 */
static int cx24123_pll_writereg(struct dvb_frontend* fe, struct dvb_frontend_parameters *p, u32 data)
{
	struct cx24123_state *state = fe->demodulator_priv;
	unsigned long timeout;

	dprintk("%s:  pll writereg called, data=0x%08x\n",__FUNCTION__,data);

	/* align the 21 bytes into to bit23 boundary */
	data = data << 3;

	/* Reset the demod pll word length to 0x15 bits */
	cx24123_writereg(state, 0x21, 0x15);

	/* write the msb 8 bits, wait for the send to be completed */
	timeout = jiffies + msecs_to_jiffies(40);
	cx24123_writereg(state, 0x22, (data >> 16) & 0xff);
	while ((cx24123_readreg(state, 0x20) & 0x40) == 0) {
		if (time_after(jiffies, timeout)) {
			printk("%s:  demodulator is not responding, possibly hung, aborting.\n", __FUNCTION__);
			return -EREMOTEIO;
		}
		msleep(10);
	}

	/* send another 8 bytes, wait for the send to be completed */
	timeout = jiffies + msecs_to_jiffies(40);
	cx24123_writereg(state, 0x22, (data>>8) & 0xff );
	while ((cx24123_readreg(state, 0x20) & 0x40) == 0) {
		if (time_after(jiffies, timeout)) {
			printk("%s:  demodulator is not responding, possibly hung, aborting.\n", __FUNCTION__);
			return -EREMOTEIO;
		}
		msleep(10);
	}

	/* send the lower 5 bits of this byte, padded with 3 LBB, wait for the send to be completed */
	timeout = jiffies + msecs_to_jiffies(40);
	cx24123_writereg(state, 0x22, (data) & 0xff );
	while ((cx24123_readreg(state, 0x20) & 0x80)) {
		if (time_after(jiffies, timeout)) {
			printk("%s:  demodulator is not responding, possibly hung, aborting.\n", __FUNCTION__);
			return -EREMOTEIO;
		}
		msleep(10);
	}

	/* Trigger the demod to configure the tuner */
	cx24123_writereg(state, 0x20, cx24123_readreg(state, 0x20) | 2);
	cx24123_writereg(state, 0x20, cx24123_readreg(state, 0x20) & 0xfd);

	return 0;
}

static int cx24123_pll_tune(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct cx24123_state *state = fe->demodulator_priv;
	u8 val;

	dprintk("frequency=%i\n", p->frequency);

	if (cx24123_pll_calculate(fe, p) != 0) {
		printk("%s: cx24123_pll_calcutate failed\n",__FUNCTION__);
		return -EINVAL;
	}

	/* Write the new VCO/VGA */
	cx24123_pll_writereg(fe, p, state->VCAarg);
	cx24123_pll_writereg(fe, p, state->VGAarg);

	/* Write the new bandselect and pll args */
	cx24123_pll_writereg(fe, p, state->bandselectarg);
	cx24123_pll_writereg(fe, p, state->pllarg);

	/* set the FILTUNE voltage */
	val = cx24123_readreg(state, 0x28) & ~0x3;
	cx24123_writereg(state, 0x27, state->FILTune >> 2);
	cx24123_writereg(state, 0x28, val | (state->FILTune & 0x3));

	dprintk("%s:  pll tune VCA=%d, band=%d, pll=%d\n",__FUNCTION__,state->VCAarg,
			state->bandselectarg,state->pllarg);

	return 0;
}

static int cx24123_initfe(struct dvb_frontend* fe)
{
	struct cx24123_state *state = fe->demodulator_priv;
	int i;

	dprintk("%s:  init frontend\n",__FUNCTION__);

	/* Configure the demod to a good set of defaults */
	for (i = 0; i < ARRAY_SIZE(cx24123_regdata); i++)
		cx24123_writereg(state, cx24123_regdata[i].reg, cx24123_regdata[i].data);

	/* Set the LNB polarity */
	if(state->config->lnb_polarity)
		cx24123_writereg(state, 0x32, cx24123_readreg(state, 0x32) | 0x02);

	return 0;
}

static int cx24123_set_voltage(struct dvb_frontend* fe, fe_sec_voltage_t voltage)
{
	struct cx24123_state *state = fe->demodulator_priv;
	u8 val;

	val = cx24123_readreg(state, 0x29) & ~0x40;

	switch (voltage) {
	case SEC_VOLTAGE_13:
		dprintk("%s: setting voltage 13V\n", __FUNCTION__);
		return cx24123_writereg(state, 0x29, val & 0x7f);
	case SEC_VOLTAGE_18:
		dprintk("%s: setting voltage 18V\n", __FUNCTION__);
		return cx24123_writereg(state, 0x29, val | 0x80);
	case SEC_VOLTAGE_OFF:
		/* already handled in cx88-dvb */
		return 0;
	default:
		return -EINVAL;
	};

	return 0;
}

/* wait for diseqc queue to become ready (or timeout) */
static void cx24123_wait_for_diseqc(struct cx24123_state *state)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(200);
	while (!(cx24123_readreg(state, 0x29) & 0x40)) {
		if(time_after(jiffies, timeout)) {
			printk("%s: diseqc queue not ready, command may be lost.\n", __FUNCTION__);
			break;
		}
		msleep(10);
	}
}

static int cx24123_send_diseqc_msg(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd *cmd)
{
	struct cx24123_state *state = fe->demodulator_priv;
	int i, val, tone;

	dprintk("%s:\n",__FUNCTION__);

	/* stop continuous tone if enabled */
	tone = cx24123_readreg(state, 0x29);
	if (tone & 0x10)
		cx24123_writereg(state, 0x29, tone & ~0x50);

	/* wait for diseqc queue ready */
	cx24123_wait_for_diseqc(state);

	/* select tone mode */
	cx24123_writereg(state, 0x2a, cx24123_readreg(state, 0x2a) & 0xfb);

	for (i = 0; i < cmd->msg_len; i++)
		cx24123_writereg(state, 0x2C + i, cmd->msg[i]);

	val = cx24123_readreg(state, 0x29);
	cx24123_writereg(state, 0x29, ((val & 0x90) | 0x40) | ((cmd->msg_len-3) & 3));

	/* wait for diseqc message to finish sending */
	cx24123_wait_for_diseqc(state);

	/* restart continuous tone if enabled */
	if (tone & 0x10) {
		cx24123_writereg(state, 0x29, tone & ~0x40);
	}

	return 0;
}

static int cx24123_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t burst)
{
	struct cx24123_state *state = fe->demodulator_priv;
	int val, tone;

	dprintk("%s:\n", __FUNCTION__);

	/* stop continuous tone if enabled */
	tone = cx24123_readreg(state, 0x29);
	if (tone & 0x10)
		cx24123_writereg(state, 0x29, tone & ~0x50);

	/* wait for diseqc queue ready */
	cx24123_wait_for_diseqc(state);

	/* select tone mode */
	cx24123_writereg(state, 0x2a, cx24123_readreg(state, 0x2a) | 0x4);
	msleep(30);
	val = cx24123_readreg(state, 0x29);
	if (burst == SEC_MINI_A)
		cx24123_writereg(state, 0x29, ((val & 0x90) | 0x40 | 0x00));
	else if (burst == SEC_MINI_B)
		cx24123_writereg(state, 0x29, ((val & 0x90) | 0x40 | 0x08));
	else
		return -EINVAL;

	cx24123_wait_for_diseqc(state);
	cx24123_writereg(state, 0x2a, cx24123_readreg(state, 0x2a) & 0xfb);

	/* restart continuous tone if enabled */
	if (tone & 0x10) {
		cx24123_writereg(state, 0x29, tone & ~0x40);
	}
	return 0;
}

static int cx24123_read_status(struct dvb_frontend* fe, fe_status_t* status)
{
	struct cx24123_state *state = fe->demodulator_priv;

	int sync = cx24123_readreg(state, 0x14);
	int lock = cx24123_readreg(state, 0x20);

	*status = 0;
	if (lock & 0x01)
		*status |= FE_HAS_SIGNAL;
	if (sync & 0x02)
		*status |= FE_HAS_CARRIER;	/* Phase locked */
	if (sync & 0x04)
		*status |= FE_HAS_VITERBI;

	/* Reed-Solomon Status */
	if (sync & 0x08)
		*status |= FE_HAS_SYNC;
	if (sync & 0x80)
		*status |= FE_HAS_LOCK;		/*Full Sync */

	return 0;
}

/*
 * Configured to return the measurement of errors in blocks, because no UCBLOCKS value
 * is available, so this value doubles up to satisfy both measurements
 */
static int cx24123_read_ber(struct dvb_frontend* fe, u32* ber)
{
	struct cx24123_state *state = fe->demodulator_priv;

	/* The true bit error rate is this value divided by
	   the window size (set as 256 * 255) */
	*ber = ((cx24123_readreg(state, 0x1c) & 0x3f) << 16) |
		(cx24123_readreg(state, 0x1d) << 8 |
		 cx24123_readreg(state, 0x1e));

	dprintk("%s:  BER = %d\n",__FUNCTION__,*ber);

	return 0;
}

static int cx24123_read_signal_strength(struct dvb_frontend* fe, u16* signal_strength)
{
	struct cx24123_state *state = fe->demodulator_priv;

	*signal_strength = cx24123_readreg(state, 0x3b) << 8; /* larger = better */

	dprintk("%s:  Signal strength = %d\n",__FUNCTION__,*signal_strength);

	return 0;
}

static int cx24123_read_snr(struct dvb_frontend* fe, u16* snr)
{
	struct cx24123_state *state = fe->demodulator_priv;

	/* Inverted raw Es/N0 count, totally bogus but better than the
	   BER threshold. */
	*snr = 65535 - (((u16)cx24123_readreg(state, 0x18) << 8) |
			 (u16)cx24123_readreg(state, 0x19));

	dprintk("%s:  read S/N index = %d\n",__FUNCTION__,*snr);

	return 0;
}

static int cx24123_set_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct cx24123_state *state = fe->demodulator_priv;

	dprintk("%s:  set_frontend\n",__FUNCTION__);

	if (state->config->set_ts_params)
		state->config->set_ts_params(fe, 0);

	state->currentfreq=p->frequency;
	state->currentsymbolrate = p->u.qpsk.symbol_rate;

	cx24123_set_inversion(state, p->inversion);
	cx24123_set_fec(state, p->u.qpsk.fec_inner);
	cx24123_set_symbolrate(state, p->u.qpsk.symbol_rate);
	cx24123_pll_tune(fe, p);

	/* Enable automatic aquisition and reset cycle */
	cx24123_writereg(state, 0x03, (cx24123_readreg(state, 0x03) | 0x07));
	cx24123_writereg(state, 0x00, 0x10);
	cx24123_writereg(state, 0x00, 0);

	return 0;
}

static int cx24123_get_frontend(struct dvb_frontend* fe, struct dvb_frontend_parameters *p)
{
	struct cx24123_state *state = fe->demodulator_priv;

	dprintk("%s:  get_frontend\n",__FUNCTION__);

	if (cx24123_get_inversion(state, &p->inversion) != 0) {
		printk("%s: Failed to get inversion status\n",__FUNCTION__);
		return -EREMOTEIO;
	}
	if (cx24123_get_fec(state, &p->u.qpsk.fec_inner) != 0) {
		printk("%s: Failed to get fec status\n",__FUNCTION__);
		return -EREMOTEIO;
	}
	p->frequency = state->currentfreq;
	p->u.qpsk.symbol_rate = state->currentsymbolrate;

	return 0;
}

static int cx24123_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
{
	struct cx24123_state *state = fe->demodulator_priv;
	u8 val;

	/* wait for diseqc queue ready */
	cx24123_wait_for_diseqc(state);

	val = cx24123_readreg(state, 0x29) & ~0x40;

	switch (tone) {
	case SEC_TONE_ON:
		dprintk("%s: setting tone on\n", __FUNCTION__);
		return cx24123_writereg(state, 0x29, val | 0x10);
	case SEC_TONE_OFF:
		dprintk("%s: setting tone off\n",__FUNCTION__);
		return cx24123_writereg(state, 0x29, val & 0xef);
	default:
		printk("%s: CASE reached default with tone=%d\n", __FUNCTION__, tone);
		return -EINVAL;
	}

	return 0;
}

static int cx24123_tune(struct dvb_frontend* fe,
			struct dvb_frontend_parameters* params,
			unsigned int mode_flags,
			unsigned int *delay,
			fe_status_t *status)
{
	int retval = 0;

	if (params != NULL)
		retval = cx24123_set_frontend(fe, params);

	if (!(mode_flags & FE_TUNE_MODE_ONESHOT))
		cx24123_read_status(fe, status);
	*delay = HZ/10;

	return retval;
}

static int cx24123_get_algo(struct dvb_frontend *fe)
{
	return 1; //FE_ALGO_HW
}

static void cx24123_release(struct dvb_frontend* fe)
{
	struct cx24123_state* state = fe->demodulator_priv;
	dprintk("%s\n",__FUNCTION__);
	kfree(state);
}

static struct dvb_frontend_ops cx24123_ops;

struct dvb_frontend* cx24123_attach(const struct cx24123_config* config,
				    struct i2c_adapter* i2c)
{
	struct cx24123_state* state = NULL;
	int ret;

	dprintk("%s\n",__FUNCTION__);

	/* allocate memory for the internal state */
	state = kmalloc(sizeof(struct cx24123_state), GFP_KERNEL);
	if (state == NULL) {
		printk("Unable to kmalloc\n");
		goto error;
	}

	/* setup the state */
	state->config = config;
	state->i2c = i2c;
	state->VCAarg = 0;
	state->VGAarg = 0;
	state->bandselectarg = 0;
	state->pllarg = 0;
	state->currentfreq = 0;
	state->currentsymbolrate = 0;

	/* check if the demod is there */
	ret = cx24123_readreg(state, 0x00);
	if ((ret != 0xd1) && (ret != 0xe1)) {
		printk("Version != d1 or e1\n");
		goto error;
	}

	/* create dvb_frontend */
	memcpy(&state->frontend.ops, &cx24123_ops, sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;
	return &state->frontend;

error:
	kfree(state);

	return NULL;
}

static struct dvb_frontend_ops cx24123_ops = {

	.info = {
		.name = "Conexant CX24123/CX24109",
		.type = FE_QPSK,
		.frequency_min = 950000,
		.frequency_max = 2150000,
		.frequency_stepsize = 1011, /* kHz for QPSK frontends */
		.frequency_tolerance = 5000,
		.symbol_rate_min = 1000000,
		.symbol_rate_max = 45000000,
		.caps = FE_CAN_INVERSION_AUTO |
			FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
			FE_CAN_FEC_4_5 | FE_CAN_FEC_5_6 | FE_CAN_FEC_6_7 |
			FE_CAN_FEC_7_8 | FE_CAN_FEC_AUTO |
			FE_CAN_QPSK | FE_CAN_RECOVER
	},

	.release = cx24123_release,

	.init = cx24123_initfe,
	.set_frontend = cx24123_set_frontend,
	.get_frontend = cx24123_get_frontend,
	.read_status = cx24123_read_status,
	.read_ber = cx24123_read_ber,
	.read_signal_strength = cx24123_read_signal_strength,
	.read_snr = cx24123_read_snr,
	.diseqc_send_master_cmd = cx24123_send_diseqc_msg,
	.diseqc_send_burst = cx24123_diseqc_send_burst,
	.set_tone = cx24123_set_tone,
	.set_voltage = cx24123_set_voltage,
	.tune = cx24123_tune,
	.get_frontend_algo = cx24123_get_algo,
};

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activates frontend debugging (default:0)");

module_param(force_band, int, 0644);
MODULE_PARM_DESC(force_band, "Force a specific band select (1-9, default:off).");

MODULE_DESCRIPTION("DVB Frontend module for Conexant cx24123/cx24109 hardware");
MODULE_AUTHOR("Steven Toth");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(cx24123_attach);

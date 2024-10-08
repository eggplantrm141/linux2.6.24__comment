/*

   i2c tv tuner chip device driver
   controls the philips tda8290+75 tuner chip combo.

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

   This "tda8290" module was split apart from the original "tuner" module.
*/

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev.h>
#include "tuner-i2c.h"
#include "tda8290.h"

static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "enable verbose debug messages");

#define PREFIX "tda8290 "

/* ---------------------------------------------------------------------- */

struct tda8290_priv {
	struct tuner_i2c_props i2c_props;

	unsigned char tda8290_easy_mode;
	unsigned char tda827x_lpsel;
	unsigned char tda827x_addr;
	unsigned char tda827x_ver;
	unsigned int sgIF;

	u32 frequency;

	unsigned int *lna_cfg;
	int (*tuner_callback) (void *dev, int command,int arg);
};

/* ---------------------------------------------------------------------- */

struct tda827x_data {
	u32 lomax;
	u8  spd;
	u8  bs;
	u8  bp;
	u8  cp;
	u8  gc3;
	u8 div1p5;
};

     /* Note lomax entry is lo / 62500 */

static struct tda827x_data tda827x_analog[] = {
	{ .lomax =   992, .spd = 3, .bs = 2, .bp = 0, .cp = 0, .gc3 = 3, .div1p5 = 1}, /*  62 MHz */
	{ .lomax =  1056, .spd = 3, .bs = 3, .bp = 0, .cp = 0, .gc3 = 3, .div1p5 = 1}, /*  66 MHz */
	{ .lomax =  1216, .spd = 3, .bs = 1, .bp = 0, .cp = 0, .gc3 = 3, .div1p5 = 0}, /*  76 MHz */
	{ .lomax =  1344, .spd = 3, .bs = 2, .bp = 0, .cp = 0, .gc3 = 3, .div1p5 = 0}, /*  84 MHz */
	{ .lomax =  1488, .spd = 3, .bs = 2, .bp = 0, .cp = 0, .gc3 = 1, .div1p5 = 0}, /*  93 MHz */
	{ .lomax =  1568, .spd = 3, .bs = 3, .bp = 0, .cp = 0, .gc3 = 1, .div1p5 = 0}, /*  98 MHz */
	{ .lomax =  1744, .spd = 3, .bs = 3, .bp = 1, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 109 MHz */
	{ .lomax =  1968, .spd = 2, .bs = 2, .bp = 1, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 123 MHz */
	{ .lomax =  2128, .spd = 2, .bs = 3, .bp = 1, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 133 MHz */
	{ .lomax =  2416, .spd = 2, .bs = 1, .bp = 1, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 151 MHz */
	{ .lomax =  2464, .spd = 2, .bs = 2, .bp = 1, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 154 MHz */
	{ .lomax =  2896, .spd = 2, .bs = 2, .bp = 1, .cp = 0, .gc3 = 0, .div1p5 = 0}, /* 181 MHz */
	{ .lomax =  2960, .spd = 2, .bs = 2, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 185 MHz */
	{ .lomax =  3472, .spd = 2, .bs = 3, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 217 MHz */
	{ .lomax =  3904, .spd = 1, .bs = 2, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 244 MHz */
	{ .lomax =  4240, .spd = 1, .bs = 3, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 265 MHz */
	{ .lomax =  4832, .spd = 1, .bs = 1, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 302 MHz */
	{ .lomax =  5184, .spd = 1, .bs = 2, .bp = 2, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 324 MHz */
	{ .lomax =  5920, .spd = 1, .bs = 2, .bp = 3, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 370 MHz */
	{ .lomax =  7264, .spd = 1, .bs = 3, .bp = 3, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 454 MHz */
	{ .lomax =  7888, .spd = 0, .bs = 2, .bp = 3, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 493 MHz */
	{ .lomax =  8480, .spd = 0, .bs = 3, .bp = 3, .cp = 0, .gc3 = 1, .div1p5 = 1}, /* 530 MHz */
	{ .lomax =  8864, .spd = 0, .bs = 1, .bp = 3, .cp = 0, .gc3 = 1, .div1p5 = 0}, /* 554 MHz */
	{ .lomax =  9664, .spd = 0, .bs = 1, .bp = 4, .cp = 0, .gc3 = 0, .div1p5 = 0}, /* 604 MHz */
	{ .lomax = 11088, .spd = 0, .bs = 2, .bp = 4, .cp = 0, .gc3 = 0, .div1p5 = 0}, /* 696 MHz */
	{ .lomax = 11840, .spd = 0, .bs = 2, .bp = 4, .cp = 1, .gc3 = 0, .div1p5 = 0}, /* 740 MHz */
	{ .lomax = 13120, .spd = 0, .bs = 3, .bp = 4, .cp = 0, .gc3 = 0, .div1p5 = 0}, /* 820 MHz */
	{ .lomax = 13840, .spd = 0, .bs = 3, .bp = 4, .cp = 1, .gc3 = 0, .div1p5 = 0}, /* 865 MHz */
	{ .lomax =     0, .spd = 0, .bs = 0, .bp = 0, .cp = 0, .gc3 = 0, .div1p5 = 0}  /* End      */
};

static void tda827x_set_analog_params(struct dvb_frontend *fe,
				      struct analog_parameters *params)
{
	unsigned char tuner_reg[8];
	unsigned char reg2[2];
	u32 N;
	int i;
	struct tda8290_priv *priv = fe->tuner_priv;
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .flags = 0};
	unsigned int freq = params->frequency;

	if (params->mode == V4L2_TUNER_RADIO)
		freq = freq / 1000;

	N = freq + priv->sgIF;
	i = 0;
	while (tda827x_analog[i].lomax < N) {
		if(tda827x_analog[i + 1].lomax == 0)
			break;
		i++;
	}

	N = N << tda827x_analog[i].spd;

	tuner_reg[0] = 0;
	tuner_reg[1] = (unsigned char)(N>>8);
	tuner_reg[2] = (unsigned char) N;
	tuner_reg[3] = 0x40;
	tuner_reg[4] = 0x52 + (priv->tda827x_lpsel << 5);
	tuner_reg[5] = (tda827x_analog[i].spd   << 6) + (tda827x_analog[i].div1p5 <<5) +
		       (tda827x_analog[i].bs     <<3) +  tda827x_analog[i].bp;
	tuner_reg[6] = 0x8f + (tda827x_analog[i].gc3 << 4);
	tuner_reg[7] = 0x8f;

	msg.buf = tuner_reg;
	msg.len = 8;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msg.buf= reg2;
	msg.len = 2;
	reg2[0] = 0x80;
	reg2[1] = 0;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	reg2[0] = 0x60;
	reg2[1] = 0xbf;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	reg2[0] = 0x30;
	reg2[1] = tuner_reg[4] + 0x80;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msleep(1);
	reg2[0] = 0x30;
	reg2[1] = tuner_reg[4] + 4;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msleep(1);
	reg2[0] = 0x30;
	reg2[1] = tuner_reg[4];
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msleep(550);
	reg2[0] = 0x30;
	reg2[1] = (tuner_reg[4] & 0xfc) + tda827x_analog[i].cp ;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	reg2[0] = 0x60;
	reg2[1] = 0x3f;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	reg2[0] = 0x80;
	reg2[1] = 0x08;   // Vsync en
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
}

static void tda827x_agcf(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char data[] = {0x80, 0x0c};
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .buf = data,
			      .flags = 0, .len = 2};
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
}

/* ---------------------------------------------------------------------- */

struct tda827xa_data {
	u32 lomax;
	u8  svco;
	u8  spd;
	u8  scr;
	u8  sbs;
	u8  gc3;
};

static struct tda827xa_data tda827xa_analog[] = {
	{ .lomax =   910, .svco = 3, .spd = 4, .scr = 0, .sbs = 0, .gc3 = 3},  /*  56.875 MHz */
	{ .lomax =  1076, .svco = 0, .spd = 3, .scr = 0, .sbs = 0, .gc3 = 3},  /*  67.25 MHz */
	{ .lomax =  1300, .svco = 1, .spd = 3, .scr = 0, .sbs = 0, .gc3 = 3},  /*  81.25 MHz */
	{ .lomax =  1560, .svco = 2, .spd = 3, .scr = 0, .sbs = 0, .gc3 = 3},  /*  97.5  MHz */
	{ .lomax =  1820, .svco = 3, .spd = 3, .scr = 0, .sbs = 1, .gc3 = 1},  /* 113.75 MHz */
	{ .lomax =  2152, .svco = 0, .spd = 2, .scr = 0, .sbs = 1, .gc3 = 1},  /* 134.5 MHz */
	{ .lomax =  2464, .svco = 1, .spd = 2, .scr = 0, .sbs = 1, .gc3 = 1},  /* 154   MHz */
	{ .lomax =  2600, .svco = 1, .spd = 2, .scr = 0, .sbs = 1, .gc3 = 1},  /* 162.5 MHz */
	{ .lomax =  2928, .svco = 2, .spd = 2, .scr = 0, .sbs = 1, .gc3 = 1},  /* 183   MHz */
	{ .lomax =  3120, .svco = 2, .spd = 2, .scr = 0, .sbs = 2, .gc3 = 1},  /* 195   MHz */
	{ .lomax =  3640, .svco = 3, .spd = 2, .scr = 0, .sbs = 2, .gc3 = 3},  /* 227.5 MHz */
	{ .lomax =  4304, .svco = 0, .spd = 1, .scr = 0, .sbs = 2, .gc3 = 3},  /* 269   MHz */
	{ .lomax =  5200, .svco = 1, .spd = 1, .scr = 0, .sbs = 2, .gc3 = 1},  /* 325   MHz */
	{ .lomax =  6240, .svco = 2, .spd = 1, .scr = 0, .sbs = 3, .gc3 = 3},  /* 390   MHz */
	{ .lomax =  7280, .svco = 3, .spd = 1, .scr = 0, .sbs = 3, .gc3 = 3},  /* 455   MHz */
	{ .lomax =  8320, .svco = 0, .spd = 0, .scr = 0, .sbs = 3, .gc3 = 1},  /* 520   MHz */
	{ .lomax =  8608, .svco = 0, .spd = 0, .scr = 1, .sbs = 3, .gc3 = 1},  /* 538   MHz */
	{ .lomax =  8864, .svco = 1, .spd = 0, .scr = 0, .sbs = 3, .gc3 = 1},  /* 554   MHz */
	{ .lomax =  9920, .svco = 1, .spd = 0, .scr = 0, .sbs = 4, .gc3 = 0},  /* 620   MHz */
	{ .lomax = 10400, .svco = 1, .spd = 0, .scr = 1, .sbs = 4, .gc3 = 0},  /* 650   MHz */
	{ .lomax = 11200, .svco = 2, .spd = 0, .scr = 0, .sbs = 4, .gc3 = 0},  /* 700   MHz */
	{ .lomax = 12480, .svco = 2, .spd = 0, .scr = 1, .sbs = 4, .gc3 = 0},  /* 780   MHz */
	{ .lomax = 13120, .svco = 3, .spd = 0, .scr = 0, .sbs = 4, .gc3 = 0},  /* 820   MHz */
	{ .lomax = 13920, .svco = 3, .spd = 0, .scr = 1, .sbs = 4, .gc3 = 0},  /* 870   MHz */
	{ .lomax = 14576, .svco = 3, .spd = 0, .scr = 2, .sbs = 4, .gc3 = 0},  /* 911   MHz */
	{ .lomax =     0, .svco = 0, .spd = 0, .scr = 0, .sbs = 0, .gc3 = 0}   /* End */
};

static void tda827xa_lna_gain(struct dvb_frontend *fe, int high,
			      struct analog_parameters *params)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char buf[] = {0x22, 0x01};
	int arg;
	struct i2c_msg msg = {.addr = priv->i2c_props.addr, .flags = 0, .buf = buf, .len = sizeof(buf)};

	if ((priv->lna_cfg == NULL)  || (priv->tuner_callback == NULL))
	    return;

	if (*priv->lna_cfg) {
		if (high)
			tuner_dbg("setting LNA to high gain\n");
		else
			tuner_dbg("setting LNA to low gain\n");
	}
	switch (*priv->lna_cfg) {
	case 0: /* no LNA */
		break;
	case 1: /* switch is GPIO 0 of tda8290 */
	case 2:
		/* turn Vsync on */
		if (params->std & V4L2_STD_MN)
			arg = 1;
		else
			arg = 0;
		if (priv->tuner_callback)
			priv->tuner_callback(priv->i2c_props.adap->algo_data, 1, arg);
		buf[1] = high ? 0 : 1;
		if (*priv->lna_cfg == 2)
			buf[1] = high ? 1 : 0;
		i2c_transfer(priv->i2c_props.adap, &msg, 1);
		break;
	case 3: /* switch with GPIO of saa713x */
		if (priv->tuner_callback)
			priv->tuner_callback(priv->i2c_props.adap->algo_data, 0, high);
		break;
	}
}

static void tda827xa_set_analog_params(struct dvb_frontend *fe,
				       struct analog_parameters *params)
{
	unsigned char tuner_reg[11];
	u32 N;
	int i;
	struct tda8290_priv *priv = fe->tuner_priv;
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .flags = 0, .buf = tuner_reg};
	unsigned int freq = params->frequency;

	tda827xa_lna_gain(fe, 1, params);
	msleep(10);

	if (params->mode == V4L2_TUNER_RADIO)
		freq = freq / 1000;

	N = freq + priv->sgIF;
	i = 0;
	while (tda827xa_analog[i].lomax < N) {
		if(tda827xa_analog[i + 1].lomax == 0)
			break;
		i++;
	}

	N = N << tda827xa_analog[i].spd;

	tuner_reg[0] = 0;
	tuner_reg[1] = (unsigned char)(N>>8);
	tuner_reg[2] = (unsigned char) N;
	tuner_reg[3] = 0;
	tuner_reg[4] = 0x16;
	tuner_reg[5] = (tda827xa_analog[i].spd << 5) + (tda827xa_analog[i].svco << 3) +
			tda827xa_analog[i].sbs;
	tuner_reg[6] = 0x8b + (tda827xa_analog[i].gc3 << 4);
	tuner_reg[7] = 0x1c;
	tuner_reg[8] = 4;
	tuner_reg[9] = 0x20;
	tuner_reg[10] = 0x00;
	msg.len = 11;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0x90;
	tuner_reg[1] = 0xff;
	tuner_reg[2] = 0xe0;
	tuner_reg[3] = 0;
	tuner_reg[4] = 0x99 + (priv->tda827x_lpsel << 1);
	msg.len = 5;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0xa0;
	tuner_reg[1] = 0xc0;
	msg.len = 2;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0x30;
	tuner_reg[1] = 0x10 + tda827xa_analog[i].scr;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msg.flags = I2C_M_RD;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
	msg.flags = 0;
	tuner_reg[1] >>= 4;
	tuner_dbg("AGC2 gain is: %d\n", tuner_reg[1]);
	if (tuner_reg[1] < 1)
		tda827xa_lna_gain(fe, 0, params);

	msleep(100);
	tuner_reg[0] = 0x60;
	tuner_reg[1] = 0x3c;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	msleep(163);
	tuner_reg[0] = 0x50;
	tuner_reg[1] = 0x8f + (tda827xa_analog[i].gc3 << 4);
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0x80;
	tuner_reg[1] = 0x28;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0xb0;
	tuner_reg[1] = 0x01;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);

	tuner_reg[0] = 0xc0;
	tuner_reg[1] = 0x19 + (priv->tda827x_lpsel << 1);
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
}

static void tda827xa_agcf(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char data[] = {0x80, 0x2c};
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .buf = data,
			      .flags = 0, .len = 2};
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
}

/*---------------------------------------------------------------------*/

static void tda8290_i2c_bridge(struct dvb_frontend *fe, int close)
{
	struct tda8290_priv *priv = fe->tuner_priv;

	unsigned char  enable[2] = { 0x21, 0xC0 };
	unsigned char disable[2] = { 0x21, 0x00 };
	unsigned char *msg;
	if(close) {
		msg = enable;
		tuner_i2c_xfer_send(&priv->i2c_props, msg, 2);
		/* let the bridge stabilize */
		msleep(20);
	} else {
		msg = disable;
		tuner_i2c_xfer_send(&priv->i2c_props, msg, 2);
	}
}

/*---------------------------------------------------------------------*/

static void set_audio(struct dvb_frontend *fe,
		      struct analog_parameters *params)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	char* mode;

	priv->tda827x_lpsel = 0;
	if (params->std & V4L2_STD_MN) {
		priv->sgIF = 92;
		priv->tda8290_easy_mode = 0x01;
		priv->tda827x_lpsel = 1;
		mode = "MN";
	} else if (params->std & V4L2_STD_B) {
		priv->sgIF = 108;
		priv->tda8290_easy_mode = 0x02;
		mode = "B";
	} else if (params->std & V4L2_STD_GH) {
		priv->sgIF = 124;
		priv->tda8290_easy_mode = 0x04;
		mode = "GH";
	} else if (params->std & V4L2_STD_PAL_I) {
		priv->sgIF = 124;
		priv->tda8290_easy_mode = 0x08;
		mode = "I";
	} else if (params->std & V4L2_STD_DK) {
		priv->sgIF = 124;
		priv->tda8290_easy_mode = 0x10;
		mode = "DK";
	} else if (params->std & V4L2_STD_SECAM_L) {
		priv->sgIF = 124;
		priv->tda8290_easy_mode = 0x20;
		mode = "L";
	} else if (params->std & V4L2_STD_SECAM_LC) {
		priv->sgIF = 20;
		priv->tda8290_easy_mode = 0x40;
		mode = "LC";
	} else {
		priv->sgIF = 124;
		priv->tda8290_easy_mode = 0x10;
		mode = "xx";
	}

	if (params->mode == V4L2_TUNER_RADIO)
		priv->sgIF = 88; /* if frequency is 5.5 MHz */

	tuner_dbg("setting tda8290 to system %s\n", mode);
}

static int tda8290_set_params(struct dvb_frontend *fe,
			      struct analog_parameters *params)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char soft_reset[]  = { 0x00, 0x00 };
	unsigned char easy_mode[]   = { 0x01, priv->tda8290_easy_mode };
	unsigned char expert_mode[] = { 0x01, 0x80 };
	unsigned char agc_out_on[]  = { 0x02, 0x00 };
	unsigned char gainset_off[] = { 0x28, 0x14 };
	unsigned char if_agc_spd[]  = { 0x0f, 0x88 };
	unsigned char adc_head_6[]  = { 0x05, 0x04 };
	unsigned char adc_head_9[]  = { 0x05, 0x02 };
	unsigned char adc_head_12[] = { 0x05, 0x01 };
	unsigned char pll_bw_nom[]  = { 0x0d, 0x47 };
	unsigned char pll_bw_low[]  = { 0x0d, 0x27 };
	unsigned char gainset_2[]   = { 0x28, 0x64 };
	unsigned char agc_rst_on[]  = { 0x0e, 0x0b };
	unsigned char agc_rst_off[] = { 0x0e, 0x09 };
	unsigned char if_agc_set[]  = { 0x0f, 0x81 };
	unsigned char addr_adc_sat  = 0x1a;
	unsigned char addr_agc_stat = 0x1d;
	unsigned char addr_pll_stat = 0x1b;
	unsigned char adc_sat, agc_stat,
		      pll_stat;
	int i;

	set_audio(fe, params);

	if (priv->lna_cfg)
		tuner_dbg("tda827xa config is 0x%02x\n", *priv->lna_cfg);
	tuner_i2c_xfer_send(&priv->i2c_props, easy_mode, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, agc_out_on, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, soft_reset, 2);
	msleep(1);

	expert_mode[1] = priv->tda8290_easy_mode + 0x80;
	tuner_i2c_xfer_send(&priv->i2c_props, expert_mode, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, gainset_off, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, if_agc_spd, 2);
	if (priv->tda8290_easy_mode & 0x60)
		tuner_i2c_xfer_send(&priv->i2c_props, adc_head_9, 2);
	else
		tuner_i2c_xfer_send(&priv->i2c_props, adc_head_6, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, pll_bw_nom, 2);

	tda8290_i2c_bridge(fe, 1);
	if (priv->tda827x_ver != 0)
		tda827xa_set_analog_params(fe, params);
	else
		tda827x_set_analog_params(fe, params);
	for (i = 0; i < 3; i++) {
		tuner_i2c_xfer_send(&priv->i2c_props, &addr_pll_stat, 1);
		tuner_i2c_xfer_recv(&priv->i2c_props, &pll_stat, 1);
		if (pll_stat & 0x80) {
			tuner_i2c_xfer_send(&priv->i2c_props, &addr_adc_sat, 1);
			tuner_i2c_xfer_recv(&priv->i2c_props, &adc_sat, 1);
			tuner_i2c_xfer_send(&priv->i2c_props, &addr_agc_stat, 1);
			tuner_i2c_xfer_recv(&priv->i2c_props, &agc_stat, 1);
			tuner_dbg("tda8290 is locked, AGC: %d\n", agc_stat);
			break;
		} else {
			tuner_dbg("tda8290 not locked, no signal?\n");
			msleep(100);
		}
	}
	/* adjust headroom resp. gain */
	if ((agc_stat > 115) || (!(pll_stat & 0x80) && (adc_sat < 20))) {
		tuner_dbg("adjust gain, step 1. Agc: %d, ADC stat: %d, lock: %d\n",
			   agc_stat, adc_sat, pll_stat & 0x80);
		tuner_i2c_xfer_send(&priv->i2c_props, gainset_2, 2);
		msleep(100);
		tuner_i2c_xfer_send(&priv->i2c_props, &addr_agc_stat, 1);
		tuner_i2c_xfer_recv(&priv->i2c_props, &agc_stat, 1);
		tuner_i2c_xfer_send(&priv->i2c_props, &addr_pll_stat, 1);
		tuner_i2c_xfer_recv(&priv->i2c_props, &pll_stat, 1);
		if ((agc_stat > 115) || !(pll_stat & 0x80)) {
			tuner_dbg("adjust gain, step 2. Agc: %d, lock: %d\n",
				   agc_stat, pll_stat & 0x80);
			if (priv->tda827x_ver != 0)
				tda827xa_agcf(fe);
			else
				tda827x_agcf(fe);
			msleep(100);
			tuner_i2c_xfer_send(&priv->i2c_props, &addr_agc_stat, 1);
			tuner_i2c_xfer_recv(&priv->i2c_props, &agc_stat, 1);
			tuner_i2c_xfer_send(&priv->i2c_props, &addr_pll_stat, 1);
			tuner_i2c_xfer_recv(&priv->i2c_props, &pll_stat, 1);
			if((agc_stat > 115) || !(pll_stat & 0x80)) {
				tuner_dbg("adjust gain, step 3. Agc: %d\n", agc_stat);
				tuner_i2c_xfer_send(&priv->i2c_props, adc_head_12, 2);
				tuner_i2c_xfer_send(&priv->i2c_props, pll_bw_low, 2);
				msleep(100);
			}
		}
	}

	/* l/ l' deadlock? */
	if(priv->tda8290_easy_mode & 0x60) {
		tuner_i2c_xfer_send(&priv->i2c_props, &addr_adc_sat, 1);
		tuner_i2c_xfer_recv(&priv->i2c_props, &adc_sat, 1);
		tuner_i2c_xfer_send(&priv->i2c_props, &addr_pll_stat, 1);
		tuner_i2c_xfer_recv(&priv->i2c_props, &pll_stat, 1);
		if ((adc_sat > 20) || !(pll_stat & 0x80)) {
			tuner_dbg("trying to resolve SECAM L deadlock\n");
			tuner_i2c_xfer_send(&priv->i2c_props, agc_rst_on, 2);
			msleep(40);
			tuner_i2c_xfer_send(&priv->i2c_props, agc_rst_off, 2);
		}
	}

	tda8290_i2c_bridge(fe, 0);
	tuner_i2c_xfer_send(&priv->i2c_props, if_agc_set, 2);

	priv->frequency = (V4L2_TUNER_RADIO == params->mode) ?
		params->frequency * 125 / 2 : params->frequency * 62500;

	return 0;
}

/*---------------------------------------------------------------------*/

static int tda8290_has_signal(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	int ret;

	unsigned char i2c_get_afc[1] = { 0x1B };
	unsigned char afc = 0;

	/* for now, report based on afc status */
	tuner_i2c_xfer_send(&priv->i2c_props, i2c_get_afc, ARRAY_SIZE(i2c_get_afc));
	tuner_i2c_xfer_recv(&priv->i2c_props, &afc, 1);

	ret = (afc & 0x80) ? 65535 : 0;

	tuner_dbg("AFC status: %d\n", ret);

	return ret;
}

static int tda8290_get_status(struct dvb_frontend *fe, u32 *status)
{
	*status = 0;

	if (tda8290_has_signal(fe))
		*status = TUNER_STATUS_LOCKED;

	return 0;
}

static int tda8290_get_rf_strength(struct dvb_frontend *fe, u16 *strength)
{
	*strength = tda8290_has_signal(fe);

	return 0;
}

/*---------------------------------------------------------------------*/

static int tda8290_standby(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char cb1[] = { 0x30, 0xD0 };
	unsigned char tda8290_standby[] = { 0x00, 0x02 };
	unsigned char tda8290_agc_tri[] = { 0x02, 0x20 };
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .flags=0, .buf=cb1, .len = 2};

	tda8290_i2c_bridge(fe, 1);
	if (priv->tda827x_ver != 0)
		cb1[1] = 0x90;
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
	tda8290_i2c_bridge(fe, 0);
	tuner_i2c_xfer_send(&priv->i2c_props, tda8290_agc_tri, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, tda8290_standby, 2);

	return 0;
}


static void tda8290_init_if(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;

	unsigned char set_VS[] = { 0x30, 0x6F };
	unsigned char set_GP00_CF[] = { 0x20, 0x01 };
	unsigned char set_GP01_CF[] = { 0x20, 0x0B };

	if ((priv->lna_cfg) &&
	    ((*priv->lna_cfg == 1) || (*priv->lna_cfg == 2)))
		tuner_i2c_xfer_send(&priv->i2c_props, set_GP00_CF, 2);
	else
		tuner_i2c_xfer_send(&priv->i2c_props, set_GP01_CF, 2);
	tuner_i2c_xfer_send(&priv->i2c_props, set_VS, 2);
}

static void tda8290_init_tuner(struct dvb_frontend *fe)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	unsigned char tda8275_init[]  = { 0x00, 0x00, 0x00, 0x40, 0xdC, 0x04, 0xAf,
					  0x3F, 0x2A, 0x04, 0xFF, 0x00, 0x00, 0x40 };
	unsigned char tda8275a_init[] = { 0x00, 0x00, 0x00, 0x00, 0xdC, 0x05, 0x8b,
					  0x0c, 0x04, 0x20, 0xFF, 0x00, 0x00, 0x4b };
	struct i2c_msg msg = {.addr = priv->tda827x_addr, .flags=0,
			      .buf=tda8275_init, .len = 14};
	if (priv->tda827x_ver != 0)
		msg.buf = tda8275a_init;

	tda8290_i2c_bridge(fe, 1);
	i2c_transfer(priv->i2c_props.adap, &msg, 1);
	tda8290_i2c_bridge(fe, 0);
}

/*---------------------------------------------------------------------*/

static int tda8290_release(struct dvb_frontend *fe)
{
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;

	return 0;
}

static int tda8290_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct tda8290_priv *priv = fe->tuner_priv;
	*frequency = priv->frequency;
	return 0;
}

static struct dvb_tuner_ops tda8290_tuner_ops = {
	.sleep             = tda8290_standby,
	.set_analog_params = tda8290_set_params,
	.release           = tda8290_release,
	.get_frequency     = tda8290_get_frequency,
	.get_status        = tda8290_get_status,
	.get_rf_strength   = tda8290_get_rf_strength,
};

struct dvb_frontend *tda8290_attach(struct dvb_frontend *fe,
				    struct i2c_adapter* i2c_adap,
				    u8 i2c_addr,
				    struct tda8290_config *cfg)
{
	struct tda8290_priv *priv = NULL;
	u8 data;
	int i, ret, tuners_found;
	u32 tuner_addrs;
	struct i2c_msg msg = {.flags=I2C_M_RD, .buf=&data, .len = 1};

	priv = kzalloc(sizeof(struct tda8290_priv), GFP_KERNEL);
	if (priv == NULL)
		return NULL;
	fe->tuner_priv = priv;

	priv->i2c_props.addr = i2c_addr;
	priv->i2c_props.adap = i2c_adap;
	if (cfg) {
		priv->lna_cfg        = cfg->lna_cfg;
		priv->tuner_callback = cfg->tuner_callback;
	}

	tda8290_i2c_bridge(fe, 1);
	/* probe for tuner chip */
	tuners_found = 0;
	tuner_addrs = 0;
	for (i=0x60; i<= 0x63; i++) {
		msg.addr = i;
		ret = i2c_transfer(priv->i2c_props.adap, &msg, 1);
		if (ret == 1) {
			tuners_found++;
			tuner_addrs = (tuner_addrs << 8) + i;
		}
	}
	/* if there is more than one tuner, we expect the right one is
	   behind the bridge and we choose the highest address that doesn't
	   give a response now
	 */
	tda8290_i2c_bridge(fe, 0);
	if(tuners_found > 1)
		for (i = 0; i < tuners_found; i++) {
			msg.addr = tuner_addrs  & 0xff;
			ret = i2c_transfer(priv->i2c_props.adap, &msg, 1);
			if(ret == 1)
				tuner_addrs = tuner_addrs >> 8;
			else
				break;
		}
	if (tuner_addrs == 0) {
		tuner_addrs = 0x61;
		tuner_info("could not clearly identify tuner address, defaulting to %x\n",
			     tuner_addrs);
	} else {
		tuner_addrs = tuner_addrs & 0xff;
		tuner_info("setting tuner address to %x\n", tuner_addrs);
	}
	priv->tda827x_addr = tuner_addrs;
	msg.addr = tuner_addrs;

	tda8290_i2c_bridge(fe, 1);
	ret = i2c_transfer(priv->i2c_props.adap, &msg, 1);
	if( ret != 1)
		tuner_warn("TDA827x access failed!\n");

	memcpy(&fe->ops.tuner_ops, &tda8290_tuner_ops,
	       sizeof(struct dvb_tuner_ops));

	if ((data & 0x3c) == 0) {
		strlcpy(fe->ops.tuner_ops.info.name, "tda8290+75",
			sizeof(fe->ops.tuner_ops.info.name));
		fe->ops.tuner_ops.info.frequency_min  =  55000000;
		fe->ops.tuner_ops.info.frequency_max  = 860000000;
		fe->ops.tuner_ops.info.frequency_step =    250000;
		priv->tda827x_ver = 0;
	} else {
		strlcpy(fe->ops.tuner_ops.info.name, "tda8290+75a",
			sizeof(fe->ops.tuner_ops.info.name));
		fe->ops.tuner_ops.info.frequency_min  =  44000000;
		fe->ops.tuner_ops.info.frequency_max  = 906000000;
		fe->ops.tuner_ops.info.frequency_step =     62500;
		priv->tda827x_ver = 2;
	}

	priv->tda827x_lpsel = 0;

	tda8290_init_tuner(fe);
	tda8290_init_if(fe);
	return fe;
}

int tda8290_probe(struct i2c_adapter* i2c_adap, u8 i2c_addr)
{
	struct tuner_i2c_props i2c_props = {
		.adap = i2c_adap,
		.addr = i2c_addr
	};

	unsigned char soft_reset[]   = { 0x00, 0x00 };
	unsigned char easy_mode_b[]  = { 0x01, 0x02 };
	unsigned char easy_mode_g[]  = { 0x01, 0x04 };
	unsigned char restore_9886[] = { 0x00, 0xd6, 0x30 };
	unsigned char addr_dto_lsb = 0x07;
	unsigned char data;

	tuner_i2c_xfer_send(&i2c_props, easy_mode_b, 2);
	tuner_i2c_xfer_send(&i2c_props, soft_reset, 2);
	tuner_i2c_xfer_send(&i2c_props, &addr_dto_lsb, 1);
	tuner_i2c_xfer_recv(&i2c_props, &data, 1);
	if (data == 0) {
		tuner_i2c_xfer_send(&i2c_props, easy_mode_g, 2);
		tuner_i2c_xfer_send(&i2c_props, soft_reset, 2);
		tuner_i2c_xfer_send(&i2c_props, &addr_dto_lsb, 1);
		tuner_i2c_xfer_recv(&i2c_props, &data, 1);
		if (data == 0x7b) {
			return 0;
		}
	}
	tuner_i2c_xfer_send(&i2c_props, restore_9886, 3);
	return -1;
}

EXPORT_SYMBOL_GPL(tda8290_probe);
EXPORT_SYMBOL_GPL(tda8290_attach);

MODULE_DESCRIPTION("Philips TDA8290 + TDA8275 / TDA8275a tuner driver");
MODULE_AUTHOR("Gerd Knorr, Hartmut Hackmann");
MODULE_LICENSE("GPL");

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-basic-offset: 8
 * End:
 */

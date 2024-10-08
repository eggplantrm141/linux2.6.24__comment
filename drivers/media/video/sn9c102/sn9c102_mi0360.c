/***************************************************************************
 * Plug-in for MI-0360 image sensor connected to the SN9C1xx PC Camera     *
 * Controllers                                                             *
 *                                                                         *
 * Copyright (C) 2007 by Luca Risolia <luca.risolia@studio.unibo.it>       *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#include "sn9c102_sensor.h"


static int mi0360_init(struct sn9c102_device* cam)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	int err = 0;

	switch (sn9c102_get_bridge(cam)) {
	case BRIDGE_SN9C103:
		err = sn9c102_write_const_regs(cam, {0x00, 0x10}, {0x00, 0x11},
					       {0x0a, 0x14}, {0x40, 0x01},
					       {0x20, 0x17}, {0x07, 0x18},
					       {0xa0, 0x19}, {0x02, 0x1c},
					       {0x03, 0x1d}, {0x0f, 0x1e},
					       {0x0c, 0x1f}, {0x00, 0x20},
					       {0x10, 0x21}, {0x20, 0x22},
					       {0x30, 0x23}, {0x40, 0x24},
					       {0x50, 0x25}, {0x60, 0x26},
					       {0x70, 0x27}, {0x80, 0x28},
					       {0x90, 0x29}, {0xa0, 0x2a},
					       {0xb0, 0x2b}, {0xc0, 0x2c},
					       {0xd0, 0x2d}, {0xe0, 0x2e},
					       {0xf0, 0x2f}, {0xff, 0x30});
		break;
	case BRIDGE_SN9C105:
	case BRIDGE_SN9C120:
		err = sn9c102_write_const_regs(cam, {0x44, 0x01}, {0x40, 0x02},
					       {0x00, 0x03}, {0x1a, 0x04},
					       {0x50, 0x05}, {0x20, 0x06},
					       {0x10, 0x07}, {0x03, 0x10},
					       {0x08, 0x14}, {0xa2, 0x17},
					       {0x47, 0x18}, {0x00, 0x19},
					       {0x1d, 0x1a}, {0x10, 0x1b},
					       {0x02, 0x1c}, {0x03, 0x1d},
					       {0x0f, 0x1e}, {0x0c, 0x1f},
					       {0x00, 0x20}, {0x29, 0x21},
					       {0x40, 0x22}, {0x54, 0x23},
					       {0x66, 0x24}, {0x76, 0x25},
					       {0x85, 0x26}, {0x94, 0x27},
					       {0xa1, 0x28}, {0xae, 0x29},
					       {0xbb, 0x2a}, {0xc7, 0x2b},
					       {0xd3, 0x2c}, {0xde, 0x2d},
					       {0xea, 0x2e}, {0xf4, 0x2f},
					       {0xff, 0x30}, {0x00, 0x3F},
					       {0xC7, 0x40}, {0x01, 0x41},
					       {0x44, 0x42}, {0x00, 0x43},
					       {0x44, 0x44}, {0x00, 0x45},
					       {0x44, 0x46}, {0x00, 0x47},
					       {0xC7, 0x48}, {0x01, 0x49},
					       {0xC7, 0x4A}, {0x01, 0x4B},
					       {0xC7, 0x4C}, {0x01, 0x4D},
					       {0x44, 0x4E}, {0x00, 0x4F},
					       {0x44, 0x50}, {0x00, 0x51},
					       {0x44, 0x52}, {0x00, 0x53},
					       {0xC7, 0x54}, {0x01, 0x55},
					       {0xC7, 0x56}, {0x01, 0x57},
					       {0xC7, 0x58}, {0x01, 0x59},
					       {0x44, 0x5A}, {0x00, 0x5B},
					       {0x44, 0x5C}, {0x00, 0x5D},
					       {0x44, 0x5E}, {0x00, 0x5F},
					       {0xC7, 0x60}, {0x01, 0x61},
					       {0xC7, 0x62}, {0x01, 0x63},
					       {0xC7, 0x64}, {0x01, 0x65},
					       {0x44, 0x66}, {0x00, 0x67},
					       {0x44, 0x68}, {0x00, 0x69},
					       {0x44, 0x6A}, {0x00, 0x6B},
					       {0xC7, 0x6C}, {0x01, 0x6D},
					       {0xC7, 0x6E}, {0x01, 0x6F},
					       {0xC7, 0x70}, {0x01, 0x71},
					       {0x44, 0x72}, {0x00, 0x73},
					       {0x44, 0x74}, {0x00, 0x75},
					       {0x44, 0x76}, {0x00, 0x77},
					       {0xC7, 0x78}, {0x01, 0x79},
					       {0xC7, 0x7A}, {0x01, 0x7B},
					       {0xC7, 0x7C}, {0x01, 0x7D},
					       {0x44, 0x7E}, {0x00, 0x7F},
					       {0x14, 0x84}, {0x00, 0x85},
					       {0x27, 0x86}, {0x00, 0x87},
					       {0x07, 0x88}, {0x00, 0x89},
					       {0xEC, 0x8A}, {0x0f, 0x8B},
					       {0xD8, 0x8C}, {0x0f, 0x8D},
					       {0x3D, 0x8E}, {0x00, 0x8F},
					       {0x3D, 0x90}, {0x00, 0x91},
					       {0xCD, 0x92}, {0x0f, 0x93},
					       {0xf7, 0x94}, {0x0f, 0x95},
					       {0x0C, 0x96}, {0x00, 0x97},
					       {0x00, 0x98}, {0x66, 0x99},
					       {0x05, 0x9A}, {0x00, 0x9B},
					       {0x04, 0x9C}, {0x00, 0x9D},
					       {0x08, 0x9E}, {0x00, 0x9F},
					       {0x2D, 0xC0}, {0x2D, 0xC1},
					       {0x3A, 0xC2}, {0x05, 0xC3},
					       {0x04, 0xC4}, {0x3F, 0xC5},
					       {0x00, 0xC6}, {0x00, 0xC7},
					       {0x50, 0xC8}, {0x3C, 0xC9},
					       {0x28, 0xCA}, {0xD8, 0xCB},
					       {0x14, 0xCC}, {0xEC, 0xCD},
					       {0x32, 0xCE}, {0xDD, 0xCF},
					       {0x32, 0xD0}, {0xDD, 0xD1},
					       {0x6A, 0xD2}, {0x50, 0xD3},
					       {0x00, 0xD4}, {0x00, 0xD5},
					       {0x00, 0xD6});
		break;
	default:
		break;
	}

	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x0d,
					 0x00, 0x01, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x0d,
					 0x00, 0x00, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x03,
					 0x01, 0xe1, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x04,
					 0x02, 0x81, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x05,
					 0x00, 0x17, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x06,
					 0x00, 0x11, 0, 0);
	err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id, 0x62,
					 0x04, 0x9a, 0, 0);

	return err;
}


static int mi0360_get_ctrl(struct sn9c102_device* cam,
			   struct v4l2_control* ctrl)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	u8 data[2];

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x09, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[0];
		return 0;
	case V4L2_CID_GAIN:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x35, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1];
		return 0;
	case V4L2_CID_RED_BALANCE:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x2c, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1];
		return 0;
	case V4L2_CID_BLUE_BALANCE:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x2d, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1];
		return 0;
	case SN9C102_V4L2_CID_GREEN_BALANCE:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x2e, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1];
		return 0;
	case V4L2_CID_HFLIP:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x20, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1] & 0x20 ? 1 : 0;
		return 0;
	case V4L2_CID_VFLIP:
		if (sn9c102_i2c_try_raw_read(cam, s, s->i2c_slave_id, 0x20, 2,
					     data) < 0)
			return -EIO;
		ctrl->value = data[1] & 0x80 ? 1 : 0;
		return 0;
	default:
		return -EINVAL;
	}

	return 0;
}


static int mi0360_set_ctrl(struct sn9c102_device* cam,
			   const struct v4l2_control* ctrl)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x09, ctrl->value, 0x00,
						 0, 0);
		break;
	case V4L2_CID_GAIN:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x35, 0x03, ctrl->value,
						 0, 0);
		break;
	case V4L2_CID_RED_BALANCE:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x2c, 0x03, ctrl->value,
						 0, 0);
		break;
	case V4L2_CID_BLUE_BALANCE:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x2d, 0x03, ctrl->value,
						 0, 0);
		break;
	case SN9C102_V4L2_CID_GREEN_BALANCE:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x2b, 0x03, ctrl->value,
						 0, 0);
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x2e, 0x03, ctrl->value,
						 0, 0);
		break;
	case V4L2_CID_HFLIP:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x20, ctrl->value ? 0x40:0x00,
						 ctrl->value ? 0x20:0x00,
						 0, 0);
		break;
	case V4L2_CID_VFLIP:
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x20, ctrl->value ? 0x80:0x00,
						 ctrl->value ? 0x80:0x00,
						 0, 0);
		break;
	default:
		return -EINVAL;
	}

	return err ? -EIO : 0;
}


static int mi0360_set_crop(struct sn9c102_device* cam,
			    const struct v4l2_rect* rect)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	int err = 0;
	u8 h_start = 0, v_start = (u8)(rect->top - s->cropcap.bounds.top) + 1;

	switch (sn9c102_get_bridge(cam)) {
	case BRIDGE_SN9C103:
		h_start = (u8)(rect->left - s->cropcap.bounds.left) + 0;
		break;
	case BRIDGE_SN9C105:
	case BRIDGE_SN9C120:
		h_start = (u8)(rect->left - s->cropcap.bounds.left) + 1;
		break;
	default:
		break;
	}

	err += sn9c102_write_reg(cam, h_start, 0x12);
	err += sn9c102_write_reg(cam, v_start, 0x13);

	return err;
}


static int mi0360_set_pix_format(struct sn9c102_device* cam,
				 const struct v4l2_pix_format* pix)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	int err = 0;

	if (pix->pixelformat == V4L2_PIX_FMT_SBGGR8) {
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x0a, 0x00, 0x05, 0, 0);
		err += sn9c102_write_reg(cam, 0x60, 0x19);
		if (sn9c102_get_bridge(cam) == BRIDGE_SN9C105 ||
		    sn9c102_get_bridge(cam) == BRIDGE_SN9C120)
			err += sn9c102_write_reg(cam, 0xa6, 0x17);
	} else {
		err += sn9c102_i2c_try_raw_write(cam, s, 4, s->i2c_slave_id,
						 0x0a, 0x00, 0x02, 0, 0);
		err += sn9c102_write_reg(cam, 0x20, 0x19);
		if (sn9c102_get_bridge(cam) == BRIDGE_SN9C105 ||
		    sn9c102_get_bridge(cam) == BRIDGE_SN9C120)
			err += sn9c102_write_reg(cam, 0xa2, 0x17);
	}

	return err;
}


static const struct sn9c102_sensor mi0360 = {
	.name = "MI-0360",
	.maintainer = "Luca Risolia <luca.risolia@studio.unibo.it>",
	.supported_bridge = BRIDGE_SN9C103 | BRIDGE_SN9C105 | BRIDGE_SN9C120,
	.frequency = SN9C102_I2C_100KHZ,
	.interface = SN9C102_I2C_2WIRES,
	.i2c_slave_id = 0x5d,
	.init = &mi0360_init,
	.qctrl = {
		{
			.id = V4L2_CID_EXPOSURE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure",
			.minimum = 0x00,
			.maximum = 0x0f,
			.step = 0x01,
			.default_value = 0x05,
			.flags = 0,
		},
		{
			.id = V4L2_CID_GAIN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "global gain",
			.minimum = 0x00,
			.maximum = 0x7f,
			.step = 0x01,
			.default_value = 0x25,
			.flags = 0,
		},
		{
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "horizontal mirror",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		{
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "vertical mirror",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		{
			.id = V4L2_CID_BLUE_BALANCE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "blue balance",
			.minimum = 0x00,
			.maximum = 0x7f,
			.step = 0x01,
			.default_value = 0x0f,
			.flags = 0,
		},
		{
			.id = V4L2_CID_RED_BALANCE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "red balance",
			.minimum = 0x00,
			.maximum = 0x7f,
			.step = 0x01,
			.default_value = 0x32,
			.flags = 0,
		},
		{
			.id = SN9C102_V4L2_CID_GREEN_BALANCE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "green balance",
			.minimum = 0x00,
			.maximum = 0x7f,
			.step = 0x01,
			.default_value = 0x25,
			.flags = 0,
		},
	},
	.get_ctrl = &mi0360_get_ctrl,
	.set_ctrl = &mi0360_set_ctrl,
	.cropcap = {
		.bounds = {
			.left = 0,
			.top = 0,
			.width = 640,
			.height = 480,
		},
		.defrect = {
			.left = 0,
			.top = 0,
			.width = 640,
			.height = 480,
		},
	},
	.set_crop = &mi0360_set_crop,
	.pix_format = {
		.width = 640,
		.height = 480,
		.pixelformat = V4L2_PIX_FMT_SBGGR8,
		.priv = 8,
	},
	.set_pix_format = &mi0360_set_pix_format
};


int sn9c102_probe_mi0360(struct sn9c102_device* cam)
{

	u8 data[2];

	switch (sn9c102_get_bridge(cam)) {
	case BRIDGE_SN9C103:
		if (sn9c102_write_const_regs(cam, {0x01, 0x01}, {0x00, 0x01},
					     {0x28, 0x17}))
			return -EIO;
		break;
	case BRIDGE_SN9C105:
	case BRIDGE_SN9C120:
		if (sn9c102_write_const_regs(cam, {0x01, 0xf1}, {0x00, 0xf1},
					     {0x01, 0x01}, {0x00, 0x01},
					     {0x28, 0x17}))
			return -EIO;
		break;
	default:
		break;
	}

	if (sn9c102_i2c_try_raw_read(cam, &mi0360, mi0360.i2c_slave_id, 0x00,
				     2, data) < 0)
		return -EIO;

	if (data[0] != 0x82 || data[1] != 0x43)
		return -ENODEV;

	sn9c102_attach_sensor(cam, &mi0360);

	return 0;
}

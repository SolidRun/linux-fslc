/*
 * Copyright 2009-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @defgroup Framebuffer Framebuffer Driver for SDC and ADC.
 */

/*!
 * @file mxc_edid.c
 *
 * @brief MXC EDID driver
 *
 * @ingroup Framebuffer
 */

/*!
 * Include files
 */
#include <linux/i2c.h>
#include <linux/fb.h>
#include <video/mxc_edid.h>
#include "../edid.h"

#undef DEBUG  /* define this for verbose EDID parsing output */
#ifdef DEBUG
#define DPRINTK(fmt, args...) printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

const struct fb_videomode mxc_cea_mode[64] = {
	/* #1: 640x480p@59.94/60Hz */
	[1] = {
		NULL, 60, 640, 480, 39722, 48, 16, 33, 10, 96, 2, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #2: 720x480p@59.94/60Hz 4:3 */
	[2] = {
		NULL, 60, 720, 480, 37037, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #3: 720x480p@59.94/60Hz 16:9 */
	[3] = {
		NULL, 60, 720, 480, 37037, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #4: 1280x720p@59.94/60Hz */
	[4] = {
		NULL, 60, 1280, 720, 13468, 220, 110, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #5: 1920x1080i@59.94/60Hz */
	[5] = {
		NULL, 60, 1920, 1080, 13468, 148, 88, 31, 4, 44, 10,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #6: 720(1440)x480iH@59.94/60Hz 4:3 */
	[6] = {
		NULL, 60, 1440, 480, 37037, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #7: 720(1440)x480iH@59.94/60Hz 16:9 */
	[7] = {
		NULL, 60, 1440, 480, 37037, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #8: 720(1440)x240pH@59.94/60Hz 4:3 */
	[8] = {
		NULL, 60, 1440, 240, 37037, 114, 38, 15, 4, 124, 3, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #9: 720(1440)x240pH@59.94/60Hz 16:9 */
	[9] = {
		NULL, 60, 1440, 240, 37037, 114, 38, 15, 4, 124, 3, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #14: 1440x480p@59.94/60Hz 4:3 */
	[14] = {
		NULL, 60, 1440, 480, 18518, 120, 32, 30, 9, 124, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #15: 1440x480p@59.94/60Hz 16:9 */
	[15] = {
		NULL, 60, 1440, 480, 18518, 120, 32, 30, 9, 124, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #16: 1920x1080p@59.94/60Hz */
	[16] = {
		NULL, 60, 1920, 1080, 6734, 148, 88, 36, 4, 44, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #17: 720x576p@50Hz 4:3 */
	[17] = {
		NULL, 50, 720, 576, 37037, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #18: 720x576p@50Hz 16:9 */
	[18] = {
		NULL, 50, 720, 576, 37037, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #19: 1280x720p@50Hz */
	[19] = {
		NULL, 50, 1280, 720, 13468, 220, 440, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #20: 1920x1080i@50Hz */
	[20] = {
		NULL, 50, 1920, 1080, 13468, 148, 528, 31, 4, 44, 10,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #21: 720(1440)x576iH@50Hz 4:3 */
	[21] = {
		NULL, 50, 1440, 576, 37037, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #22: 720(1440)x576iH@50Hz 16:9 */
	[22] = {
		NULL, 50, 1440, 576, 37037, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #23: 720(1440)x288pH@50Hz 4:3 */
	[23] = {
		NULL, 50, 1440, 288, 37037, 138, 24, 19, 2, 126, 3, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #24: 720(1440)x288pH@50Hz 16:9 */
	[24] = {
		NULL, 50, 1440, 288, 37037, 138, 24, 19, 2, 126, 3, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #29: 1440x576p@50Hz 4:3 */
	[29] = {
		NULL, 50, 1440, 576, 18518, 136, 24, 39, 5, 128, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #30: 1440x576p@50Hz 16:9 */
	[30] = {
		NULL, 50, 1440, 576, 18518, 136, 24, 39, 5, 128, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #31: 1920x1080p@50Hz */
	[31] = {
		NULL, 50, 1920, 1080, 6734, 148, 528, 36, 4, 44, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #32: 1920x1080p@23.97/24Hz */
	[32] = {
		NULL, 24, 1920, 1080, 13468, 148, 638, 36, 4, 44, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #33: 1920x1080p@25Hz */
	[33] = {
		NULL, 25, 1920, 1080, 13468, 148, 528, 36, 4, 44, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #34: 1920x1080p@29.97/30Hz */
	[34] = {
		NULL, 30, 1920, 1080, 13468, 148, 88, 36, 4, 44, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #40: 1920x1080i@100Hz */
	[40] = {
		NULL, 100, 1920, 1080, 6734, 148, 528, 31, 4, 44, 10,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #41: 1280x720p@100Hz */
	[41] = {
		NULL, 100, 1280, 720, 6734, 220, 440, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #42: 720x576p@100Hz 4:3 */
	[42] = {
		NULL, 100, 720, 576, 18518, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #43: 720x576p@100Hz 16:9 */
	[43] = {
		NULL, 100, 720, 576, 18518, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #44: 720(1440)x576iH@100Hz 4:3 */
	[44] = {
		NULL, 100, 1440, 576, 18518, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #45: 720(1440)x576iH@100Hz 16:9 */
	[45] = {
		NULL, 100, 1440, 576, 18518, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #46: 1920x1080i@119.88/120Hz */
	[46] = {
		NULL, 120, 1920, 1080, 6734, 148, 88, 31, 4, 44, 10,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #47: 1280x720p@119.88/120Hz */
	[47] = {
		NULL, 120, 1280, 720, 6734, 220, 110, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #48: 720x480p@119.88/120Hz 4:3 */
	[48] = {
		NULL, 120, 720, 480, 18518, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #49: 720x480p@119.88/120Hz 16:9 */
	[49] = {
		NULL, 120, 720, 480, 18518, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #50: 720(1440)x480iH@119.88/120Hz 4:3 */
	[50] = {
		NULL, 120, 1440, 480, 18518, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #51: 720(1440)x480iH@119.88/120Hz 16:9 */
	[51] = {
		NULL, 120, 1440, 480, 18518, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #52: 720x576p@200Hz 4:3 */
	[52] = {
		NULL, 200, 720, 576, 9259, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #53: 720x576p@200Hz 16:9 */
	[53] = {
		NULL, 200, 720, 576, 9259, 68, 12, 39, 5, 64, 5, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #54: 720(1440)x576iH@200Hz 4:3 */
	[54] = {
		NULL, 200, 1440, 576, 9259, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #55: 720(1440)x576iH@200Hz 16:9 */
	[55] = {
		NULL, 200, 1440, 576, 9259, 138, 24, 38, 4, 126, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #56: 720x480p@239.76/240Hz 4:3 */
	[56] = {
		NULL, 240, 720, 480, 9259, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #57: 720x480p@239.76/240Hz 16:9 */
	[57] = {
		NULL, 240, 720, 480, 9259, 60, 16, 30, 9, 62, 6, 0,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#ifdef	PIXEL_REPETITION_FIXED
	/* #58: 720(1440)x480iH@239.76/240Hz 4:3 */
	[58] = {
		NULL, 240, 1440, 480, 9259, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_4_3, 0
	},
	/* #59: 720(1440)x480iH@239.76/240Hz 16:9 */
	[59] = {
		NULL, 240, 1440, 480, 9259, 114, 38, 30, 8, 124, 6, 0,
		FB_VMODE_INTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
#endif
	/* #61: 1280x720p@25Hz */
	[61] = {
		NULL, 25, 1280, 720, 13468, 220, 2420, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
	/* #62: 1280x720p@29.97/30Hz */
	[62] = {
		NULL, 30, 1280, 720, 13468, 220, 1760, 20, 5, 40, 5,
		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		FB_VMODE_NONINTERLACED | FB_VMODE_ASPECT_16_9, 0
	},
};

/*
 * We have a special version of fb_mode_is_equal that ignores
 * pixclock, since for many CEA modes, 2 frequencies are supported
 * e.g. 640x480 @ 60Hz or 59.94Hz
 */
int mxc_edid_fb_mode_is_equal(bool use_aspect,
			const struct fb_videomode *mode1,
			const struct fb_videomode *mode2)
{
	u32 mask;

	if (use_aspect)
		mask = ~0;
	else
		mask = ~FB_VMODE_ASPECT_MASK;

	return (mode1->xres         == mode2->xres &&
		mode1->yres         == mode2->yres &&
		mode1->hsync_len    == mode2->hsync_len &&
		mode1->vsync_len    == mode2->vsync_len &&
		mode1->left_margin  == mode2->left_margin &&
		mode1->right_margin == mode2->right_margin &&
		mode1->upper_margin == mode2->upper_margin &&
		mode1->lower_margin == mode2->lower_margin &&
		mode1->sync         == mode2->sync &&
		/* refresh check, 59.94Hz and 60Hz have the same parameter
		 * in struct of mxc_cea_mode */
		abs(mode1->refresh - mode2->refresh) <= 1 &&
		(mode1->vmode & mask) == (mode2->vmode & mask));
}
EXPORT_SYMBOL(mxc_edid_fb_mode_is_equal);

int mxc_edid_external_get(struct i2c_adapter *adp, unsigned short addr,
				  unsigned char *data, int num_blocks_max)
{
	int ret, msg_ofs, block_num = 0, num_ext_blocks = 0;
	unsigned char segment, regaddr;
	struct i2c_msg msg[3] = {
		{
		.addr	= 0x30,
		.flags	= 0,
		.len	= 1,
		.buf	= &segment,
		}, {
		.addr	= addr,
		.flags	= 0,
		.len	= 1,
		.buf	= &regaddr,
		}, {
		.addr	= addr,
		.flags	= I2C_M_RD,
		.len	= EDID_LENGTH,
		.buf	= data,
		},
	};

	do {
		regaddr = (block_num & 1) * EDID_LENGTH;
		segment = block_num >> 1;
		msg_ofs = (segment == 0) ? 1 : 0;

		ret = i2c_transfer(adp, msg + msg_ofs, ARRAY_SIZE(msg) - msg_ofs);
		if (ret != ARRAY_SIZE(msg) - msg_ofs) {
			dev_err(&adp->dev, "EDID external failed to read block %d\n", block_num);
			return -EIO;
		}

		msg[2].buf += EDID_LENGTH;

		if (block_num++ == 0) {
			if (!mxc_edid_get_ext_blks(data, &num_ext_blocks)) {
				dev_err(&adp->dev, "EDID external header check failed!");
				return -ENOENT;
			}

			if (num_ext_blocks > num_blocks_max - 1)
				num_ext_blocks = num_blocks_max - 1;
		}
	} while (block_num <= num_ext_blocks);

	return num_ext_blocks;
}
EXPORT_SYMBOL(mxc_edid_external_get);

static void get_detailed_timing(unsigned char *block,
				struct fb_videomode *mode)
{
	mode->xres = H_ACTIVE;
	mode->yres = V_ACTIVE;
	mode->pixclock = PIXEL_CLOCK;
	mode->pixclock /= 1000;
	mode->pixclock = KHZ2PICOS(mode->pixclock);
	mode->right_margin = H_SYNC_OFFSET;
	mode->left_margin = (H_ACTIVE + H_BLANKING) -
		(H_ACTIVE + H_SYNC_OFFSET + H_SYNC_WIDTH);
	mode->upper_margin = V_BLANKING - V_SYNC_OFFSET -
		V_SYNC_WIDTH;
	mode->lower_margin = V_SYNC_OFFSET;
	mode->hsync_len = H_SYNC_WIDTH;
	mode->vsync_len = V_SYNC_WIDTH;
	if (HSYNC_POSITIVE)
		mode->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (VSYNC_POSITIVE)
		mode->sync |= FB_SYNC_VERT_HIGH_ACT;
	mode->refresh = PIXEL_CLOCK/((H_ACTIVE + H_BLANKING) *
				     (V_ACTIVE + V_BLANKING));
	if (INTERLACED) {
		mode->yres *= 2;
		mode->upper_margin *= 2;
		mode->lower_margin *= 2;
		mode->vsync_len *= 2;
		mode->vmode |= FB_VMODE_INTERLACED;
	}
	mode->flag = FB_MODE_IS_DETAILED;

	if ((H_SIZE / 16) == (V_SIZE / 9))
		mode->vmode |= FB_VMODE_ASPECT_16_9;
	else if ((H_SIZE / 4) == (V_SIZE / 3))
		mode->vmode |= FB_VMODE_ASPECT_4_3;
	else if ((mode->xres / 16) == (mode->yres / 9))
		mode->vmode |= FB_VMODE_ASPECT_16_9;
	else if ((mode->xres / 4) == (mode->yres / 3))
		mode->vmode |= FB_VMODE_ASPECT_4_3;

	if (mode->vmode & FB_VMODE_ASPECT_16_9)
		DPRINTK("Aspect ratio: 16:9\n");
	if (mode->vmode & FB_VMODE_ASPECT_4_3)
		DPRINTK("Aspect ratio: 4:3\n");
	DPRINTK("      %d MHz ",  PIXEL_CLOCK/1000000);
	DPRINTK("%d %d %d %d ", H_ACTIVE, H_ACTIVE + H_SYNC_OFFSET,
	       H_ACTIVE + H_SYNC_OFFSET + H_SYNC_WIDTH, H_ACTIVE + H_BLANKING);
	DPRINTK("%d %d %d %d ", V_ACTIVE, V_ACTIVE + V_SYNC_OFFSET,
	       V_ACTIVE + V_SYNC_OFFSET + V_SYNC_WIDTH, V_ACTIVE + V_BLANKING);
	DPRINTK("%sHSync %sVSync\n\n", (HSYNC_POSITIVE) ? "+" : "-",
	       (VSYNC_POSITIVE) ? "+" : "-");
}

static int parse_ext_blk(unsigned char *edid,
		struct mxc_edid_cfg *cfg, struct fb_monspecs *specs)
{
	char detail_timing_desc_offset;
	struct fb_videomode *mode, *m;
	unsigned char index = 0x0;
	unsigned char *block;
	int i, num = 0, revision;

	if (edid[index++] != 0x2) /* only support cea ext block now */
		return 0;
	revision = edid[index++];
	DPRINTK("cea extent revision %d\n", revision);
	mode = kzalloc((ARRAY_SIZE(mxc_cea_mode) + 8) * 
			sizeof(struct fb_videomode), GFP_KERNEL);
	if (mode == NULL)
		return -1;

	detail_timing_desc_offset = edid[index++];

	if (revision >= 2) {
		cfg->cea_underscan = (edid[index] >> 7) & 0x1;
		cfg->cea_basicaudio = (edid[index] >> 6) & 0x1;
		cfg->cea_ycbcr444 = (edid[index] >> 5) & 0x1;
		cfg->cea_ycbcr422 = (edid[index] >> 4) & 0x1;

		if (cfg->cea_basicaudio) {
		      cfg->sample_rates[0] = 0x07;
		      cfg->sample_sizes[0] = 0x01;
		}

		DPRINTK("CEA underscan %d\n", cfg->cea_underscan);
		DPRINTK("CEA basicaudio %d\n", cfg->cea_basicaudio);
		DPRINTK("CEA ycbcr444 %d\n", cfg->cea_ycbcr444);
		DPRINTK("CEA ycbcr422 %d\n", cfg->cea_ycbcr422);
	}

	if (revision >= 3) {
		/* short desc */
		DPRINTK("CEA Short desc timmings\n");
		index++;
		while (index < detail_timing_desc_offset) {
			unsigned char tagcode, blklen;

			tagcode = (edid[index] >> 5) & 0x7;
			blklen = (edid[index]) & 0x1f;

			DPRINTK("Tagcode %x Len %d\n", tagcode, blklen);

			switch (tagcode) {
			case 0x2: /*Video data block*/
				{
					int cea_idx;
					i = 0;
					while (i < blklen) {
						index++;
						cea_idx = edid[index] & 0x7f;
						if (cea_idx < ARRAY_SIZE(mxc_cea_mode) &&
								(mxc_cea_mode[cea_idx].xres)) {
							DPRINTK("Support CEA Format #%d\n", cea_idx);
							mode[num] = mxc_cea_mode[cea_idx];
							mode[num].flag |= FB_MODE_IS_STANDARD;
							num++;
						}
						i++;
					}
					break;
				}
			case 0x3: /*Vendor specific data*/
				{
					unsigned      IEEE_reg_iden;
					unsigned char deep_color;
					unsigned char latency_present;
					unsigned char I_latency_present;
					unsigned char hdmi_video_present;
					unsigned char hdmi_3d_present;
					unsigned char hdmi_3d_multi_present;
					unsigned char hdmi_vic_len;
					unsigned char hdmi_3d_len;
					unsigned char index_inc;
					unsigned char vsd_end;

					IEEE_reg_iden = (blklen > 2) ?
						((edid[index+3] << 16) | (edid[index+2] << 8) | edid[index+1]) : 0;

					if (IEEE_reg_iden != 0x000c03) {
						DPRINTK("VSD block OUI=0x%06x (unknown) present\n", IEEE_reg_iden);
						index += blklen;
						break;
					}

					DPRINTK("VSD block HDMI 1.x 'HDMI Licensing, LLC' present\n");

					cfg->hdmi_cap = 1;
					cfg->physical_address[0] = edid[index+4] >> 4;
					cfg->physical_address[1] = edid[index+4] & 0x0f;
					cfg->physical_address[2] = edid[index+5] >> 4;
					cfg->physical_address[3] = edid[index+5] & 0x0f;

					DPRINTK("VSD physical address 0x%x%x%x%x\n",
						cfg->physical_address[0], cfg->physical_address[1],
						cfg->physical_address[2], cfg->physical_address[3]);

					if (blklen > 5) {
						deep_color = edid[index+6];
						if (deep_color & 0x80)
							cfg->vsd_support_ai = true;
						if (deep_color & 0x40)
							cfg->vsd_dc_48bit = true;
						if (deep_color & 0x20)
							cfg->vsd_dc_36bit = true;
						if (deep_color & 0x10)
							cfg->vsd_dc_30bit = true;
						if (deep_color & 0x08)
							cfg->vsd_dc_y444 = true;
						if (deep_color & 0x01)
							cfg->vsd_dvi_dual = true;
					}

					DPRINTK("VSD support ai %d\n", cfg->vsd_support_ai);
					DPRINTK("VSD support deep color 48bit %d\n", cfg->vsd_dc_48bit);
					DPRINTK("VSD support deep color 36bit %d\n", cfg->vsd_dc_36bit);
					DPRINTK("VSD support deep color 30bit %d\n", cfg->vsd_dc_30bit);
					DPRINTK("VSD support deep color y444 %d\n", cfg->vsd_dc_y444);
					DPRINTK("VSD support dvi dual %d\n", cfg->vsd_dvi_dual);

					if (blklen > 6)
						cfg->vsd_max_tmdsclk_rate = edid[index+7] * 5;
					DPRINTK("VSD MAX TMDS CLOCK RATE %d\n", cfg->vsd_max_tmdsclk_rate);

					if (blklen > 7) {
						latency_present = edid[index+8] >> 7;
						I_latency_present =  (edid[index+8] & 0x40) >> 6;
						hdmi_video_present = (edid[index+8] & 0x20) >> 5;
						cfg->vsd_cnc3 = (edid[index+8] & 0x8) >> 3;
						cfg->vsd_cnc2 = (edid[index+8] & 0x4) >> 2;
						cfg->vsd_cnc1 = (edid[index+8] & 0x2) >> 1;
						cfg->vsd_cnc0 = edid[index+8] & 0x1;

						DPRINTK("VSD cnc0 %d\n", cfg->vsd_cnc0);
						DPRINTK("VSD cnc1 %d\n", cfg->vsd_cnc1);
						DPRINTK("VSD cnc2 %d\n", cfg->vsd_cnc2);
						DPRINTK("VSD cnc3 %d\n", cfg->vsd_cnc3);
						DPRINTK("latency_present %d\n", latency_present);
						DPRINTK("I_latency_present %d\n", I_latency_present);
						DPRINTK("hdmi_video_present %d\n", hdmi_video_present);

					} else {
						index += blklen;
						break;
					}

					vsd_end = index + blklen;
					index += 9;

					/*latency present */
					if (latency_present) {
						cfg->vsd_video_latency = edid[index++];
						cfg->vsd_audio_latency = edid[index++];

						if (I_latency_present) {
							cfg->vsd_I_video_latency = edid[index++];
							cfg->vsd_I_audio_latency = edid[index++];
						} else {
							cfg->vsd_I_video_latency = cfg->vsd_video_latency;
							cfg->vsd_I_audio_latency = cfg->vsd_audio_latency;
						}

						DPRINTK("VSD latency video_latency  %d\n", cfg->vsd_video_latency);
						DPRINTK("VSD latency audio_latency  %d\n", cfg->vsd_audio_latency);
						DPRINTK("VSD latency I_video_latency  %d\n", cfg->vsd_I_video_latency);
						DPRINTK("VSD latency I_audio_latency  %d\n", cfg->vsd_I_audio_latency);
					}

					if (hdmi_video_present) {
						hdmi_3d_present = edid[index] >> 7;
						hdmi_3d_multi_present = (edid[index] & 0x60) >> 5;
						index++;
						hdmi_vic_len = (edid[index] & 0xe0) >> 5;
						hdmi_3d_len = edid[index] & 0x1f;
						index++;

						DPRINTK("hdmi_3d_present %d\n", hdmi_3d_present);
						DPRINTK("hdmi_3d_multi_present %d\n", hdmi_3d_multi_present);
						DPRINTK("hdmi_vic_len %d\n", hdmi_vic_len);
						DPRINTK("hdmi_3d_len %d\n", hdmi_3d_len);

						if (hdmi_vic_len > 0) {
							for (i = 0; i < hdmi_vic_len; i++) {
								cfg->hdmi_vic[i] = edid[index++];
								DPRINTK("HDMI_vic=%d\n", cfg->hdmi_vic[i]);
							}
						}

						if (hdmi_3d_len > 0) {
							index_inc = 0;

							if (hdmi_3d_present) {
								if (hdmi_3d_multi_present == 0x1) {
									cfg->hdmi_3d_struct_all = (edid[index] << 8) | edid[index+1];
									index_inc = 2;
								} else if (hdmi_3d_multi_present == 0x2) {
									cfg->hdmi_3d_struct_all = (edid[index] << 8) | edid[index+1];
									cfg->hdmi_3d_mask_all = (edid[index+2] << 8) | edid[index+3];
									index_inc = 4;
								}
							}

							DPRINTK("HDMI 3d struct all =0x%x\n", cfg->hdmi_3d_struct_all);
							DPRINTK("HDMI 3d mask all =0x%x\n", cfg->hdmi_3d_mask_all);

							/* Read 2D vic 3D_struct */
							if ((hdmi_3d_len - index_inc) > 0) {
								DPRINTK("Support 3D video format\n");
								i = 0;
								while ((hdmi_3d_len - index_inc) > 0) {

									cfg->hdmi_3d_format[i].vic_order_2d = edid[index+index_inc] >> 4;
									cfg->hdmi_3d_format[i].struct_3d = edid[index+index_inc] & 0x0f;
									index_inc++;

									if (cfg->hdmi_3d_format[i].struct_3d ==  8) {
										cfg->hdmi_3d_format[i].detail_3d = edid[index+index_inc] >> 4;
										index_inc++;
									} else if (cfg->hdmi_3d_format[i].struct_3d > 8) {
										cfg->hdmi_3d_format[i].detail_3d = 0;
										index_inc++;
									}

									DPRINTK("vic_order_2d=%d, 3d_struct=%d, 3d_detail=0x%x\n",
											cfg->hdmi_3d_format[i].vic_order_2d,
											cfg->hdmi_3d_format[i].struct_3d,
											cfg->hdmi_3d_format[i].detail_3d);
									i++;
								}
							}
							index += index_inc;
						}
					}

					index = vsd_end;
					break;
				}
			case 0x1: /*Audio data block*/
				{
					u8 audio_format, byte1, byte2, byte3;
					int ch_idx;

					i = 0;
					while (i < blklen) {
						byte1 = edid[index + 1];
						byte2 = edid[index + 2];
						byte3 = edid[index + 3];
						index += 3;
						i += 3;

						audio_format = (byte1 & 0x7f) >> 3;

						DPRINTK("Audio Format Descriptor : %2d\n", audio_format);
						DPRINTK("Max Number of Channels  : %2d\n", (byte1 & 0x07) + 1);
						DPRINTK("Sample Rates            : %02x\n", byte2 & 0x7f);

						/* ALSA can't specify specific compressed
						 * formats, so only care about PCM for now. */
						if (audio_format == AUDIO_CODING_TYPE_LPCM) {
							for (ch_idx = (byte1 & 0x07) / 2; ch_idx >= 0; ch_idx--) {
								cfg->sample_rates[ch_idx] |= byte2 & 0x7f;
								cfg->sample_sizes[ch_idx] |= byte3 & 0x7;
							}
							DPRINTK("Sample Sizes            : %02x\n",
								byte3 & 0x7);
						}

						/* HD-audio: pretend to support 192kHz/16bit in 8ch mode to
						 * allow HD passthrough on sinks that report only 6ch LPCM */
						if (audio_format == AUDIO_CODING_TYPE_MLP ||
						    audio_format == AUDIO_CODING_TYPE_DTS_HD) {
							cfg->sample_rates[3] |= 0x40;
							cfg->sample_sizes[3] |= 0x01;
						}
					}
					break;
				}
			case 0x4: /*Speaker allocation block*/
				{
					i = 0;
					while (i < blklen) {
						cfg->speaker_alloc = edid[index + 1] & 0x7f;
						index += 3;
						i += 3;
						DPRINTK("Speaker Alloc           : %02x\n", cfg->speaker_alloc);
					}
					break;
				}
			case 0x7: /*User extended block*/
				if (blklen >= 2 && edid[index + 1] == 0) {	/*Video Capability Data Block*/
					u8 data = edid[index + 2];

					cfg->cea_scan_mode_ce = (data >> 0) & 3;
					cfg->cea_scan_mode_it = (data >> 2) & 3;
					cfg->cea_scan_mode_pt = (data >> 4) & 3;
					cfg->cea_rgb_range_selectable = (data >> 6) & 1;

					DPRINTK("VCDB over/underscan behavior (CE)  %d\n", cfg->cea_scan_mode_ce);
					DPRINTK("VCDB over/underscan behavior (IT)  %d\n", cfg->cea_scan_mode_it);
					DPRINTK("VCDB over/underscan behavior (PT)  %d\n", cfg->cea_scan_mode_pt);
					DPRINTK("VCDB RGB quant. range selectable   %d\n", cfg->cea_rgb_range_selectable);

					index += blklen;
					break;
				}
			default:
				/* skip */
				DPRINTK("Not handle block, tagcode = 0x%x\n", tagcode);
				index += blklen;
				break;
			}

			index++;
		}
	}

	/* long desc */
	DPRINTK("CEA long desc timmings\n");
	index = detail_timing_desc_offset;
	block = edid + index;
	while (index < (EDID_LENGTH - DETAILED_TIMING_DESCRIPTION_SIZE)) {
		if (!(block[0] == 0x00 && block[1] == 0x00)) {
			get_detailed_timing(block, &mode[num]);
			num++;
		}
		block += DETAILED_TIMING_DESCRIPTION_SIZE;
		index += DETAILED_TIMING_DESCRIPTION_SIZE;
	}

	if (!num) {
		kfree(mode);
		return 0;
	}

	m = kmalloc((num + specs->modedb_len) *
			sizeof(struct fb_videomode), GFP_KERNEL);
	if (!m) {
		kfree(mode);
		return 0;
	}

	memcpy(m, mode, num * sizeof(struct fb_videomode));
	kfree(mode);

	if (specs->modedb_len) {
		memcpy(m + num, specs->modedb,
		       specs->modedb_len * sizeof(struct fb_videomode));
		kfree(specs->modedb);
	}

	specs->modedb_len += num;
	specs->modedb = m;

	/* Remove FB_MODE_IS_STANDARD if an alternate aspect ratio exists.
	   This allows to distiguish between 16:9 and 4:3 modes in sysfs. */
	for (mode = &specs->modedb[1]; mode < &specs->modedb[specs->modedb_len]; mode++) {
		if (mode->flag != FB_MODE_IS_STANDARD || !(mode->vmode & FB_VMODE_ASPECT_MASK))
			continue;

		mode->vmode ^= FB_VMODE_ASPECT_MASK;
		for (m = mode - 1; m >= specs->modedb; m--) {
		      if (fb_mode_is_equal(mode, m)) {
			      mode->flag &= ~FB_MODE_IS_STANDARD;
			      break;
		      }
		}
		mode->vmode ^= FB_VMODE_ASPECT_MASK;
	}

	return 0;
}

int mxc_edid_get_ext_blks(unsigned char *edid, int *num_ext_blocks)
{
	static const u8 edid_header[] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
	};

	if (memcmp(edid, edid_header, sizeof(edid_header)) == 0) {
		*num_ext_blocks = (edid[0x7E] == 0xFF) ? 0 : edid[0x7E];
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(mxc_edid_get_ext_blks);

void mxc_edid_parse(unsigned char *edid,
	int num_ext_blocks, struct mxc_edid_cfg *cfg, struct fb_info *fbi)
{
	struct fb_videomode *m;

	if (!edid || !cfg || !fbi)
		return;

	/* Free old monspecs */
	if (fbi->monspecs.modedb_len > 0)
		fb_destroy_modedb(fbi->monspecs.modedb);

	/* edid first block parsing */
	memset(&fbi->monspecs, 0, sizeof(fbi->monspecs));
	fb_edid_to_monspecs(edid, &fbi->monspecs);

	for (m = fbi->monspecs.modedb; m < fbi->monspecs.modedb + fbi->monspecs.modedb_len; m++) {
		if (m->xres / 16 == m->yres / 9)
			m->vmode |= FB_VMODE_ASPECT_16_9;
		else if (m->xres / 4 == m->yres / 3)
			m->vmode |= FB_VMODE_ASPECT_4_3;

		/* HACK: Our IPU doesn't like some timings calculated by the VESA formula.
		   Especially, a lower margin below 2 is not acceptable. So let's increase
		   the vertical blanking intervall a bit in this situation */
		if (m->lower_margin <= 1) {
			m->lower_margin += 2;
			m->vsync_len += 2;
			m->pixclock = KHZ2PICOS(((m->xres + m->left_margin + m->right_margin + m->hsync_len) *
						  (m->yres + m->upper_margin + m->lower_margin + m->vsync_len) *
						  m->refresh) / (1000 << !!(m->vmode & FB_VMODE_INTERLACED)));
			pr_warn("%s: Vertical blanking adjusted for %dx%d%c-%d. Please check modedb!\n",
				__func__, m->xres, m->yres, (m->vmode & FB_VMODE_INTERLACED) ? 'i' : 'p', m->refresh);
		}
	}

	/* edid extension block parsing */
	memset(cfg, 0, sizeof(struct mxc_edid_cfg));
	while (num_ext_blocks-- > 0) {
		edid += EDID_LENGTH;
		parse_ext_blk(edid, cfg, &fbi->monspecs);
	}
}
EXPORT_SYMBOL(mxc_edid_parse);

int mxc_edid_var_to_vic(const struct fb_var_screeninfo *var)
{
	int i;
	struct fb_videomode m;

	fb_var_to_videomode(&m, var);

	for (i = 0; i < ARRAY_SIZE(mxc_cea_mode); i++) {
		if (mxc_edid_fb_mode_is_equal(false, &m, &mxc_cea_mode[i]))
			return i;
	}

	return 0;
}
EXPORT_SYMBOL(mxc_edid_var_to_vic);

int mxc_edid_mode_to_vic(const struct fb_videomode *mode)
{
	int i;
	bool use_aspect = (mode->vmode & FB_VMODE_ASPECT_MASK);

	for (i = 0; i < ARRAY_SIZE(mxc_cea_mode); i++) {
		if (mxc_edid_fb_mode_is_equal(use_aspect, mode, &mxc_cea_mode[i]))
			return i;
	}

	return 0;
}
EXPORT_SYMBOL(mxc_edid_mode_to_vic);

const struct fb_videomode *mxc_fb_find_nearest_mode(const struct fb_videomode *mode,
						    struct list_head *head)
{
	struct list_head *pos;
	struct fb_modelist *modelist;
	u32 i, d, diff = -1, diff_refresh = -1;
	struct fb_videomode *cmode, *best = NULL;
	static const u32 masks[] = { ~0, ~FB_VMODE_ASPECT_MASK };

	for (i = 0; i < ARRAY_SIZE(masks); i++) {
		list_for_each(pos, head) {
			modelist = list_entry(pos, struct fb_modelist, list);
			cmode = &modelist->mode;

			if ((mode->vmode ^ cmode->vmode) & masks[i])
				continue;

			d = abs(cmode->xres - mode->xres) +
				abs(cmode->yres - mode->yres);
			if (diff > d) {
				diff = d;
				diff_refresh = abs(cmode->refresh - mode->refresh);
				best = cmode;
			} else if (diff == d) {
				d = abs(cmode->refresh - mode->refresh);
				if (diff_refresh > d) {
					diff_refresh = d;
					best = cmode;
				}
			}

			if (diff == 0 && diff_refresh == 0)
				return best;
		}
	}

	return best;
}
EXPORT_SYMBOL(mxc_fb_find_nearest_mode);

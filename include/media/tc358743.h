/*
 * tc358743 - Toshiba HDMI to CSI-2 bridge
 *
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates. All rights
 * reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * References (c = chapter, p = page):
 * REF_01 - Toshiba, TC358743XBG (H2C), Functional Specification, Rev 0.60
 * REF_02 - Toshiba, TC358743XBG_HDMI-CSI_Tv11p_nm.xls
 */

#ifndef _TC358743_
#define _TC358743_

enum tc358743_ddc5v_delays {
	DDC5V_DELAY_0MS,
	DDC5V_DELAY_50MS,
	DDC5V_DELAY_100MS,
	DDC5V_DELAY_200MS,
};

struct tc358743_platform_data {
	u32 refclk_hz; /* System clock connected to REFCLK (pin H5) */
	enum tc358743_ddc5v_delays ddc5v_delay; /* DDC +5V debounce delay */
	bool enable_hdcp;
	/*
	 * The FIFO size is 512x32, so Toshiba recommend to set the default FIFO
	 * level to somewhere in the middle (eg. 200), so it can cover speed
	 * mismatches in input and output ports.
	 */
	u16 fifo_level;
	u16 pll_prd; /* PLLCTL0 */
	u16 pll_fbd; /*PLLCTL0 */

	/* CSI
	 * Calculate CSI parameters with REF_02 for the highest resolution your
	 * CSI interface can handle. The driver will adjust the number of CSI
	 * lanes in use according to the pixel clock.
	 */
	u32 bps_pr_lane;
	u32 lineinitcnt;
	u32 lptxtimecnt;
	u32 tclk_headercnt;
	u32 tclk_trailcnt;
	u32 ths_headercnt;
	u32 twakeup;
	u32 tclk_postcnt;
	u32 ths_trailcnt;
	u32 hstxvregcnt;

	/* HDMI PHY */
	u8 phy_auto_rst; /* PHY_CTL2, default = 0 */
	u8 hdmi_det_v; /* HDMI_DET, default = 0 */
	u8 h_pi_rst; /* HV_RST, default = 0 */
	u8 v_pi_rst; /* HV_RST, default = 0 */
};

enum tc358743_cable_connection {
	TC358743_CABLE_PLUGGED,
	TC358743_CABLE_UNPLUGGED,
};

/* notify events */
#define TC358743_FMT_CHANGE     1

/* ioctls */
#define TC358743_CSI_RESET      _IO('a', 1)

/* custom controls */
#define TC358743_CID_AUDIO_SAMPLING_RATE (V4L2_CID_USER_TC358743_BASE + 0)
#define TC358743_CID_AUDIO_PRESENT       (V4L2_CID_USER_TC358743_BASE + 1)

#endif

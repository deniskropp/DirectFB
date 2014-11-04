/*
   (c) Copyright 2012-2014  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/
/*
 * Copyright © 2014 Renesas Electronics Corp.
 *
 * Based on pixman-renderer by:
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Vasily Khoruzhick <anarsoul@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Takanari Hayama <taki@igel.co.jp>
 */

#ifndef __VSP1_RENDERER_H__
#define __VSP1_RENDERER_H__


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

#include "media-ctl/mediactl.h"
#include "media-ctl/mediactl-priv.h"
#include "media-ctl/v4l2subdev.h"
#include "media-ctl/tools.h"

#include "vsp-adapt.h"

#if 0
#define DBG(...) weston_log(__VA_ARGS__)
#define DBGC(...) weston_log_continue(__VA_ARGS__)
#else
#define DBG(...) do {} while (0)
#define DBGC(...) do {} while (0)
#endif

struct vsp_surface_state {
	struct v4l2_surface_state base;

	struct v4l2_format fmt;
	enum v4l2_mbus_pixelcode mbus_code;
};

struct vsp_renderer_output {
	struct v4l2_renderer_output base;
	struct vsp_surface_state surface_state;
};

#define VSP_INPUT_MAX	4
#define VSP_SCALER_MAX	1

struct vsp_media_pad {
	struct media_pad	*infmt_pad;
	struct media_pad	*outfmt_pad;
	struct media_pad	*compose_pad;
	struct media_entity	*input_entity;

	struct media_link	*link;

	int			fd;
};

struct vsp_scaler_template {
	struct media_link *link0;	// rpf -> uds
	struct media_link *link1;	// uds -> bru
};

struct vsp_scaler {
	int input;

	struct media_pad	*infmt_pad;
	struct media_pad	*outfmt_pad;

	struct vsp_scaler_template	templates[VSP_INPUT_MAX];
};

typedef enum {
	VSP_STATE_IDLE,
	VSP_STATE_START,
	VSP_STATE_COMPOSING,
} vsp_state_t;

struct vsp_device {
	struct v4l2_renderer_device base;

	vsp_state_t state;

	struct vsp_media_pad output_pad;
	struct vsp_surface_state *output_surface_state;

	int input_count;
	int input_max;
	struct vsp_media_pad input_pads[VSP_INPUT_MAX];
	struct vsp_surface_state *input_surface_states[VSP_INPUT_MAX];
	struct vsp_scaler *use_scaler[VSP_INPUT_MAX];

	int scaler_count;
	int scaler_max;
	struct vsp_scaler scalers[VSP_SCALER_MAX];
};


extern struct v4l2_device_interface v4l2_device_interface;


#endif


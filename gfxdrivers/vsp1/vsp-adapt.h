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

#ifndef __VSP1_ADAPT_H__
#define __VSP1_ADAPT_H__


#include <direct/types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>

#include "media-ctl/mediactl.h"
#include "media-ctl/mediactl-priv.h"
#include "media-ctl/v4l2subdev.h"
#include "media-ctl/tools.h"


struct weston_compositor;
struct weston_output;


struct v4l2_bo_state {
	int dmafd;
	void *map;
	u32 stride;
};

struct v4l2_renderer_interface {
	int (*init)(struct weston_compositor *ec, int drm_fd, char *drm_fn);
	int (*output_create)(struct weston_output *output);
	void (*output_destroy)(struct weston_output *output);
	void (*set_output_buffer)(struct weston_output *output, struct v4l2_bo_state *ro);
};



struct v4l2_renderer_device {
	struct media_device *media;
	const char *device_name;
};

struct v4l2_renderer_output {
	int width;
	int height;
};

struct v4l2_renderer_plane {
	int dmafd;
	unsigned int stride;
};

struct v4l2_surface_state {
//	struct weston_surface *surface;
//	struct weston_buffer_reference buffer_ref;

//	struct v4l2_renderer *renderer;

	struct kms_bo *bo;
	void *addr;
	int bpp;
	int bo_stride;

	int num_planes;
	struct v4l2_renderer_plane planes[VIDEO_MAX_PLANES];

	float alpha;
	int width;
	int height;
	unsigned int pixel_format;

	struct v4l2_rect src_rect;
	struct v4l2_rect dst_rect;

//	struct wl_listener buffer_destroy_listener;
//	struct wl_listener surface_destroy_listener;
//	struct wl_listener renderer_destroy_listener;
};

struct v4l2_device_interface {
	struct v4l2_renderer_device *(*init)(struct media_device *media);

	struct v4l2_renderer_output *(*create_output)(struct v4l2_renderer_device *dev, int width, int height);
	void (*set_output_buffer)(struct v4l2_renderer_output *out, struct v4l2_bo_state *bo);

	struct v4l2_surface_state *(*create_surface)(struct v4l2_renderer_device *dev);
	int (*attach_buffer)(struct v4l2_surface_state *vs);

	void (*begin_compose)(struct v4l2_renderer_device *dev, struct v4l2_renderer_output *out);
	void (*finish_compose)(struct v4l2_renderer_device *dev);
	int (*draw_view)(struct v4l2_renderer_device *dev, struct v4l2_surface_state *vs);
	void (*flush)(struct v4l2_renderer_device *dev, bool flush);

	uint32_t (*get_capabilities)(void);
};

#define weston_log            printf
#define weston_log_continue   printf

#define WL_EXPORT

#endif


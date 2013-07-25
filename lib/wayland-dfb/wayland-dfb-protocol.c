/* 
 * (c) Copyright 2012-2013  DirectFB integrated media GmbH
 * (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
 * (c) Copyright 2000-2004  Convergence (integrated media) GmbH
 * 
 * All rights reserved.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface wl_dfb_buffer_interface;

static const struct wl_interface *types[] = {
	NULL,
	NULL,
	&wl_dfb_buffer_interface,
	NULL,
	NULL,
	NULL,
};

static const struct wl_message wl_dfb_requests[] = {
	{ "create_buffer", "nuuu", types + 2 },
};

WL_EXPORT const struct wl_interface wl_dfb_interface = {
	"wl_dfb", 1,
	1, wl_dfb_requests,
	0, NULL,
};

static const struct wl_message wl_dfb_buffer_requests[] = {
	{ "destroy", "", types + 0 },
	{ "set_display_timestamp", "uu", types + 0 },
};

WL_EXPORT const struct wl_interface wl_dfb_buffer_interface = {
	"wl_dfb_buffer", 1,
	2, wl_dfb_buffer_requests,
	0, NULL,
};


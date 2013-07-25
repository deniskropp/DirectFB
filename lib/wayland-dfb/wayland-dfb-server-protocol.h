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

#ifndef DFB_SERVER_PROTOCOL_H
#define DFB_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-util.h"

struct wl_client;
struct wl_resource;

struct wl_dfb;
struct wl_dfb_buffer;

extern const struct wl_interface wl_dfb_interface;
extern const struct wl_interface wl_dfb_buffer_interface;

#ifndef WL_DFB_ERROR_ENUM
#define WL_DFB_ERROR_ENUM
enum wl_dfb_error {
	WL_DFB_ERROR_AUTHENTICATE_FAIL = 0,
	WL_DFB_ERROR_INVALID_FORMAT = 1,
	WL_DFB_ERROR_INVALID_NAME = 2,
};
#endif /* WL_DFB_ERROR_ENUM */

struct wl_dfb_interface {
	/**
	 * create_buffer - (none)
	 * @id: (none)
	 * @surface_id: (none)
	 * @buffer_id: (none)
	 * @allocation_id: (none)
	 */
	void (*create_buffer)(struct wl_client *client,
			      struct wl_resource *resource,
			      uint32_t id,
			      uint32_t surface_id,
			      uint32_t buffer_id,
			      uint32_t allocation_id);
};

struct wl_dfb_buffer_interface {
	/**
	 * destroy - destroy a buffer
	 *
	 * Destroy a buffer. If and how you need to release the backing
	 * storage is defined by the buffer factory interface.
	 *
	 * For possible side-effects to a surface, see wl_surface.attach.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set_display_timestamp - (none)
	 * @ts_low: (none)
	 * @ts_high: (none)
	 */
	void (*set_display_timestamp)(struct wl_client *client,
				      struct wl_resource *resource,
				      uint32_t ts_low,
				      uint32_t ts_high);
};

#ifdef  __cplusplus
}
#endif

#endif

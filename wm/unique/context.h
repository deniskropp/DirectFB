/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __UNIQUE__CONTEXT_H__
#define __UNIQUE__CONTEXT_H__

#include <directfb.h>

#include <fusion/object.h>

#include <core/wm.h>

#include <unique/types.h>


typedef enum {
     UCNF_NONE           = 0x00000000,

     UCNF_DESTROYED      = 0x00000001,

     UCNF_WINDOW_ADDED   = 0x00000010,
     UCNF_WINDOW_REMOVED = 0x00000020,

     UCNF_ACTIVATE       = 0x00000100,
     UCNF_DEACTIVATE     = 0x00000200,

     UCNF_RESIZE         = 0x00001000,

     UCNF_WARP_CURSOR    = 0x00010000,

     UCNF_ALL            = 0x00011331
} UniqueContextNotificationFlags;

typedef struct {
     UniqueContextNotificationFlags  flags;
     UniqueContext                  *context;

     DFBPoint                        pos;         /* New cursor position (UCNF_WARP_CURSOR) */
     DFBDimension                    size;        /* New root (desktop) size (UCNF_RESIZE) */
} UniqueContextNotification;



DFBResult unique_context_create       ( CoreWindowStack                 *stack,
                                        CoreLayerRegion                 *region,
                                        DFBDisplayLayerID                layer_id,
                                        WMShared                        *shared,
                                        UniqueContext                  **ret_context );

DFBResult unique_context_notify       ( UniqueContext                   *context,
                                        UniqueContextNotificationFlags   flags );

DFBResult unique_context_set_active   ( UniqueContext                   *context,
                                        bool                             active );

DFBResult unique_context_set_color    ( UniqueContext                   *context,
                                        const DFBColor                  *color );


DFBResult unique_context_update       ( UniqueContext                   *context,
                                        const DFBRegion                 *updates,
                                        int                              num,
                                        DFBSurfaceFlipFlags              flags );

DFBResult unique_context_resize       ( UniqueContext                   *context,
                                        int                              width,
                                        int                              height );

DFBResult unique_context_flush_keys   ( UniqueContext                   *context );

DFBResult unique_context_window_at    ( UniqueContext                   *context,
                                        int                              x,
                                        int                              y,
                                        UniqueWindow                   **ret_window );

DFBResult unique_context_lookup_window( UniqueContext                   *context,
                                        DFBWindowID                      window_id,
                                        UniqueWindow                   **ret_window );

DFBResult unique_context_enum_windows ( UniqueContext                   *context,
                                        CoreWMWindowCallback             callback,
                                        void                            *callback_ctx );

DFBResult unique_context_warp_cursor  ( UniqueContext                   *context,
                                        int                              x,
                                        int                              y );



/*
 * Creates a pool of context objects.
 */
FusionObjectPool *unique_context_pool_create();

/*
 * Generates unique_context_ref(), unique_context_attach() etc.
 */
FUSION_OBJECT_METHODS( UniqueContext, unique_context )


/* global reactions */

typedef enum {
     UNIQUE_WM_MODULE_CONTEXT_LISTENER
} UNIQUE_CONTEXT_GLOBALS;

#endif


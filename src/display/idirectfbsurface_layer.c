/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <malloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/state.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include <core/fbdev/fbdev.h>

#include <idirectfbsurface.h>
#include <idirectfbsurface_layer.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/util.h>



/*
 * private data struct of IDirectFBSurface_Layer
 */
typedef struct {
     IDirectFBSurface_data base;  /* base Surface implementation */

     DisplayLayer         *layer; /* pointer to layer this surface belongs to */
} IDirectFBSurface_Layer_data;


static void
IDirectFBSurface_Layer_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Destruct( thiz );
}

static DFBResult
IDirectFBSurface_Layer_Release( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface_Layer)

     if (--data->base.ref == 0)
          IDirectFBSurface_Layer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Layer_Flip( IDirectFBSurface    *thiz,
                             const DFBRegion     *region,
                             DFBSurfaceFlipFlags  flags )
{
     INTERFACE_GET_DATA(IDirectFBSurface_Layer)

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!(data->base.caps & DSCAPS_FLIPPING))
          return DFB_UNSUPPORTED;

     if (!data->base.area.current.w || !data->base.area.current.h)
          return DFB_INVAREA;


     if (flags & DSFLIP_BLIT || region || data->base.caps & DSCAPS_SUBSURFACE) {
          if (flags & DSFLIP_WAITFORSYNC)
               dfb_fbdev_wait_vsync();
          
          if (region) {
               DFBRegion    reg  = *region;
               DFBRectangle rect = data->base.area.current;

               reg.x1 += data->base.area.wanted.x;
               reg.x2 += data->base.area.wanted.x;
               reg.y1 += data->base.area.wanted.y;
               reg.y2 += data->base.area.wanted.y;

               if (dfb_rectangle_intersect_by_unsafe_region( &rect, &reg ))
                    dfb_back_to_front_copy( data->base.surface, &rect );
          }
          else {
               DFBRectangle rect = data->base.area.current;

               dfb_back_to_front_copy( data->base.surface, &rect );
          }
     }
     else
          return dfb_layer_flip_buffers( data->layer, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Layer_GetSubSurface( IDirectFBSurface    *thiz,
                                      const DFBRectangle  *rect,
                                      IDirectFBSurface   **surface )
{
     DFBRectangle wanted, granted;  

     INTERFACE_GET_DATA(IDirectFBSurface_Layer)

     /* Check arguments */
     if (!data->base.surface)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     /* Compute wanted rectangle */
     if (rect) {
          wanted = *rect;

          wanted.x += data->base.area.wanted.x;
          wanted.y += data->base.area.wanted.y;
 
          if (wanted.w <= 0 || wanted.h <= 0) {
               wanted.w = 0;
               wanted.h = 0;
          }
     }
     else
          wanted = data->base.area.wanted;

     /* Compute granted rectangle */
     granted = wanted;

     dfb_rectangle_intersect( &granted, &data->base.area.granted );

     /* Allocate and construct */
     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Layer_Construct( *surface, &wanted, &granted,
                                              data->layer, data->base.caps |
                                              DSCAPS_SUBSURFACE );
}

DFBResult
IDirectFBSurface_Layer_Construct( IDirectFBSurface       *thiz,
                                  DFBRectangle           *wanted,
                                  DFBRectangle           *granted,
                                  DisplayLayer           *layer,
                                  DFBSurfaceCapabilities  caps )
{
     DFBResult err = DFB_OK;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface_Layer)
     
     if (!(caps & DSCAPS_SUBSURFACE)  &&  !wanted) {
          DFBDisplayLayerConfig config;

          config.flags      = DLCONF_BUFFERMODE;

          if (caps & DSCAPS_FLIPPING) {
               if (caps & DSCAPS_VIDEOONLY)
                    config.buffermode = DLBM_BACKVIDEO;
               else if (caps & DSCAPS_SYSTEMONLY)
                    config.buffermode = DLBM_BACKSYSTEM;
               else {
                    config.buffermode = DLBM_BACKVIDEO;
                    if (dfb_layer_set_configuration( layer, &config ))
                         config.buffermode = DLBM_BACKSYSTEM;
               }
          }
          else
               config.buffermode = DLBM_FRONTONLY;

          err = dfb_layer_set_configuration( layer, &config );
          if (err) {
               DFB_DEALLOCATE_INTERFACE( thiz );
               return err;
          }
     }

     IDirectFBSurface_Construct( thiz, wanted, granted,
                                 dfb_layer_surface( layer ), caps );

     data->layer = layer;

     thiz->Release = IDirectFBSurface_Layer_Release;
     thiz->Flip = IDirectFBSurface_Layer_Flip;
     thiz->GetSubSurface = IDirectFBSurface_Layer_GetSubSurface;

     return DFB_OK;
}


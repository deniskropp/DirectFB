/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <endian.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "input.h"
#include "gfxcard.h"
#include "fbdev.h"
#include "layers.h"
#include "state.h"
#include "surfacemanager.h"
#include "windows.h"

#include "gfx/util.h"
#include "misc/mem.h"
#include "misc/util.h"


#define CURSORFILE         DATADIR"/cursor.dat"

#define MAX_DISPLAY_LAYERS 100

typedef struct {
     int                 num;
     DisplayLayerShared *layers[MAX_DISPLAY_LAYERS];
} CoreLayersField;

static CoreLayersField *layersfield = NULL;

/* FIXME: make static (and private) */
DisplayLayer    *dfb_layers      = NULL;


static int layer_id_pool = 0;

static DFBResult
dfb_layer_cursor_load_default( DisplayLayer *layer );

static DFBResult
dfb_layer_create_cursor_window( DisplayLayer *layer,
                                int           width,
                                int           height );


/** public **/

void
dfb_layers_add( DisplayLayer *layer )
{
     if (layersfield->num == MAX_LAYERS) {
          ERRORMSG( "DirectFB/Core/Layers: "
                    "Maximum number of layer reached!\n" );
          return;
     }

     layer->shared->id = layer_id_pool++;

     /* init skirmish */
     skirmish_init( &layer->shared->lock );

     /* add it to the list */
     if (dfb_layers) {
          DisplayLayer *last = dfb_layers;

          while (last->next)
               last = last->next;

          last->next = layer;
     }
     else
          dfb_layers = layer;

     layersfield->layers[ layersfield->num++ ] = layer->shared;
}

DFBResult
dfb_layers_initialize()
{
     DFBResult ret;

     layersfield = shcalloc( 1, sizeof (CoreLayersField) );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, layersfield, "Core/Layers" );
#endif

     ret = dfb_primarylayer_initialize();
     if (ret)
          return ret;

     /* FIXME: card layers for multiple apps */
#ifdef FUSION_FAKE
     ret = dfb_gfxcard_init_layers();
     if (ret)
          return ret;
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_layers_join()
{
     int       i;
     DFBResult ret;

     if (arena_get_shared_field( dfb_core->arena,
                                 (void**) &layersfield, "Core/Layers" ))
         return DFB_INIT;

     for (i=0; i<layersfield->num; i++) {
          DisplayLayer *layer;

          layer = DFBCALLOC( 1, sizeof(DisplayLayer) );

          layer->shared = layersfield->layers[i];

          /* add it to the list */
          if (dfb_layers) {
               DisplayLayer *last = dfb_layers;

               while (last->next)
                    last = last->next;

               last->next = layer;
          }
          else
               dfb_layers = layer;
     }

     /* FIXME: generalize this */
     ret = dfb_primarylayer_join();
     if (ret)
          return ret;

     /* FIXME: card layers */

     return DFB_OK;
}
#endif

DFBResult
dfb_layers_shutdown()
{
     while (dfb_layers) {
          DisplayLayer *l = dfb_layers;

          if (l->deinit)
               l->deinit( l );

          skirmish_destroy( &l->shared->lock );

          dfb_layers = l->next;
          DFBFREE( l );
     }

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_layers_leave()
{
     while (dfb_layers) {
          DisplayLayer *l = dfb_layers;

          dfb_layers = l->next;
          DFBFREE( l );
     }

     return DFB_OK;
}
#endif

#ifdef FUSION_FAKE
DFBResult
dfb_layers_suspend()
{
     DisplayLayer *l = dfb_layers;

     while (l) {
          l->Disable( l );

          l = l->next;
     }

     return DFB_OK;
}

DFBResult
dfb_layers_resume()
{
     DisplayLayer *l = dfb_layers;

     while (l) {
          /* bad check if layer was enabled before */
          if (l->shared->surface) {
               l->Enable( l );

               if (l->shared->windowstack)
                    dfb_windowstack_repaint_all( l->shared->windowstack );
          }

          l = l->next;
     }

     /* restore primary layer */
     dfb_layers->SetConfiguration (dfb_layers, NULL);

     return DFB_OK;
}
#endif

DFBResult
dfb_layer_lock( DisplayLayer *layer )
{
     if (skirmish_swoop( &layer->shared->lock ))
          return DFB_LOCKED;

     layer->shared->exclusive = 1;

     return DFB_OK;
}

DFBResult
dfb_layer_unlock( DisplayLayer *layer )
{
     skirmish_dismiss( &layer->shared->lock );

     layer->shared->exclusive = 0;

     dfb_windowstack_repaint_all( layer->shared->windowstack );

     return DFB_OK;
}

DFBResult
dfb_layer_enable( DisplayLayer *layer )
{
     return layer->Enable( layer );
}

CoreSurface *
dfb_layer_surface( DisplayLayer *layer )
{
     return layer->shared->surface;
}

DFBResult
dfb_layer_set_configuration( DisplayLayer          *layer,
                             DFBDisplayLayerConfig *config )
{
     DFBResult              ret;
     DFBDisplayLayerConfig  new_config;
     DisplayLayerShared    *shared = layer->shared;

     /* set all flags in new configuration */
     new_config.flags = DLCONF_BUFFERMODE | DLCONF_HEIGHT |
                        DLCONF_OPTIONS | DLCONF_PIXELFORMAT | DLCONF_WIDTH;

     /* fill new configuration depending on flags set by the caller */
     if (config->flags & DLCONF_BUFFERMODE)
          new_config.buffermode = config->buffermode;
     else
          new_config.buffermode = layer->shared->buffermode;

     if (config->flags & DLCONF_HEIGHT)
          new_config.height = config->height;
     else
          new_config.height = layer->shared->height;

     if (config->flags & DLCONF_OPTIONS)
          new_config.options = config->options;
     else
          new_config.options = layer->shared->options;

     if (config->flags & DLCONF_PIXELFORMAT)
          new_config.pixelformat = config->pixelformat;
     else
          new_config.pixelformat = layer->shared->surface->format;

     if (config->flags & DLCONF_WIDTH)
          new_config.width = config->width;
     else
          new_config.width = layer->shared->width;

     /* check if new configuration is supported */
     ret = layer->TestConfiguration( layer, &new_config, NULL );
     if (ret)
          return ret;

     /* FIXME: implement buffer mode changes */
     if (shared->buffermode != new_config.buffermode) {
          ONCE("Changing the buffermode of layers is unimplemented!");
          return DFB_UNIMPLEMENTED;
     }

     if (shared->width           != new_config.width   ||
         shared->height          != new_config.height  ||
         shared->options         != new_config.options ||
         shared->surface->format != new_config.pixelformat)
     {
          /* FIXME: write surface management functions
                    for easier configuration changes */

          ret = dfb_surface_reformat( shared->surface,
                                      new_config.width,
                                      new_config.height,
                                      new_config.pixelformat );
          if (ret)
               return ret;

          if (new_config.options & DLOP_INTERLACED_VIDEO)
               shared->surface->caps |= DSCAPS_INTERLACED;
          else
               shared->surface->caps &= ~DSCAPS_INTERLACED;

          shared->options = new_config.options;
          shared->width   = new_config.width;
          shared->height  = new_config.height;

          shared->windowstack->cursor_region.x1 = 0;
          shared->windowstack->cursor_region.y1 = 0;
          shared->windowstack->cursor_region.x2 = shared->width - 1;
          shared->windowstack->cursor_region.y2 = shared->height - 1;

          /* apply new configuration, this shouldn't fail */
          ret = layer->SetConfiguration( layer, &new_config );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_enable( DisplayLayer *layer, int enable )
{
     DFBResult           ret    = DFB_OK;
     DisplayLayerShared *shared = layer->shared;

     if (enable) {
          if (!shared->windowstack->cursor_window)        /* no cursor yet?      */
               ret = dfb_layer_cursor_load_default( layer ); /* install the default */

          if (ret == DFB_OK)
               ret = dfb_window_set_opacity( shared->windowstack->cursor_window,
                                             shared->windowstack->cursor_opacity );
          if (ret == DFB_OK)
               shared->windowstack->cursor = 1;
     }
     else {
          if (shared->windowstack->cursor_window)
               ret = dfb_window_set_opacity( shared->windowstack->cursor_window, 0 );

          if (ret == DFB_OK)
               shared->windowstack->cursor = 0;
     }
     return ret;
}

DFBResult
dfb_layer_cursor_set_opacity( DisplayLayer *layer, __u8 opacity )
{
     DFBResult        ret   = DFB_OK;
     CoreWindowStack *stack = layer->shared->windowstack;

     if (stack->cursor) {
          if (!stack->cursor_window)                         /* no cursor yet?  */
               ret = dfb_layer_cursor_load_default( layer ); /* install default */

          if (ret == DFB_OK)
               ret = dfb_window_set_opacity( stack->cursor_window, opacity );

     }

     if (ret == DFB_OK)
          stack->cursor_opacity = opacity;

     return ret;
}

DFBResult
dfb_layer_cursor_set_shape( DisplayLayer *layer,
                            CoreSurface  *shape,
                            int           hot_x,
                            int           hot_y )
{
     int                 dx, dy;
     DisplayLayerShared *shared = layer->shared;

     if (!shared->windowstack->cursor_window) {
          DFBResult ret =
               dfb_layer_create_cursor_window( layer, shape->width, shape->height );

          if (ret)
               return ret;
     }
     else if (shared->windowstack->cursor_window->width != shape->width  ||
              shared->windowstack->cursor_window->height != shape->height)
     {
          dfb_window_resize( shared->windowstack->cursor_window,
                             shape->width, shape->height );
     }

     dfb_gfx_copy( shape, shared->windowstack->cursor_window->surface, NULL );

     dx = shared->windowstack->cx - hot_x - shared->windowstack->cursor_window->x;
     dy = shared->windowstack->cy - hot_y - shared->windowstack->cursor_window->y;

     if (dx || dy)
          dfb_window_move( shared->windowstack->cursor_window, dx, dy );
     else
          dfb_window_repaint( shared->windowstack->cursor_window, NULL );

     return DFB_OK;
}

DFBResult
dfb_layer_cursor_warp( DisplayLayer *layer, int x, int y )
{
     int                 dx, dy;
     DisplayLayerShared *shared = layer->shared;

     if (x < 0  ||  y < 0  ||  x >= shared->width  ||  y >= shared->height)
          return DFB_INVARG;

     dx = x - shared->windowstack->cx;
     dy = y - shared->windowstack->cy;

     dfb_windowstack_handle_motion( shared->windowstack, dx, dy );

     return DFB_OK;
}


/** internal **/

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult
dfb_layer_cursor_load_default( DisplayLayer *layer )
{
     DFBResult           ret;
     int                 i;
     unsigned int        pitch;
     void               *data;
     FILE               *f;
     DisplayLayerShared *shared = layer->shared;

     if (!shared->windowstack->cursor_window) {
          ret = dfb_layer_create_cursor_window( layer, 40, 40 );
          if (ret)
               return ret;
     }

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2dfb( errno );
          PERRORMSG( "`" CURSORFILE "` could not be opened!\n" );
          return ret;
     }

     /* lock the surface of the window */
     ret = dfb_surface_soft_lock( shared->windowstack->cursor_window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "cannot lock the surface for cursor window data!\n" );
          fclose( f );

          return ret;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, 40*4, 1, f ) != 1) {
               ret = errno2dfb( errno );

               ERRORMSG( "DirectFB/core/layers: "
                         "unexpected end or read error of cursor data!\n" );

               dfb_surface_unlock( shared->windowstack->cursor_window->surface, 0 );
               fclose( f );

               return ret;
          }
#if __BYTE_ORDER == __BIG_ENDIAN
          {
               int i = 40;
               __u32 *tmp_data = data;

               while(i--) {
                    *tmp_data = (*tmp_data & 0xFF000000) >> 24 |
                                (*tmp_data & 0x00FF0000) >>  8 |
                                (*tmp_data & 0x0000FF00) <<  8 |
                                (*tmp_data & 0x000000FF) << 24;
                    ++tmp_data;
               }
          }
#endif
          data += pitch;
     }

     fclose( f );
     dfb_surface_unlock( shared->windowstack->cursor_window->surface, 0 );

     dfb_window_repaint( shared->windowstack->cursor_window, NULL );

     return DFB_OK;
}

static DFBResult
dfb_layer_create_cursor_window( DisplayLayer *layer,
                                int           width,
                                int           height )
{
     CoreWindow         *cursor;
     DisplayLayerShared *shared = layer->shared;

     /* reinitialization check */
     if (shared->windowstack->cursor_window) {
          BUG( "already created a cursor for this layer" );
          return DFB_BUG;
     }

     shared->windowstack->cursor_opacity = 0xFF;
     shared->windowstack->cx = shared->width / 2;
     shared->windowstack->cy = shared->height / 2;

     /* create a super-top-most_event-and-focus-less window */
     cursor = dfb_window_create( shared->windowstack,
                                 shared->windowstack->cx,
                                 shared->windowstack->cy, width, height,
                                 DWHC_GHOST | DWCAPS_ALPHACHANNEL,
                                 DSPF_UNKNOWN );
     if (!cursor) {
          ERRORMSG( "DirectFB/core/layers: "
                    "failed creating a window for software cursor!\n" );
          return DFB_FAILURE;
     }

     dfb_window_init( cursor );
     dfb_window_set_opacity( cursor, shared->windowstack->cursor_opacity );

     shared->windowstack->cursor_window  = cursor;

     return DFB_OK;
}


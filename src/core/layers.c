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
DisplayLayer    *layers      = NULL;


static int layer_id_pool = 0;

static DFBResult layer_cursor_load_default( DisplayLayer *layer );
static DFBResult layer_create_cursor_window( DisplayLayer *layer,
                                             int width, int height );


/** public **/

void layers_add( DisplayLayer *layer )
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
     if (layers) {
          DisplayLayer *last = layers;

          while (last->next)
               last = last->next;

          last->next = layer;
     }
     else
          layers = layer;

     layersfield->layers[ layersfield->num++ ] = layer->shared;
}

DFBResult layers_initialize()
{
     DFBResult ret;

     layersfield = shcalloc( 1, sizeof (CoreLayersField) );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, layersfield, "Core/Layers" );
#endif

     ret = primarylayer_initialize();
     if (ret)
          return ret;

     /* FIXME: card layers for multiple apps */
#ifdef FUSION_FAKE
     ret = gfxcard_init_layers();
     if (ret)
          return ret;
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult layers_join()
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
          if (layers) {
               DisplayLayer *last = layers;

               while (last->next)
                    last = last->next;

               last->next = layer;
          }
          else
               layers = layer;
     }

     /* FIXME: generalize this */
     ret = primarylayer_join();
     if (ret)
          return ret;

     /* FIXME: card layers */

     return DFB_OK;
}
#endif

DFBResult layers_shutdown()
{
     while (layers) {
          DisplayLayer *l = layers;

          if (l->deinit)
               l->deinit( l );

          skirmish_destroy( &l->shared->lock );

          layers = l->next;
          DFBFREE( l );
     }

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult layers_leave()
{
     while (layers) {
          DisplayLayer *l = layers;

          layers = l->next;
          DFBFREE( l );
     }

     return DFB_OK;
}
#endif


DFBResult layer_lock( DisplayLayer *layer )
{
     if (skirmish_swoop( &layer->shared->lock ))
          return DFB_LOCKED;

     layer->shared->exclusive = 1;

     return DFB_OK;
}

DFBResult layer_unlock( DisplayLayer *layer )
{
     skirmish_dismiss( &layer->shared->lock );

     layer->shared->exclusive = 0;

     windowstack_repaint_all( layer->shared->windowstack );

     return DFB_OK;
}

DFBResult layer_enable( DisplayLayer *layer )
{
     return layer->Enable( layer );
}

CoreSurface *layer_surface( DisplayLayer *layer )
{
     return layer->shared->surface;
}

DFBResult layer_cursor_enable( DisplayLayer *layer, int enable )
{
     DFBResult ret = DFB_OK;

     if (enable) {
          if (!layer->shared->windowstack->cursor_window)        /* no cursor yet?      */
               ret = layer_cursor_load_default( layer ); /* install the default */

          if (ret == DFB_OK)
               ret = window_set_opacity( layer->shared->windowstack->cursor_window,
                                         layer->shared->windowstack->cursor_opacity );
          if (ret == DFB_OK)
               layer->shared->windowstack->cursor = 1;
     }
     else {
          if (layer->shared->windowstack->cursor_window)
               ret = window_set_opacity( layer->shared->windowstack->cursor_window, 0 );

          if (ret == DFB_OK)
               layer->shared->windowstack->cursor = 0;
     }
     return ret;
}

DFBResult layer_cursor_set_opacity( DisplayLayer *layer, __u8 opacity )
{
     DFBResult        ret   = DFB_OK;
     CoreWindowStack *stack = layer->shared->windowstack;

     if (stack->cursor) {
          if (!stack->cursor_window)                      /* no cursor yet?  */
               ret = layer_cursor_load_default( layer );  /* install default */

          if (ret == DFB_OK)
               ret = window_set_opacity( stack->cursor_window, opacity );

     }

     if (ret == DFB_OK)
          stack->cursor_opacity = opacity;

     return ret;
}

DFBResult layer_cursor_set_shape( DisplayLayer *layer, CoreSurface *shape,
                                  int hot_x, int hot_y )
{
     int dx, dy;

     if (!layer->shared->windowstack->cursor_window) {
          DFBResult ret =
               layer_create_cursor_window( layer, shape->width, shape->height );

          if (ret)
               return ret;
     }
     else if (layer->shared->windowstack->cursor_window->width != shape->width  ||
              layer->shared->windowstack->cursor_window->height != shape->height)
     {
          window_resize( layer->shared->windowstack->cursor_window,
                         shape->width, shape->height );
     }

     gfx_copy( shape, layer->shared->windowstack->cursor_window->surface, NULL );

     dx = layer->shared->windowstack->cx - hot_x - layer->shared->windowstack->cursor_window->x;
     dy = layer->shared->windowstack->cy - hot_y - layer->shared->windowstack->cursor_window->y;

     if (dx || dy)
          window_move( layer->shared->windowstack->cursor_window, dx, dy );
     else
          window_repaint( layer->shared->windowstack->cursor_window, NULL );

     return DFB_OK;
}

DFBResult layer_cursor_warp( DisplayLayer *layer, int x, int y )
{
     int dx, dy;

     if (x < 0  ||  y < 0  ||  x >= layer->shared->width  ||  y >= layer->shared->height)
          return DFB_INVARG;

     dx = x - layer->shared->windowstack->cx;
     dy = y - layer->shared->windowstack->cy;

     windowstack_handle_motion( layer->shared->windowstack, dx, dy );

     return DFB_OK;
}


/** internal **/

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult layer_cursor_load_default( DisplayLayer *layer )
{
     DFBResult ret;
     int i, pitch;
     void *data;
     FILE *f;

     if (!layer->shared->windowstack->cursor_window) {
          ret = layer_create_cursor_window( layer, 40, 40 );
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
     surfacemanager_lock( gfxcard_surface_manager() );
     ret = surface_software_lock( layer->shared->windowstack->cursor_window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     surfacemanager_unlock( gfxcard_surface_manager() );
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

               surface_unlock( layer->shared->windowstack->cursor_window->surface, 0 );
               fclose( f );

               return ret;
          }
#ifdef __BIG_ENDIAN__
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
     surface_unlock( layer->shared->windowstack->cursor_window->surface, 0 );

     window_repaint( layer->shared->windowstack->cursor_window, NULL );

     return DFB_OK;
}

static DFBResult layer_create_cursor_window( DisplayLayer *layer,
                                             int width, int height )
{
     CoreWindow *cursor;

     /* reinitialization check */
     if (layer->shared->windowstack->cursor_window) {
          BUG( "already created a cursor for this layer" );
          return DFB_BUG;
     }

     layer->shared->windowstack->cursor_opacity = 0xFF;
     layer->shared->windowstack->cx = layer->shared->width / 2;
     layer->shared->windowstack->cy = layer->shared->height / 2;

     /* create a super-top-most_event-and-focus-less window */
     cursor = window_create( layer->shared->windowstack, layer->shared->windowstack->cx,
                             layer->shared->windowstack->cy, width, height,
                             DWHC_GHOST | DWCAPS_ALPHACHANNEL );
     if (!cursor) {
          ERRORMSG( "DirectFB/core/layers: "
                    "failed creating a window for software cursor!\n" );
          return DFB_FAILURE;
     }

     window_init( cursor );
     window_set_opacity( cursor, layer->shared->windowstack->cursor_opacity );

     layer->shared->windowstack->cursor_window  = cursor;

     return DFB_OK;
}


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

#include <pthread.h>
#include <errno.h>

#include <gfx/util.h>
#include <misc/util.h>

#include <directfb.h>

#include "core.h"
#include "coredefs.h"
#include "layers.h"

#include "input.h"
#include "gfxcard.h"

#include "windows.h"



#define CURSORFILE     DATADIR"/cursor.dat"

static int layer_id_pool = 0;

DisplayLayer *layers = NULL;

DFBResult layer_create_cursor_window( DisplayLayer *layer,
                                      int width, int height );
DFBResult layer_cursor_load_default( DisplayLayer *layer );

/*
 * added to the cleanup stack
 */
void layers_remove_all()
{
     while (layers) {
          DisplayLayer *l = layers;

          if (l->deinit)
               l->deinit( l );

          layers = l->next;
          free( l );
     }
}

void layers_add( DisplayLayer *layer )
{
     layer->id = layer_id_pool++;

     /* init mutex */
     pthread_mutex_init( &layer->lock, NULL );

     /* add it to the list, if the list was empty add the cleanup function */
     if (layers) {
          DisplayLayer *last = layers;

          while (last->next)
               last = last->next;

          last->next = layer;
     }
     else {
          layers = layer;
          core_cleanup_push( layers_remove_all );
     }
}

DFBResult layers_suspend()
{
     DisplayLayer *l = layers;
     
     DEBUGMSG( "DirectFB/core/layers: suspending...\n" );
     
     while (l) {
          l->Disable( l );

          l = l->next;
     }

     DEBUGMSG( "DirectFB/core/layers: ...suspended\n" );
     
     return DFB_OK;
}

DFBResult layers_resume()
{
     DisplayLayer *l = layers;
     
     DEBUGMSG( "DirectFB/core/layers: resuming...\n" );
     
     while (l) {
          /* bad check if layer was enabled before */
          if (l->surface) {
               l->Enable( l );

               if (l->windowstack)
                    windowstack_repaint_all( l->windowstack );
          }

          l = l->next;
     }

     DEBUGMSG( "DirectFB/core/layers: ...resumed\n" );
     
     return DFB_OK;
}

DFBResult layer_lock( DisplayLayer *layer )
{
     if (pthread_mutex_trylock( &layer->lock ))
          return DFB_LOCKED;

     layer->exclusive = 1;

     return DFB_OK;
}

DFBResult layer_unlock( DisplayLayer *layer )
{
     pthread_mutex_unlock( &layer->lock );

     layer->exclusive = 0;
     
     windowstack_repaint_all( layer->windowstack );

     return DFB_OK;
}

DFBResult layer_cursor_enable( DisplayLayer *layer, int enable )
{
     DFBResult ret = DFB_OK;

     if (!layer->windowstack->cursor) {                 /* no cursor yet? */
          if (enable)
               ret = layer_cursor_load_default( layer );/* install the default*/
          else
               return DFB_OK;                           /* we dont have it,
                                                           they dont want it */
     }

     if (ret == DFB_OK)
          ret = window_set_opacity( layer->windowstack->cursor,
                                    enable ? 0xFF : 0 );

     return ret;
}

DFBResult layer_cursor_set_shape( DisplayLayer *layer, CoreSurface *shape,
                                  int hot_x, int hot_y )
{
     int dx, dy;

     if (!layer->windowstack->cursor) {
          DFBResult ret =
               layer_create_cursor_window( layer, shape->width, shape->height );

          if (ret)
               return ret;
     }
     else if (layer->windowstack->cursor->width != shape->width  ||
              layer->windowstack->cursor->height != shape->height)
     {
          window_resize( layer->windowstack->cursor,
                         shape->width, shape->height );
     }

     gfx_copy( shape, layer->windowstack->cursor->surface, NULL );

     dx = layer->windowstack->cx - hot_x - layer->windowstack->cursor->x;
     dy = layer->windowstack->cy - hot_y - layer->windowstack->cursor->y;

     if (dx || dy)
          window_move( layer->windowstack->cursor, dx, dy );
     else
          window_repaint( layer->windowstack->cursor, NULL );

     return DFB_OK;
}

DFBResult layer_cursor_warp( DisplayLayer *layer, int x, int y )
{
     int dx, dy;

     if (x < 0  ||  y < 0  ||  x >= layer->width  ||  y >= layer->height)
          return DFB_INVARG;

     dx = x - layer->windowstack->cx;
     dy = y - layer->windowstack->cy;

     windowstack_handle_motion( layer->windowstack, dx, dy );

     return DFB_OK;
}

DFBResult layer_create_cursor_window( DisplayLayer *layer,
                                      int width, int height )
{
     CoreWindow *cursor;

     /* reinitialization check */
     if (layer->windowstack->cursor) {
          BUG( "already created a cursor for this layer" );
          return DFB_BUG;
     }

     /* create a super-top-most_event-and-focus-less window */
     cursor = window_create( layer->windowstack, layer->windowstack->cx,
                             layer->windowstack->cy, width, height,
                             DWHC_GHOST | DWCAPS_ALPHACHANNEL );
     if (!cursor) {
          ERRORMSG( "DirectFB/core/layers: "
                    "failed creating a window for software cursor!\n" );
          return DFB_FAILURE;
     }

     window_init( cursor );

     layer->windowstack->cursor = cursor;

     return DFB_OK;
}

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
DFBResult layer_cursor_load_default( DisplayLayer *layer )
{
     DFBResult ret;
     int i, pitch;
     void *data;
     FILE *f;

     if (!layer->windowstack->cursor) {
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
     ret = surface_soft_lock( layer->windowstack->cursor->surface,
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

               surface_unlock( layer->windowstack->cursor->surface, 0 );
               fclose( f );

               return ret;
          }
#ifdef __BIG_ENDIAN__
          {
               int i = 40;                              
               __u32 *tmp_data = data;

               while(i--) {
                    *tmp_data++ = (*tmp_data & 0xFF000000) >> 24 |
                                  (*tmp_data & 0x00FF0000) >>  8 |
                                  (*tmp_data & 0x0000FF00) <<  8 |
                                  (*tmp_data & 0x000000FF) << 24;
               }
          }
#endif          
          data += pitch;
     }

     fclose( f );
     surface_unlock( layer->windowstack->cursor->surface, 0 );

     return DFB_OK;
}

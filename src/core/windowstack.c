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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <pthread.h>

#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>


#define CURSORFILE         DATADIR"/cursor.dat"


typedef struct {
     DirectLink       link;

     DFBInputDeviceID id;
     GlobalReaction   reaction;
} StackDevice;

typedef struct {
     DirectLink                  link;

     DFBInputDeviceKeySymbol     symbol;
     DFBInputDeviceModifierMask  modifiers;

     CoreWindow                 *owner;
} GrabbedKey;


static DFBResult   load_default_cursor ( CoreWindowStack *stack );
static DFBResult   create_cursor_window( CoreWindowStack *stack,
                                         int              width,
                                         int              height );

static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx );


/******************************************************************************/

/*
 * Allocates and initializes a window stack.
 */
CoreWindowStack*
dfb_windowstack_create( CoreLayerContext *context )
{
     CoreWindowStack *stack;

     D_ASSERT( context != NULL );

     /* Allocate window stack data (completely shared) */
     stack = (CoreWindowStack*) SHCALLOC( 1, sizeof(CoreWindowStack) );
     if (!stack)
          return NULL;

     /* Store context which we belong to. */
     stack->context = context;

     /* Set default acceleration */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Set default background mode. */
     stack->bg.mode = DLBM_COLOR;

     dfb_wm_init_stack( stack );

     /* Attach to all input devices */
     dfb_input_enumerate_devices( stack_attach_devices, stack, DICAPS_ALL );

     return stack;
}

void
dfb_windowstack_destroy( CoreWindowStack *stack )
{
     DirectLink *l;

     D_ASSERT( stack != NULL );

     /* Detach all input devices. */
     l = stack->devices;
     while (l) {
          DirectLink  *next   = l->next;
          StackDevice *device = (StackDevice*) l;

          dfb_input_detach_global( dfb_input_device_at( device->id ),
                                   &device->reaction );

          SHFREE( device );

          l = next;
     }

     /* Unlink cursor window. */
     /*if (stack->cursor.window)
          dfb_window_unlink( &stack->cursor.window );*/

     dfb_wm_close_stack( stack, true );

     /* detach listener from background surface and unlink it */
     if (stack->bg.image) {
          dfb_surface_detach_global( stack->bg.image,
                                     &stack->bg.image_reaction );

          dfb_surface_unlink( &stack->bg.image );
     }

     /* Free stack data. */
     SHFREE( stack );
}

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Store the width and height of the stack */
     stack->width  = width;
     stack->height = height;

     /* Setup new cursor clipping region */
     stack->cursor.region.x1 = 0;
     stack->cursor.region.y1 = 0;
     stack->cursor.region.x2 = width - 1;
     stack->cursor.region.y2 = height - 1;

     /* Notify the window manager. */
     dfb_wm_resize_stack( stack, width, height );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

/*
 * Prohibit access to the window stack data.
 * Waits until stack is accessible.
 */
DirectResult
dfb_windowstack_lock( CoreWindowStack *stack )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_lock( stack->context );
}

/*
 * Allow access to the window stack data.
 */
DirectResult
dfb_windowstack_unlock( CoreWindowStack *stack )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_unlock( stack->context );
}

DFBResult
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBResult ret;
     DFBRegion region;

     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     region.x1 = 0;
     region.y1 = 0;
     region.x2 = stack->width  - 1;
     region.y2 = stack->height - 1;

     ret = dfb_wm_update_stack( stack, &region, 0 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

/*
 * background handling
 */

DFBResult
dfb_windowstack_set_background_mode ( CoreWindowStack               *stack,
                                      DFBDisplayLayerBackgroundMode  mode )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* nothing to do if mode is the same */
     if (mode != stack->bg.mode) {
          /* for these modes a surface is required */
          if ((mode == DLBM_IMAGE || mode == DLBM_TILE) && !stack->bg.image) {
               dfb_windowstack_unlock( stack );
               return DFB_MISSINGIMAGE;
          }

          /* set new mode */
          stack->bg.mode = mode;

          /* force an update of the window stack */
          if (mode != DLBM_DONTCARE)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_image( CoreWindowStack *stack,
                                      CoreSurface     *image )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( image != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* if the surface is changed */
     if (stack->bg.image != image) {
          /* detach listener from old surface and unlink it */
          if (stack->bg.image) {
               dfb_surface_detach_global( stack->bg.image,
                                          &stack->bg.image_reaction );

               dfb_surface_unlink( &stack->bg.image );
          }

          /* link surface object */
          dfb_surface_link( &stack->bg.image, image );

          /* attach listener to new surface */
          dfb_surface_attach_global( image,
                                     DFB_WINDOWSTACK_BACKGROUND_IMAGE_LISTENER,
                                     stack, &stack->bg.image_reaction );
     }

     /* force an update of the window stack */
     if (stack->bg.mode == DLBM_IMAGE || stack->bg.mode == DLBM_TILE)
          dfb_windowstack_repaint_all( stack );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_color( CoreWindowStack *stack,
                                      DFBColor        *color )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( color != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* do nothing if color didn't change */
     if (!DFB_COLOR_EQUAL( stack->bg.color, *color )) {
          /* set new color */
          stack->bg.color = *color;

          /* force an update of the window stack */
          if (stack->bg.mode == DLBM_COLOR)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/*
 * cursor control
 */

DFBResult
dfb_windowstack_cursor_enable( CoreWindowStack *stack, bool enable )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.set = true;

     if (dfb_config->no_cursor) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     if (enable) {
          if (!stack->cursor.window) {
               DFBResult        ret;
               CoreWindowConfig config;

               ret = load_default_cursor( stack );
               if (ret) {
                    dfb_windowstack_unlock( stack );
                    return ret;
               }

               config.events  = 0;
               config.options = DWOP_ALPHACHANNEL | DWOP_GHOST;

               dfb_wm_set_window_config( stack->cursor.window, &config,
                                         CWCF_EVENTS | CWCF_OPTIONS );
          }

          dfb_window_set_opacity( stack->cursor.window,
                                  stack->cursor.opacity );

          stack->cursor.enabled = 1;
     }
     else {
          if (stack->cursor.window)
               dfb_window_set_opacity( stack->cursor.window, 0 );

          stack->cursor.enabled = 0;
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_opacity( CoreWindowStack *stack, __u8 opacity )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (stack->cursor.enabled) {
          D_ASSERT( stack->cursor.window );

          dfb_window_set_opacity( stack->cursor.window, opacity );
     }

     stack->cursor.opacity = opacity;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_shape( CoreWindowStack *stack,
                                  CoreSurface     *shape,
                                  int              hot_x,
                                  int              hot_y )
{
     DFBResult              ret;
     int                    dx, dy;
     CoreWindow            *cursor;
     CoreWindowConfig       config;
     CoreWindowConfigFlags  flags = CWCF_NONE;

     D_ASSERT( stack != NULL );
     D_ASSERT( shape != NULL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     cursor = stack->cursor.window;
     if (!cursor) {
          ret = create_cursor_window( stack, shape->width, shape->height );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }
          cursor = stack->cursor.window;

          config.events  = 0;
          config.options = DWOP_ALPHACHANNEL | DWOP_GHOST;
          config.opacity = stack->cursor.opacity;

          flags |= CWCF_EVENTS | CWCF_OPTIONS | CWCF_OPACITY;
     }
     else if (cursor->config.bounds.w != shape->width || cursor->config.bounds.h != shape->height) {
          config.bounds.w = shape->width;
          config.bounds.h = shape->height;

          ret = dfb_wm_set_window_config( cursor, &config, CWCF_SIZE );
          if (ret) {
               D_DERROR( ret, "DirectFB/Core/WindowStack: Could not "
                         "resize the cursor window (%dx%d!\n", shape->width, shape->height );
               return ret;
          }
     }

     if (DFB_PIXELFORMAT_HAS_ALPHA( shape->format ) && dfb_config->translucent_windows) {
          if (cursor->config.options & DWOP_COLORKEYING) {
               config.options = (cursor->config.options & ~DWOP_COLORKEYING) | DWOP_ALPHACHANNEL;
               flags |= CWCF_OPTIONS;
          }
     }
     else {
          __u32 key = dfb_color_to_pixel( cursor->surface->format, 0xff, 0x00, 0xff );

          if (config.color_key != key) {
               config.color_key = key;
               flags |= CWCF_COLOR_KEY;
          }

          if (cursor->config.options & DWOP_ALPHACHANNEL) {
               config.options = (cursor->config.options & ~DWOP_ALPHACHANNEL) | DWOP_COLORKEYING;
               flags |= CWCF_OPTIONS;
          }
     }

     dx = stack->cursor.x - hot_x - cursor->config.bounds.x;
     dy = stack->cursor.y - hot_y - cursor->config.bounds.y;

     if (dx || dy) {
          config.bounds.x = cursor->config.bounds.x + dx;
          config.bounds.y = cursor->config.bounds.y + dy;
          flags |= CWCF_POSITION;
     }


     dfb_gfx_copy( shape, cursor->surface, NULL );


     if (flags) {
          ret = dfb_wm_set_window_config( cursor, &config, flags );
          if (ret) {
               D_DERROR( ret, "DirectFB/Core/WindowStack: "
                         "Could not set window configuration (flags 0x%08x)!\n", flags );
               return ret;
          }
     }
     else
          dfb_window_repaint( stack->cursor.window, NULL, DSFLIP_NONE );


     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_warp( CoreWindowStack *stack, int x, int y )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     dfb_wm_warp_cursor( stack, x, y );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_acceleration( CoreWindowStack *stack,
                                         int              numerator,
                                         int              denominator,
                                         int              threshold )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.numerator   = numerator;
     stack->cursor.denominator = denominator;
     stack->cursor.threshold   = threshold;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_get_cursor_position( CoreWindowStack *stack, int *x, int *y )
{
     D_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (x)
          *x = stack->cursor.x;

     if (y)
          *y = stack->cursor.y;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/******************************************************************************/

ReactionResult
_dfb_windowstack_inputdevice_listener( const void *msg_data,
                                       void       *ctx )
{
     const DFBInputEvent *event = msg_data;
     CoreWindowStack     *stack = ctx;

     D_ASSERT( msg_data != NULL );
     D_ASSERT( ctx != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return RS_REMOVE;

     /* Call the window manager to dispatch the event. */
     if (dfb_layer_context_active( stack->context ))
          dfb_wm_process_input( stack, event );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return RS_OK;
}

/*
 * listen to the background image
 */
ReactionResult
_dfb_windowstack_background_image_listener( const void *msg_data,
                                            void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CoreWindowStack               *stack        = ctx;

     D_ASSERT( notification != NULL );
     D_ASSERT( stack != NULL );

     if (notification->flags & CSNF_DESTROY) {
          if (stack->bg.image == notification->surface) {
               D_ERROR("DirectFB/core/layers: Surface for background vanished.\n");

               stack->bg.mode  = DLBM_COLOR;
               stack->bg.image = NULL;

               dfb_windowstack_repaint_all( stack );
          }

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}

/******************************************************************************/

/*
 * internals
 */

static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx )
{
     StackDevice     *dev;
     CoreWindowStack *stack = (CoreWindowStack*) ctx;

     dev = SHCALLOC( 1, sizeof(StackDevice) );
     if (!dev) {
          D_ERROR( "DirectFB/core/windows: Could not allocate %d bytes\n",
                    sizeof(StackDevice) );
          return DFENUM_CANCEL;
     }

     dev->id = dfb_input_device_id( device );

     direct_list_prepend( &stack->devices, &dev->link );

     dfb_input_attach_global( device, DFB_WINDOWSTACK_INPUTDEVICE_LISTENER,
                              ctx, &dev->reaction );

     return DFENUM_OK;
}

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult
load_default_cursor( CoreWindowStack *stack )
{
     DFBResult        ret;
     int              i;
     int              pitch;
     void            *data;
     FILE            *f;
     CoreWindow      *window;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.window) {
          ret = create_cursor_window( stack, 40, 40 );
          if (ret)
               return ret;
     }

     window = stack->cursor.window;

     /* lock the surface of the window */
     ret = dfb_surface_soft_lock( window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     if (ret) {
          D_ERROR( "DirectFB/core/layers: "
                    "cannot lock the surface for cursor window data!\n" );
          return ret;
     }

     /* initialize as empty cursor */
     memset( data, 0, 40 * pitch);

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2result( errno );

          /* ignore a missing cursor file */
          if (ret == DFB_FILENOTFOUND)
               ret = DFB_OK;
          else
               D_PERROR( "`" CURSORFILE "` could not be opened!\n" );

          goto finish;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, MIN (40*4, pitch), 1, f ) != 1) {
               ret = errno2result( errno );

               D_ERROR( "DirectFB/core/layers: "
                         "unexpected end or read error of cursor data!\n" );

               goto finish;
          }
#ifdef WORDS_BIGENDIAN
          {
               int i = MIN (40, pitch/4);
               __u32 *tmp_data = data;

               while (i--) {
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

finish:
     if (f)
          fclose( f );

     dfb_surface_unlock( window->surface, 0 );

     dfb_window_repaint( window, NULL, DSFLIP_NONE );

     return ret;
}

static DFBResult
create_cursor_window( CoreWindowStack *stack,
                      int              width,
                      int              height )
{
     DFBResult   ret;
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->cursor.window == NULL );

     stack->cursor.x       = stack->width  / 2;
     stack->cursor.y       = stack->height / 2;
     stack->cursor.opacity = 0xFF;

     /* create a super-top-most event-and-focus-less window */
     ret = dfb_window_create( stack, stack->cursor.x, stack->cursor.y, width, height,
                              DWHC_TOPMOST | DWCAPS_ALPHACHANNEL | DWCAPS_NODECORATION,
                              DSCAPS_NONE, DSPF_UNKNOWN, &window );
     if (ret) {
          D_ERROR( "DirectFB/Core/layers: "
                    "Failed creating a window for software cursor!\n" );
          return ret;
     }

     stack->cursor.window = window;

     dfb_window_inherit( window, stack->context );

     dfb_window_unref( window );

     return DFB_OK;
}


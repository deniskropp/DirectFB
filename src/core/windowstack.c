/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#include <config.h>

#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/reactor.h>
#include <fusion/shmalloc.h>

#include <core/input.h>
#include <core/layer_context.h>
#include <core/layers_internal.h>
#include <core/surface.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <misc/conf.h>

#include <gfx/convert.h>
#include <gfx/util.h>


#define CURSORFILE  DATADIR"/cursor.dat"


D_DEBUG_DOMAIN( Core_WindowStack, "Core/WindowStack", "DirectFB Core WindowStack" );

/**********************************************************************************************************************/

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

typedef struct {
     DirectLink                  link;
     void                       *ctx;
} Stack_Container;

static  DirectLink      *stack_containers = NULL;
static  pthread_mutex_t  stack_containers_lock = PTHREAD_MUTEX_INITIALIZER;

/**********************************************************************************************************************/

static DFBResult load_default_cursor  ( CoreDFB         *core,
                                        CoreWindowStack *stack );
static DFBResult create_cursor_surface( CoreWindowStack *stack,
                                        int              width,
                                        int              height );

/**********************************************************************************************************************/

static DFBEnumerationResult stack_attach_devices( CoreInputDevice *device,
                                                  void            *ctx );

static DFBEnumerationResult stack_detach_devices( CoreInputDevice *device,
                                                  void            *ctx );

/**********************************************************************************************************************/

// Implement stack_containers_XXX function family to maintain the connections
// between windowstacks and input devices.
static void
stack_containers_add(CoreWindowStack *p)
{
     Stack_Container    *stack_cntr;

     D_DEBUG_AT( Core_WindowStack, "Enter:%s()\n", __FUNCTION__);

     pthread_mutex_lock( &stack_containers_lock );

     stack_cntr = (Stack_Container *) D_CALLOC(1, sizeof(Stack_Container));
     if (stack_cntr != NULL) {
          stack_cntr->ctx = (void *)p;
          direct_list_append(&stack_containers, &stack_cntr->link);
     }
     else{
          D_ERROR( "Core/WindowStack: stack_cntr = NULL\n");
     }

     pthread_mutex_unlock( &stack_containers_lock );
}

static void
stack_containers_remove(CoreWindowStack *p)
{
     Stack_Container    *stack_cntr = NULL;

     D_DEBUG_AT( Core_WindowStack, "Enter:%s()\n", __FUNCTION__);

     pthread_mutex_lock( &stack_containers_lock );

     direct_list_foreach(stack_cntr, stack_containers) {
          if((void *)p == stack_cntr->ctx) {
               direct_list_remove(&stack_containers, &stack_cntr->link);
               D_FREE(stack_cntr);
          }
     }

     pthread_mutex_unlock( &stack_containers_lock );
}

void
stack_containers_attach_device(CoreInputDevice *device)
{
     Stack_Container    *stack_container;

     D_DEBUG_AT( Core_WindowStack, "Enter:%s()\n", __FUNCTION__);

     pthread_mutex_lock( &stack_containers_lock );

     direct_list_foreach(stack_container, stack_containers) {
          stack_attach_devices(device, stack_container->ctx);
     }

     pthread_mutex_unlock( &stack_containers_lock );
}

void
stack_containers_detach_device(CoreInputDevice *device)
{
     Stack_Container    *stack_container;

     D_DEBUG_AT( Core_WindowStack, "Enter:%s()\n", __FUNCTION__);

     pthread_mutex_lock( &stack_containers_lock );

     direct_list_foreach(stack_container, stack_containers) {
          stack_detach_devices(device, stack_container->ctx);
     }

     pthread_mutex_unlock( &stack_containers_lock );
}

/*
 * Allocates and initializes a window stack.
 */
CoreWindowStack*
dfb_windowstack_create( CoreLayerContext *context )
{
     DFBResult          ret;
     CoreWindowStack   *stack;
     CoreSurfacePolicy  policy = CSP_SYSTEMONLY;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, context );

     D_ASSERT( context != NULL );

     /* Allocate window stack data (completely shared) */
     stack = (CoreWindowStack*) SHCALLOC( context->shmpool, 1, sizeof(CoreWindowStack) );
     if (!stack) {
          D_OOSHM();
          return NULL;
     }

     stack->shmpool = context->shmpool;

     /* Store context which we belong to. */
     stack->context = context;

     /* Set default acceleration */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Choose cursor surface policy. */
     if (context->config.buffermode != DLBM_BACKSYSTEM) {
          CardCapabilities card_caps;

          /* Use the explicitly specified policy? */
          if (dfb_config->window_policy != -1)
               policy = dfb_config->window_policy;
          else {
               /* Examine the hardware capabilities. */
               dfb_gfxcard_get_capabilities( &card_caps );

               if (card_caps.accel & DFXL_BLIT && card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL)
                    policy = CSP_VIDEOHIGH;
          }
     }

     stack->cursor.policy = policy;

     /* Set default background mode. */
     stack->bg.mode        = DLBM_DONTCARE;
     stack->bg.color_index = -1;

     D_MAGIC_SET( stack, CoreWindowStack );

     /* Initialize window manager */
     ret = dfb_wm_init_stack( stack );
     if (ret) {
          D_MAGIC_CLEAR( stack );
          SHFREE( context->shmpool, stack );
          return NULL;
     }

     /* Attach to all input devices */
     dfb_input_enumerate_devices( stack_attach_devices, stack, DICAPS_ALL );

     stack_containers_add(stack);

     D_DEBUG_AT( Core_WindowStack, "  -> %p\n", stack );

     return stack;
}

void
dfb_windowstack_detach_devices( CoreWindowStack *stack )
{
     DirectLink *l;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     stack_containers_remove(stack);

     /* Detach all input devices. */
     l = stack->devices;
     while (l) {
          DirectLink  *next   = l->next;
          StackDevice *device = (StackDevice*) l;

          dfb_input_detach_global( dfb_input_device_at( device->id ),
                                   &device->reaction );

          SHFREE( stack->shmpool, device );

          l = next;
     }
}

void
dfb_windowstack_destroy( CoreWindowStack *stack )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Unlink cursor surface. */
     if (stack->cursor.surface)
          dfb_surface_unlink( &stack->cursor.surface );

     /* Shutdown window manager? */
     if (stack->flags & CWSF_INITIALIZED)
          dfb_wm_close_stack( stack );

     /* detach listener from background surface and unlink it */
     if (stack->bg.image) {
          dfb_surface_detach_global( stack->bg.image,
                                     &stack->bg.image_reaction );

          dfb_surface_unlink( &stack->bg.image );
     }

     /* Deallocate shared stack data. */
     if (stack->stack_data) {
          SHFREE( stack->shmpool, stack->stack_data );
          stack->stack_data = NULL;
     }

     D_MAGIC_CLEAR( stack );

     /* Free stack data. */
     SHFREE( stack->shmpool, stack );
}

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height,
                        int              rotation )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %dx%d, %d )\n", __FUNCTION__, stack, width, height, rotation );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Store the width and height of the stack */
     stack->width    = width;
     stack->height   = height;
     stack->rotation = rotation;

     switch (stack->rotation) {
          default:
               D_BUG( "invalid rotation %d", stack->rotation );
          case 0:
               stack->rotated_blit   = DSBLIT_NOFX;
               stack->rotated_width  = stack->width;
               stack->rotated_height = stack->height;
               break;

          case 90:
               stack->rotated_blit   = DSBLIT_ROTATE90;
               stack->rotated_width  = stack->height;
               stack->rotated_height = stack->width;
               break;

          case 180:
               stack->rotated_blit   = DSBLIT_ROTATE180;
               stack->rotated_width  = stack->width;
               stack->rotated_height = stack->height;
               break;

          case 270:
               stack->rotated_blit   = DSBLIT_ROTATE270;
               stack->rotated_width  = stack->height;
               stack->rotated_height = stack->width;
               break;
     }

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
     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_lock( stack->context );
}

/*
 * Allow access to the window stack data.
 */
DirectResult
dfb_windowstack_unlock( CoreWindowStack *stack )
{
     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->context != NULL );

     return dfb_layer_context_unlock( stack->context );
}

DFBResult
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBResult ret;
     DFBRegion region;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     region.x1 = 0;
     region.y1 = 0;
     region.x2 = stack->rotated_width  - 1;
     region.y2 = stack->rotated_height - 1;

     ret = dfb_wm_update_stack( stack, &region, 0 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

/**********************************************************************************************************************/

/*
 * background handling
 */

DFBResult
dfb_windowstack_set_background_mode ( CoreWindowStack               *stack,
                                      DFBDisplayLayerBackgroundMode  mode )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d )\n", __FUNCTION__, stack, mode );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

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
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, stack, image );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( image != NULL );

     if (!(image->type & CSTF_SHARED))
          return DFB_INVARG;

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
                                      const DFBColor  *color )
{
     D_ASSERT( color != NULL );

     D_DEBUG_AT( Core_WindowStack, "%s( %p, 0x%08x )\n", __FUNCTION__, stack,
                 PIXEL_ARGB( color->a, color->r, color->g, color->b ) );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

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

DFBResult
dfb_windowstack_set_background_color_index( CoreWindowStack *stack,
                                            int              index )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d )\n", __FUNCTION__, stack, index );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* do nothing if color didn't change */
     if (stack->bg.color_index != index) {
          /* set new color index */
          stack->bg.color_index = index;

          /* force an update of the window stack */
          if (stack->bg.mode == DLBM_COLOR)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/**********************************************************************************************************************/

/*
 * cursor control
 */

DFBResult
dfb_windowstack_cursor_enable( CoreDFB *core, CoreWindowStack *stack, bool enable )
{
     DFBResult ret;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %s )\n", __FUNCTION__, stack, enable ? "enable" : "disable" );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.set = true;

     if (dfb_config->no_cursor || stack->cursor.enabled == enable) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     if (enable && !stack->cursor.surface) {
          ret = load_default_cursor( core, stack );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }
     }

     /* Keep state. */
     stack->cursor.enabled = enable;

     /* Notify WM. */
     dfb_wm_update_cursor( stack, enable ? CCUF_ENABLE : CCUF_DISABLE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_opacity( CoreWindowStack *stack, u8 opacity )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, 0x%02x )\n", __FUNCTION__, stack, opacity );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (stack->cursor.opacity != opacity) {
          /* Set new opacity. */
          stack->cursor.opacity = opacity;
          
          /* Notify WM. */
          if (stack->cursor.enabled)
               dfb_wm_update_cursor( stack, CCUF_OPACITY );
     }

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
     CoreSurface           *cursor;
     CoreCursorUpdateFlags  flags = CCUF_SHAPE;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p, hot %d, %d ) <- size %dx%d\n",
                 __FUNCTION__, stack, shape, hot_x, hot_y,
                 shape->config.size.w, shape->config.size.h );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( shape != NULL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     cursor = stack->cursor.surface;
     if (!cursor) {
          D_ASSUME( !stack->cursor.enabled );

          /* Create the a surface for the shape. */
          ret = create_cursor_surface( stack, shape->config.size.w, shape->config.size.h );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }

          cursor = stack->cursor.surface;
     }
     else if (stack->cursor.size.w != shape->config.size.w || stack->cursor.size.h != shape->config.size.h) {
          dfb_surface_reformat( cursor, shape->config.size.w, shape->config.size.h, DSPF_ARGB );

          stack->cursor.size.w = shape->config.size.w;
          stack->cursor.size.h = shape->config.size.h;

          /* Notify about new size. */
          flags |= CCUF_SIZE;
     }

     if (stack->cursor.hot.x != hot_x || stack->cursor.hot.y != hot_y) {
          stack->cursor.hot.x = hot_x;
          stack->cursor.hot.y = hot_y;

          /* Notify about new position. */
          flags |= CCUF_POSITION;
     }

     /* Copy the content of the new shape. */
     dfb_gfx_copy( shape, cursor, NULL );

     cursor->config.caps = ((cursor->config.caps & ~DSCAPS_PREMULTIPLIED) | (shape->config.caps & DSCAPS_PREMULTIPLIED));

     /* Notify the WM. */
     if (stack->cursor.enabled)
          dfb_wm_update_cursor( stack, flags );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_warp( CoreWindowStack *stack, int x, int y )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d, %d )\n", __FUNCTION__, stack, x, y );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (x < 0)
          x = 0;
     else if (x > stack->width - 1)
          x = stack->width - 1;

     if (y < 0)
          y = 0;
     else if (y > stack->height - 1)
          y = stack->height - 1;

     if (stack->cursor.x != x || stack->cursor.y != y) {
          stack->cursor.x = x;
          stack->cursor.y = y;

          /* Notify the WM. */
          if (stack->cursor.enabled)
               dfb_wm_update_cursor( stack, CCUF_POSITION );
     }

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
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %d, %d, %d )\n",
                 __FUNCTION__, stack, numerator, denominator, threshold );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

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
dfb_windowstack_get_cursor_position( CoreWindowStack *stack, int *ret_x, int *ret_y )
{
     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p, %p )\n", __FUNCTION__, stack, ret_x, ret_y );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSUME( ret_x != NULL || ret_y != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (ret_x)
          *ret_x = stack->cursor.x;

     if (ret_y)
          *ret_y = stack->cursor.y;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/**********************************************************************************************************************/

ReactionResult
_dfb_windowstack_inputdevice_listener( const void *msg_data,
                                       void       *ctx )
{
     const DFBInputEvent *event = msg_data;
     CoreWindowStack     *stack = ctx;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( msg_data != NULL );
     D_MAGIC_ASSERT( stack, CoreWindowStack );

     // Dynamically add/decrease the ref to the layer context when using the
     // layer context.  This will prevent the layer context from being
     // destroyed when it is being used.

     int num = 0;

     // Make sure the layer context's reference count is non-zero.  If it is,
     // return early and indicate the listener should be removed.  In this
     // scenario, this prevents the object_reference_watcher from being called
     // more than once triggered by the reference count changing from 1 to 0
     // again.
     if (dfb_layer_context_ref_stat( stack->context, &num ) || num == 0)
          return RS_REMOVE;

     // Increase the layer context's reference count.
     if (dfb_layer_context_ref( stack->context ))
          return RS_REMOVE;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack )) {
          dfb_layer_context_unref( stack->context );
          return RS_REMOVE;
     }

     /* Call the window manager to dispatch the event. */
     if (dfb_layer_context_active( stack->context ))
          dfb_wm_process_input( stack, event );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     // Decrease the layer context's reference count.
     dfb_layer_context_unref( stack->context );

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

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( notification != NULL );
     D_MAGIC_ASSERT( stack, CoreWindowStack );

     if (notification->flags & CSNF_DESTROY) {
          if (stack->bg.image == notification->surface) {
               D_ERROR( "Core/WindowStack: Surface for background vanished.\n" );

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

/**********************************************************************************************************************/

/*
 * internals
 */

static DFBEnumerationResult
stack_attach_devices( CoreInputDevice *device,
                      void            *ctx )
{
     StackDevice     *dev;
     CoreWindowStack *stack = (CoreWindowStack*) ctx;

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     dev = SHCALLOC( stack->shmpool, 1, sizeof(StackDevice) );
     if (!dev) {
          D_ERROR( "Core/WindowStack: Could not allocate %zu bytes\n", sizeof(StackDevice) );
          return DFENUM_CANCEL;
     }

     dev->id = dfb_input_device_id( device );

     direct_list_prepend( &stack->devices, &dev->link );

     dfb_input_attach_global( device, DFB_WINDOWSTACK_INPUTDEVICE_LISTENER,
                              ctx, &dev->reaction );

     return DFENUM_OK;
}

static DFBEnumerationResult
stack_detach_devices( CoreInputDevice *device,
                        void            *ctx )
{
     DirectLink *link;
     CoreWindowStack *stack = (CoreWindowStack*) ctx;

     D_ASSERT( stack != NULL );
     D_ASSERT( device != NULL );

     link = stack->devices;
     while (link) {
          DirectLink  *next   = link->next;
          StackDevice *dev = (StackDevice*) link;

          if (dfb_input_device_id(device) == dev->id) {
               direct_list_remove( &stack->devices, &dev->link );

               dfb_input_detach_global(device, &dev->reaction );
               SHFREE( stack->shmpool, dev );
               return DFENUM_OK;
          }
          link = next;
     }
     return DFENUM_CANCEL;
}

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult
load_default_cursor( CoreDFB *core, CoreWindowStack *stack )
{
     DFBResult              ret;
     int                    i;
     FILE                  *f;
     CoreSurfaceBufferLock  lock;
     void                  *data;

     D_DEBUG_AT( Core_WindowStack, "%s( %p )\n", __FUNCTION__, stack );

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     if (!stack->cursor.surface) {
          ret = create_cursor_surface( stack, 40, 40 );
          if (ret)
               return ret;
     }
     else {
          stack->cursor.hot.x  = 0;
          stack->cursor.hot.y  = 0;
          stack->cursor.size.w = 40;
          stack->cursor.size.h = 40;
     }

     /* lock the cursor surface */
     ret = dfb_surface_lock_buffer( stack->cursor.surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret) {
          D_ERROR( "Core/WindowStack: cannot lock the cursor surface!\n" );
          return ret;
     }

     data = lock.addr;

     /* initialize as empty cursor */
     memset( data, 0, 40 * lock.pitch );

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2result( errno );

          /* ignore a missing cursor file */
          if (ret == DFB_FILENOTFOUND)
               ret = DFB_OK;
          else
               D_PERROR( "Core/WindowStack: `" CURSORFILE "` could not be opened!\n" );

          goto finish;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, MIN (40*4, lock.pitch), 1, f ) != 1) {
               ret = errno2result( errno );

               D_ERROR( "Core/WindowStack: unexpected end or read error of cursor data!\n" );

               goto finish;
          }
#ifdef WORDS_BIGENDIAN
          {
               int i = MIN (40, lock.pitch/4);
               u32 *tmp_data = data;

               while (i--) {
                    *tmp_data = (*tmp_data & 0xFF000000) >> 24 |
                                (*tmp_data & 0x00FF0000) >>  8 |
                                (*tmp_data & 0x0000FF00) <<  8 |
                                (*tmp_data & 0x000000FF) << 24;
                    ++tmp_data;
               }
          }
#endif
          data += lock.pitch;
     }

finish:
     if (f)
          fclose( f );

     dfb_surface_unlock_buffer( stack->cursor.surface, &lock );

     return ret;
}

static DFBResult
create_cursor_surface( CoreWindowStack *stack,
                       int              width,
                       int              height )
{
     DFBResult               ret;
     CoreSurface            *surface;
     CoreLayer              *layer;
     CoreLayerContext       *context;
     DFBSurfaceCapabilities  surface_caps = DSCAPS_NONE;

     D_DEBUG_AT( Core_WindowStack, "%s( %p, %dx%d )\n", __FUNCTION__, stack, width, height );

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->cursor.surface == NULL );

     context = stack->context;

     D_ASSERT( context != NULL );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );

     stack->cursor.x       = stack->width  / 2;
     stack->cursor.y       = stack->height / 2;
     stack->cursor.hot.x   = 0;
     stack->cursor.hot.y   = 0;
     stack->cursor.size.w  = width;
     stack->cursor.size.h  = height;
     stack->cursor.opacity = 0xFF;

     if (context->config.buffermode == DLBM_WINDOWS)
          D_WARN( "cursor not yet visible with DLBM_WINDOWS" );

     dfb_surface_caps_apply_policy( stack->cursor.policy, &surface_caps );

     /* Create the cursor surface. */
     ret = dfb_surface_create_simple( layer->core, width, height, DSPF_ARGB, DSCS_RGB, 
                                      surface_caps, CSTF_SHARED | CSTF_CURSOR,
                                      0, /* FIXME: no shared cursor objects, no cursor id */
                                      NULL, &surface );
     if (ret) {
          D_ERROR( "Core/WindowStack: Failed creating a surface for software cursor!\n" );
          return ret;
     }

     dfb_surface_globalize( surface );

     stack->cursor.surface = surface;

     return DFB_OK;
}


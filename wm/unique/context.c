/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/object.h>
#include <fusion/shmalloc.h>

#include <core/coretypes.h>
#include <core/input.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/layers_internal.h>      /* FIXME */
#include <core/state.h>
#include <core/surfaces.h>
#include <core/windowstack.h>
#include <core/windows_internal.h>     /* FIXME */

#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <unique/context.h>
#include <unique/device.h>
#include <unique/input_channel.h>
#include <unique/input_switch.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Context, "UniQuE/Context", "UniQuE's Stack Context" );


static const ReactionFunc unique_context_globals[] = {
     _unique_wm_module_context_listener,
     NULL
};

/**************************************************************************************************/

static void
context_destructor( FusionObject *object, bool zombie )
{
     int            i;
     UniqueContext *context = (UniqueContext*) object;

     D_DEBUG_AT( UniQuE_Context, "destroying %p (stack %p)%s\n",
                 context, context->stack, zombie ? " (ZOMBIE)" : "");

     D_ASSUME( fusion_vector_is_empty( &context->windows ) );

     unique_context_notify( context, UCNF_DESTROYED );

     unique_device_detach_global( context->devices[UDCI_POINTER], &context->cursor_reaction );


     unique_input_switch_drop( context->input_switch, context->foo_channel );

     unique_input_channel_destroy( context->foo_channel );

     unique_input_switch_destroy( context->input_switch );


     for (i=0; i<_UDCI_NUM; i++)
          unique_device_destroy( context->devices[i] );


     while (fusion_vector_has_elements( &context->windows )) {
          unique_window_destroy( fusion_vector_at( &context->windows, 0 ) );
     }

     stret_region_destroy( context->root );


     fusion_vector_destroy( &context->windows );

     dfb_surface_unlink( &context->surface );

     dfb_layer_region_unlink( &context->region );

     D_MAGIC_CLEAR( context );

     fusion_object_destroy( object );
}

FusionObjectPool *
unique_context_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "UniQuE Context Pool", sizeof(UniqueContext),
                                       sizeof(UniqueContextNotification), context_destructor, world );
}

/**************************************************************************************************/

static DFBEnumerationResult
connect_device( CoreInputDevice *source,
                void            *ctx )
{
     UniqueDevice *device = ctx;

     D_MAGIC_ASSERT( device, UniqueDevice );

     unique_device_connect( device, source );

     return DFENUM_OK;
}

static DFBResult
create_devices( CoreDFB       *core,
                UniqueContext *context,
                WMShared      *shared )
{
     int       i;
     DFBResult ret;

     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( shared, WMShared );

     for (i=0; i<_UDCI_NUM; i++) {
          DFBInputDeviceCapabilities caps;

          ret = unique_device_create( core, context, shared->device_classes[i],
                                      context, &context->devices[i] );
          if (ret)
               goto error;

          ret = unique_input_switch_add( context->input_switch, context->devices[i] );
          if (ret)
               goto error_add;

          switch (i) {
               case UDCI_POINTER:
                    caps = DICAPS_AXES | DICAPS_BUTTONS;
                    break;

               case UDCI_WHEEL:
                    caps = DICAPS_AXES;
                    break;

               case UDCI_KEYBOARD:
                    caps = DICAPS_KEYS;
                    break;

               default:
                    caps = DICAPS_ALL;
                    break;
          }

          dfb_input_enumerate_devices( connect_device, context->devices[i], caps );
     }

     return DFB_OK;


error_add:
     unique_device_destroy( context->devices[i] );

error:
     while (--i >= 0)
          unique_device_destroy( context->devices[i] );

     return ret;
}

/**************************************************************************************************/

DFBResult
unique_context_create( CoreDFB            *core,
                       CoreWindowStack    *stack,
                       CoreLayerRegion    *region,
                       DFBDisplayLayerID   layer_id,
                       WMShared           *shared,
                       UniqueContext     **ret_context )
{
     int            i;
     DFBResult      ret;
     UniqueContext *context;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( shared, WMShared );
     D_ASSERT( ret_context != NULL );

     context = unique_wm_create_context();
     if (!context)
          return DFB_FUSION;

     context->stack    = stack;
     context->shared   = shared;
     context->layer_id = layer_id;
     context->color    = (DFBColor) { 0xff, 0xa0, 0xd0, 0xf0 };
     context->shmpool  = stack->shmpool;

     fusion_vector_init( &context->windows, 16, context->shmpool );

     /* Create Root Region. */
     ret = stret_region_create( shared->region_classes[URCI_ROOT], context, 0,
                                SRF_ACTIVE | SRF_OUTPUT, _UNRL_NUM,
                                0, 0, INT_MAX, INT_MAX,
                                NULL, 0, context->shmpool, &context->root );
     if (ret)
          goto error;

     /* Link layer region. */
     ret = dfb_layer_region_link( &context->region, region );
     if (ret)
          goto error;

     /* Get the region's surface. */
     ret = dfb_layer_region_get_surface( region, &context->surface );
     if (ret)
          goto error;

     /* Make it global. */
     ret = dfb_surface_globalize( context->surface );
     if (ret) {
          dfb_surface_unref( context->surface );
          goto error;
     }

     D_MAGIC_SET( context, UniqueContext );

     ret = unique_input_switch_create( context, &context->input_switch );
     if (ret)
          goto error_switch;

     ret = create_devices( core, context, shared );
     if (ret)
          goto error_devices;

     ret = unique_input_channel_create( core, context, &context->foo_channel );
     if (ret)
          goto error_foo_channel;

     ret = unique_device_attach_global( context->devices[UDCI_POINTER],
                                        UNIQUE_CURSOR_DEVICE_LISTENER,
                                        context, &context->cursor_reaction );
     if (ret)
          goto error_attach_cursor;

     /* Change global reaction lock. */
     fusion_object_set_lock( &context->object, &context->stack->context->lock );

     /* Activate Object. */
     fusion_object_activate( &context->object );

     /* Return new context. */
     *ret_context = context;

     return DFB_OK;


error_attach_cursor:
     unique_input_channel_destroy( context->foo_channel );

error_foo_channel:
     for (i=0; i<_UDCI_NUM; i++)
          unique_device_destroy( context->devices[i] );

error_devices:
     unique_input_switch_destroy( context->input_switch );

error_switch:
     D_MAGIC_CLEAR( context );

     dfb_surface_unlink( &context->surface );

error:
     if (context->region)
          dfb_layer_region_unlink( &context->region );

     if (context->root)
          stret_region_destroy( context->root );

     fusion_vector_destroy( &context->windows );

     fusion_object_destroy( &context->object );

     return ret;
}

DFBResult
unique_context_notify( UniqueContext                  *context,
                       UniqueContextNotificationFlags  flags )
{
     UniqueContextNotification notification;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( flags != UCNF_NONE );

     D_ASSERT( ! (flags & ~UCNF_ALL) );

     notification.flags   = flags;
     notification.context = context;

     return unique_context_dispatch( context, &notification, unique_context_globals );
}

DFBResult
unique_context_set_active( UniqueContext  *context,
                           bool            active )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSUME( context->active != active );

     if (context->active == active)
          return DFB_OK;

     context->active = active;

     if (active)
          return dfb_windowstack_repaint_all( context->stack );

     /* Force release of all pressed keys. */
     return unique_context_flush_keys( context );
}

DFBResult
unique_context_set_color( UniqueContext  *context,
                          const DFBColor *color )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( color != NULL );

     context->color = *color;

     return dfb_windowstack_repaint_all( context->stack );
}

/* HACK: dumped in here for now, will move into cursor class */
void
unique_draw_cursor( CoreWindowStack *stack, UniqueContext *context, CardState *state, DFBRegion *region )
{
     DFBRectangle            src;
     DFBSurfaceBlittingFlags flags = DSBLIT_BLEND_ALPHACHANNEL;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     D_ASSUME( stack->cursor.opacity > 0 );

     /* Initialize source rectangle. */
     src.x = region->x1 - stack->cursor.x + stack->cursor.hot.x;
     src.y = region->y1 - stack->cursor.y + stack->cursor.hot.y;
     src.w = region->x2 - region->x1 + 1;
     src.h = region->y2 - region->y1 + 1;

     /* Use global alpha blending. */
     if (stack->cursor.opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          /* Set opacity as blending factor. */
          if (state->color.a != stack->cursor.opacity) {
               state->color.a   = stack->cursor.opacity;
               state->modified |= SMF_COLOR;
          }
     }

     /* Different compositing methods depending on destination format. */
     if (flags & DSBLIT_BLEND_ALPHACHANNEL) {
          if (DFB_PIXELFORMAT_HAS_ALPHA( state->destination->format )) {
               /*
                * Always use compliant Porter/Duff SRC_OVER,
                * if the destination has an alpha channel.
                *
                * Cd = destination color  (non-premultiplied)
                * Ad = destination alpha
                *
                * Cs = source color       (non-premultiplied)
                * As = source alpha
                *
                * Ac = color alpha
                *
                * cd = Cd * Ad            (premultiply destination)
                * cs = Cs * As            (premultiply source)
                *
                * The full equation to calculate resulting color and alpha (premultiplied):
                *
                * cx = cd * (1-As*Ac) + cs * Ac
                * ax = Ad * (1-As*Ac) + As * Ac
                */
               dfb_state_set_src_blend( state, DSBF_ONE );

               /* Need to premultiply source with As*Ac or only with Ac? */
               if (! (stack->cursor.surface->caps & DSCAPS_PREMULTIPLIED))
                    flags |= DSBLIT_SRC_PREMULTIPLY;
               else if (flags & DSBLIT_BLEND_COLORALPHA)
                    flags |= DSBLIT_SRC_PREMULTCOLOR;

               /* Need to premultiply/demultiply destination? */
//               if (! (state->destination->caps & DSCAPS_PREMULTIPLIED))
//                    flags |= DSBLIT_DST_PREMULTIPLY | DSBLIT_DEMULTIPLY;
          }
          else {
               /*
                * We can avoid DSBLIT_SRC_PREMULTIPLY for destinations without an alpha channel
                * by using another blending function, which is more likely that it's accelerated
                * than premultiplication at this point in time.
                *
                * This way the resulting alpha (ax) doesn't comply with SRC_OVER,
                * but as the destination doesn't have an alpha channel it's no problem.
                *
                * As the destination's alpha value is always 1.0 there's no need for
                * premultiplication. The resulting alpha value will also be 1.0 without
                * exceptions, therefore no need for demultiplication.
                *
                * cx = Cd * (1-As*Ac) + Cs*As * Ac  (still same effect as above)
                * ax = Ad * (1-As*Ac) + As*As * Ac  (wrong, but discarded anyways)
                */
               if (stack->cursor.surface->caps & DSCAPS_PREMULTIPLIED) {
                    /* Need to premultiply source with Ac? */
                    if (flags & DSBLIT_BLEND_COLORALPHA)
                         flags |= DSBLIT_SRC_PREMULTCOLOR;

                    dfb_state_set_src_blend( state, DSBF_ONE );
               }
               else
                    dfb_state_set_src_blend( state, DSBF_SRCALPHA );
          }
     }

     /* Set blitting flags. */
     dfb_state_set_blitting_flags( state, flags );

     /* Set blitting source. */
     state->source    = stack->cursor.surface;
     state->modified |= SMF_SOURCE;

     /* Blit from the window to the region being updated. */
     dfb_gfxcard_blit( &src, region->x1, region->y1, state );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

DFBResult
unique_context_update( UniqueContext       *context,
                       const DFBRegion     *updates,
                       int                  num,
                       DFBSurfaceFlipFlags  flags )
{
     int        i;
     CoreLayer *layer;
     CardState *state;
     int        count = 0;
     DFBRegion  root;
     DFBRegion  regions[num];
     DFBRegion  cursor_inter;

     D_MAGIC_ASSERT( context, UniqueContext );
     D_ASSERT( context->stack != NULL );

     D_ASSERT( updates != NULL );
     D_ASSERT( num > 0 );

     if (!context->active)
          return DFB_OK;

     D_DEBUG_AT( UniQuE_Context, "unique_context_update( num %d, flags 0x%08x )\n", num, flags );

     layer = dfb_layer_at( context->layer_id );
     state = dfb_layer_state( layer );

     root = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0, context->width, context->height );

     for (i=0; i<num; i++) {
          const DFBRegion *region = &updates[i];

          if (!dfb_region_region_intersects( region, &root ))
               continue;

          regions[count++] = DFB_REGION_INIT_INTERSECTED( region, 0, 0, root.x2, root.y2 );

          D_DEBUG_AT( UniQuE_Context, "    (%2d) %4d, %4d - %4dx%4d\n", i,
                      DFB_RECTANGLE_VALS_FROM_REGION( &regions[count-1] ) );
     }

     if (!count) {
          D_DEBUG_AT( UniQuE_Context, "    -> No intersection with root!\n" );
          return DFB_OK;
     }

     /* Set destination. */
     state->destination  = context->surface;
     state->modified    |= SMF_DESTINATION;

     for (i=0; i<count; i++) {
          const DFBRegion *update = &regions[i];

          /* Set clipping region. */
          dfb_state_set_clip( state, update );

          /* Compose updated region. */
          stret_region_update( context->root, update, state );

          /* Update cursor? */
          cursor_inter = context->cursor_region;
          if (context->cursor_drawn && dfb_region_region_intersect( &cursor_inter, update )) {
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &cursor_inter );

               D_ASSUME( context->cursor_bs_valid );

               dfb_gfx_copy_to( context->surface, context->cursor_bs, &rect,
                                rect.x - context->cursor_region.x1,
                                rect.y - context->cursor_region.y1, true );

               unique_draw_cursor( context->stack, context, state, &cursor_inter );
          }
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;

     /* Software cursor code relies on a valid back buffer. */
     if (context->stack->cursor.enabled)
          flags |= DSFLIP_BLIT;

     /* Flip all updated regions. */
     for (i=0; i<count; i++) {
          const DFBRegion *update = &regions[i];

          dfb_layer_region_flip_update( context->region, update, flags );
     }

     return DFB_OK;
}

DFBResult
unique_context_resize( UniqueContext *context,
                       int            width,
                       int            height )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     context->width  = width;
     context->height = height;

     stret_region_resize( context->root, width, height );

     return DFB_OK;
}

DFBResult
unique_context_flush_keys( UniqueContext *context )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     return DFB_OK;
}

DFBResult
unique_context_window_at( UniqueContext  *context,
                          int             x,
                          int             y,
                          UniqueWindow  **ret_window )
{
     int              i;
     CoreWindowStack *stack;
     WMShared        *shared;
     UniqueWindow    *window = NULL;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( ret_window != NULL );

     stack = context->stack;
     shared = context->shared;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( shared, WMShared );

     if (stack->cursor.enabled) {
          StretRegion *region;

          if (x < 0)
               x = stack->cursor.x;
          if (y < 0)
               y = stack->cursor.y;

          region = stret_region_at( context->root, x, y, SRF_INPUT, SRCID_UNKNOWN );
          if (region && (region->clazz == shared->region_classes[URCI_FOO] ||
                         region->clazz == shared->region_classes[URCI_WINDOW]))
          {
               window = stret_region_data( region );

               D_MAGIC_ASSERT( window, UniqueWindow );
          }
     }
     else {
          fusion_vector_foreach_reverse (window, i, context->windows)
               if (window->opacity && !(window->options & DWOP_GHOST))
                    break;

          if (i < 0)
               window = NULL;
     }

     D_MAGIC_ASSERT_IF( window, UniqueWindow );

     *ret_window = window;

     return DFB_OK;
}

DFBResult
unique_context_lookup_window( UniqueContext  *context,
                              DFBWindowID     window_id,
                              UniqueWindow  **ret_window )
{
     int           i;
     UniqueWindow *window = NULL;

     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     fusion_vector_foreach_reverse (window, i, context->windows) {
          if (window->window->id == window_id)
               break;
     }

     *ret_window = window;

     return DFB_OK;
}

DFBResult
unique_context_enum_windows( UniqueContext        *context,
                             CoreWMWindowCallback  callback,
                             void                 *callback_ctx )
{
     int           i;
     UniqueWindow *window = NULL;

     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     fusion_vector_foreach_reverse (window, i, context->windows) {
          if (callback( window->window, callback_ctx ) != DFENUM_OK)
               break;
     }

     return DFB_OK;
}

/**************************************************************************************************/

ReactionResult
_unique_cursor_device_listener( const void *msg_data,
                                void       *ctx )
{
     const UniqueInputEvent *event   = msg_data;
     UniqueContext          *context = ctx;

     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_DEBUG_AT( UniQuE_Context, "_unique_cursor_device_listener( %p, %p )\n", event, context );

     switch (event->type) {
          case UIET_MOTION:
               dfb_windowstack_cursor_warp( context->stack, event->pointer.x, event->pointer.y );
               break;

          default:
               break;
     }

     return RS_OK;
}


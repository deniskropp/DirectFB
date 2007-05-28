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

#include <fusion/object.h>
#include <fusion/shmalloc.h>

#include <core/coretypes.h>
#include <core/layers_internal.h>      /* FIXME */
#include <core/surfaces.h>
#include <core/windows.h>
#include <core/windows_internal.h>     /* FIXME */
#include <core/windowstack.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <unique/context.h>
#include <unique/input_channel.h>
#include <unique/input_switch.h>
#include <unique/internal.h>
#include <unique/window.h>


D_DEBUG_DOMAIN( UniQuE_Window, "UniQuE/Window", "UniQuE's Window" );


static const ReactionFunc unique_window_globals[] = {
     _unique_wm_module_window_listener,
     NULL
};

/**************************************************************************************************/

typedef struct {
     DirectLink                  link;

     int                         magic;

     DFBInputDeviceKeySymbol     symbol;
     DFBInputDeviceModifierMask  modifiers;

     UniqueInputFilter          *filter;
} KeyFilter;

/**************************************************************************************************/

static DFBResult  add_key_filter    ( UniqueWindow               *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers,
                                      UniqueInputFilter          *filter );

static DFBResult  remove_key_filter ( UniqueWindow               *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers );

static void       remove_all_filters( UniqueWindow               *window );

/**************************************************************************************************/

static DFBResult  create_regions  ( UniqueWindow  *window );

static void       insert_window   ( UniqueContext *context,
                                    UniqueWindow  *window );

static void       remove_window   ( UniqueWindow  *window );

static void       update_foo_frame( UniqueWindow  *window,
                                    UniqueContext *context,
                                    WMShared      *shared );

static DFBResult  update_window   ( UniqueWindow        *window,
                                    const DFBRegion     *region,
                                    DFBSurfaceFlipFlags  flags,
                                    bool                 complete );

static void       update_flags    ( UniqueWindow  *window );

static void       set_opacity     ( UniqueWindow  *window,
                                    u8             opacity );

static DFBResult  move_window     ( UniqueWindow  *window,
                                    int            dx,
                                    int            dy );

static DFBResult  resize_window   ( UniqueWindow  *window,
                                    int            width,
                                    int            height );

static DFBResult  restack_window  ( UniqueWindow           *window,
                                    UniqueWindow           *relative,
                                    int                     relation,
                                    DFBWindowStackingClass  stacking );

/**************************************************************************************************/

static inline int get_priority( DFBWindowCapabilities caps, DFBWindowStackingClass stacking );

/**************************************************************************************************/

static void
window_destructor( FusionObject *object, bool zombie, void *ctx )
{
     int            i;
     UniqueContext *context;
     UniqueWindow  *window = (UniqueWindow*) object;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_DEBUG_AT( UniQuE_Window, "destroying %p (%dx%d - %dx%d)%s\n",
                 window, DFB_RECTANGLE_VALS( &window->bounds ), zombie ? " (ZOMBIE)" : "");

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );


     D_FLAGS_SET( window->flags, UWF_DESTROYED );

     unique_window_notify( window, UWNF_DESTROYED );


     set_opacity( window, 0 );

     remove_window( window );

     for (i=0; i<8; i++) {
          if (window->foos[i])
               stret_region_destroy( window->foos[i] );
     }

     stret_region_destroy( window->region );

     stret_region_destroy( window->frame );


     remove_all_filters( window );

     unique_input_switch_drop( context->input_switch, window->channel );

     unique_input_channel_detach_global( window->channel, &window->channel_reaction );

     unique_input_channel_destroy( window->channel );


     if (window->surface)
          dfb_surface_unlink( &window->surface );

     unique_context_unlink( &window->context );

     dfb_window_unlink( &window->window );

     D_MAGIC_CLEAR( window );

     fusion_object_destroy( object );
}

FusionObjectPool *
unique_window_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "UniQuE Window Pool", sizeof(UniqueWindow),
                                       sizeof(UniqueWindowNotification), window_destructor, NULL, world );
}

/**************************************************************************************************/

DFBResult
unique_window_create( CoreDFB                 *core,
                      CoreWindow              *window,
                      UniqueContext           *context,
                      DFBWindowCapabilities    caps,
                      const CoreWindowConfig  *config,
                      UniqueWindow           **ret_window )
{
     DFBResult     ret;
     UniqueWindow *uniwin;
     WMShared     *shared;
     CoreSurface  *surface;
     DFBRectangle  bounds;

     D_ASSERT( window != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( context, UniqueContext );

     shared = context->shared;

     D_MAGIC_ASSERT( shared, WMShared );

     surface = dfb_window_surface( window );

     D_ASSERT( surface != NULL || (caps & DWCAPS_INPUTONLY));

     uniwin = unique_wm_create_window();
     if (!uniwin)
          return DFB_FUSION;

     bounds = config->bounds;

     if (bounds.x == 0 && bounds.y == 0) {
          bounds.x = (context->width  - bounds.w) / 2;
          bounds.y = (context->height - bounds.h) / 2;
     }

     /* Initialize window data. */
     uniwin->shared    = context->shared;

     uniwin->caps      = caps;
     uniwin->flags     = UWF_NONE;

     uniwin->bounds    = bounds;
     uniwin->opacity   = config->opacity;
     uniwin->stacking  = config->stacking;
     uniwin->priority  = get_priority( caps, config->stacking );
     uniwin->options   = config->options;
     uniwin->events    = config->events;
     uniwin->color_key = config->color_key;
     uniwin->opaque    = config->opaque;

     if (dfb_config->decorations && ! (caps & (DWCAPS_INPUTONLY | DWCAPS_NODECORATION))) {
          uniwin->flags |= UWF_DECORATED;
          uniwin->insets = shared->insets;
     }

     dfb_rectangle_from_rectangle_plus_insets( &uniwin->full, &uniwin->bounds, &uniwin->insets );

     ret = dfb_window_link( &uniwin->window, window );
     if (ret)
          goto error;

     ret = unique_context_link( &uniwin->context, context );
     if (ret)
          goto error;

     if (surface) {
          ret = dfb_surface_link( &uniwin->surface, surface );
          if (ret)
               goto error;
     }


     ret = unique_input_channel_create( core, context, &uniwin->channel );
     if (ret)
          goto error;

     ret = unique_input_channel_attach_global( uniwin->channel,
                                               UNIQUE_WINDOW_INPUT_CHANNEL_LISTENER,
                                               uniwin, &uniwin->channel_reaction );
     if (ret)
          goto error;


     D_MAGIC_SET( uniwin, UniqueWindow );

     ret = create_regions( uniwin );
     if (ret) {
          D_MAGIC_CLEAR( uniwin );
          goto error;
     }


     /* Change global reaction lock. */
     fusion_object_set_lock( &uniwin->object, &context->stack->context->lock );

     /* activate object */
     fusion_object_activate( &uniwin->object );

     /* Actually add the window to the stack. */
     insert_window( context, uniwin );

     /* Possibly switch focus to the new window. */
     //unique_context_update_focus( context );

     /* return the new context */
     *ret_window = uniwin;

     return DFB_OK;

error:
     if (uniwin->channel_reaction.attached)
          unique_input_channel_detach_global( uniwin->channel, &uniwin->channel_reaction );

     if (uniwin->channel)
          unique_input_channel_destroy( uniwin->channel );

     if (uniwin->surface)
          dfb_surface_unlink( &uniwin->surface );

     if (uniwin->context)
          unique_context_unlink( &uniwin->context );

     if (uniwin->window)
          dfb_window_unlink( &uniwin->window );

     fusion_object_destroy( &uniwin->object );

     return ret;
}

DFBResult
unique_window_close( UniqueWindow *window )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

DFBResult
unique_window_destroy( UniqueWindow *window )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

DFBResult
unique_window_notify( UniqueWindow                  *window,
                      UniqueWindowNotificationFlags  flags )
{
     UniqueWindowNotification notification;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( flags != UWNF_NONE );

     D_ASSERT( ! (flags & ~UWNF_ALL) );

     notification.flags  = flags;
     notification.window = window;

     return unique_window_dispatch( window, &notification, unique_window_globals );
}

DFBResult
unique_window_update( UniqueWindow        *window,
                      const DFBRegion     *region,
                      DFBSurfaceFlipFlags  flags )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     DFB_REGION_ASSERT_IF( region );

     return update_window( window, region, flags, false );
}

DFBResult
unique_window_post_event( UniqueWindow   *window,
                          DFBWindowEvent *event )
{
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     if (!D_FLAGS_IS_SET( window->events, event->type ))
          return DFB_OK;

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     dfb_window_post_event( window->window, event );

     return DFB_OK;
}

DFBResult
unique_window_set_config( UniqueWindow           *window,
                          const CoreWindowConfig *config,
                          CoreWindowConfigFlags   flags )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     if (flags & CWCF_OPTIONS) {
          window->options = config->options;
          update_flags( window );
     }

     if (flags & CWCF_EVENTS)
          window->events = config->events;

     if (flags & CWCF_COLOR_KEY)
          window->color_key = config->color_key;

     if (flags & CWCF_OPAQUE)
          window->opaque = config->opaque;

     if (flags & CWCF_OPACITY && !config->opacity)
          set_opacity( window, config->opacity );

     if (flags & CWCF_POSITION)
          move_window( window,
                       config->bounds.x - window->bounds.x,
                       config->bounds.y - window->bounds.y );

     if (flags & CWCF_STACKING)
          restack_window( window, window, 0, config->stacking );

     if (flags & CWCF_OPACITY && config->opacity)
          set_opacity( window, config->opacity );

     if (flags & CWCF_SIZE)
          return resize_window( window, config->bounds.w, config->bounds.h );

     return DFB_OK;
}

DFBResult
unique_window_get_config( UniqueWindow     *window,
                          CoreWindowConfig *config )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( config != NULL );

     config->bounds    = window->bounds;
     config->opacity   = window->opacity;
     config->stacking  = window->stacking;
     config->options   = window->options;
     config->events    = window->events;
     config->color_key = window->color_key;
     config->opaque    = window->opaque;

     return DFB_OK;
}


DFBResult
unique_window_restack( UniqueWindow *window,
                       UniqueWindow *relative,
                       int           relation )
{
     D_MAGIC_ASSERT( window, UniqueWindow );

     return restack_window( window, relative, relation, window->stacking );
}


DFBResult
unique_window_grab( UniqueWindow     *window,
                    const CoreWMGrab *grab )
{
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               return unique_input_switch_set( context->input_switch,
                                               UDCI_KEYBOARD, window->channel );

          case CWMGT_POINTER:
               return unique_input_switch_set( context->input_switch,
                                               UDCI_POINTER, window->channel );

          case CWMGT_KEY: {
               DFBResult          ret;
               UniqueInputEvent   event;
               UniqueInputFilter *filter;

               event.keyboard.key_code   = -1;
               event.keyboard.key_symbol = grab->symbol;
               event.keyboard.modifiers  = grab->modifiers;

               ret = unique_input_switch_set_filter( context->input_switch, UDCI_KEYBOARD,
                                                     window->channel, &event, &filter );
               if (ret) {
                    D_DERROR( ret, "UniQuE/Window: Could not set input filter for key grab!\n" );
                    return ret;
               }

               ret = add_key_filter( window, grab->symbol, grab->modifiers, filter );
               if (ret) {
                    unique_input_switch_unset_filter( context->input_switch, filter );
                    return ret;
               }

               break;
          }

          default:
               D_BUG( "unknown grab target" );
               break;
     }

     return DFB_OK;
}

DFBResult
unique_window_ungrab( UniqueWindow     *window,
                      const CoreWMGrab *grab )
{
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               return unique_input_switch_unset( context->input_switch,
                                                 UDCI_KEYBOARD, window->channel );

          case CWMGT_POINTER:
               return unique_input_switch_unset( context->input_switch,
                                                 UDCI_POINTER, window->channel );

          case CWMGT_KEY:
               return remove_key_filter( window, grab->symbol, grab->modifiers );

          default:
               D_BUG( "unknown grab target" );
               break;
     }

     return DFB_OK;
}

DFBResult
unique_window_request_focus( UniqueWindow *window )
{
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     return unique_input_switch_select( context->input_switch, UDCI_KEYBOARD, window->channel );
}

/**************************************************************************************************/

static DFBResult
add_key_filter( UniqueWindow               *window,
                DFBInputDeviceKeySymbol     symbol,
                DFBInputDeviceModifierMask  modifiers,
                UniqueInputFilter          *filter )
{
     KeyFilter     *key;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     key = SHCALLOC( context->shmpool, 1, sizeof(KeyFilter) );
     if (!key)
          return D_OOSHM();

     key->symbol    = symbol;
     key->modifiers = modifiers;
     key->filter    = filter;

     direct_list_append( &window->filters, &key->link );

     D_MAGIC_SET( key, KeyFilter );

     return DFB_OK;
}

static DFBResult
remove_key_filter( UniqueWindow               *window,
                   DFBInputDeviceKeySymbol     symbol,
                   DFBInputDeviceModifierMask  modifiers )
{
     KeyFilter     *key;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     direct_list_foreach (key, window->filters) {
          D_MAGIC_ASSERT( key, KeyFilter );

          if (key->symbol == symbol && key->modifiers == modifiers)
               break;
     }

     if (!key)
          return DFB_ITEMNOTFOUND;

     unique_input_switch_unset_filter( context->input_switch, key->filter );

     direct_list_remove( &window->filters, &key->link );

     D_MAGIC_CLEAR( key );

     SHFREE( context->shmpool, key );

     return DFB_OK;
}

static void
remove_all_filters( UniqueWindow *window )
{
     DirectLink    *n;
     KeyFilter     *key;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     direct_list_foreach_safe (key, n, window->filters) {
          D_MAGIC_ASSERT( key, KeyFilter );

          unique_input_switch_unset_filter( context->input_switch, key->filter );

          D_MAGIC_CLEAR( key );

          SHFREE( context->shmpool, key );
     }

     window->filters = NULL;
}

/**************************************************************************************************/

static void
dispatch_motion( UniqueWindow           *window,
                 const UniqueInputEvent *event )
{
     DFBWindowEvent evt;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     evt.type    = DWET_MOTION;
     evt.cx      = event->pointer.x;
     evt.cy      = event->pointer.y;
     evt.x       = event->pointer.x - window->bounds.x;
     evt.y       = event->pointer.y - window->bounds.y;
     evt.buttons = event->pointer.buttons;

     dfb_window_post_event( window->window, &evt );
}

static void
dispatch_button( UniqueWindow           *window,
                 const UniqueInputEvent *event )
{
     DFBWindowEvent evt;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     evt.type    = event->pointer.press ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     evt.cx      = event->pointer.x;
     evt.cy      = event->pointer.y;
     evt.x       = event->pointer.x - window->bounds.x;
     evt.y       = event->pointer.y - window->bounds.y;
     evt.button  = event->pointer.button;
     evt.buttons = event->pointer.buttons;

     dfb_window_post_event( window->window, &evt );
}

static void
dispatch_wheel( UniqueWindow           *window,
                const UniqueInputEvent *event )
{
     DFBWindowEvent evt;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     evt.type = DWET_WHEEL;
     evt.step = event->wheel.value;

     dfb_window_post_event( window->window, &evt );
}

static void
dispatch_key( UniqueWindow           *window,
              const UniqueInputEvent *event )
{
     DFBWindowEvent evt;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     evt.type       = event->keyboard.press ? DWET_KEYDOWN : DWET_KEYUP;
     evt.key_code   = event->keyboard.key_code;
     evt.key_id     = event->keyboard.key_id;
     evt.key_symbol = event->keyboard.key_symbol;
     evt.modifiers  = event->keyboard.modifiers;
     evt.locks      = event->keyboard.locks;

     dfb_window_post_event( window->window, &evt );
}

static void
dispatch_channel( UniqueWindow           *window,
                  const UniqueInputEvent *event )
{
     DFBWindowEvent evt;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     switch (event->channel.index) {
          case UDCI_POINTER:
               evt.type = event->channel.selected ? DWET_ENTER : DWET_LEAVE;
               break;

          case UDCI_KEYBOARD:
               evt.type = event->channel.selected ? DWET_GOTFOCUS : DWET_LOSTFOCUS;
               break;

          default:
               return;
     }

     evt.cx = event->channel.x;
     evt.cy = event->channel.y;
     evt.x  = event->channel.x - window->bounds.x;
     evt.y  = event->channel.y - window->bounds.y;

     dfb_window_post_event( window->window, &evt );
}

ReactionResult
_unique_window_input_channel_listener( const void *msg_data,
                                       void       *ctx )
{
     const UniqueInputEvent *event  = msg_data;
     UniqueWindow           *window = ctx;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     D_DEBUG_AT( UniQuE_Window, "_unique_window_input_channel_listener( %p, %p )\n",
                 event, window );

     switch (event->type) {
          case UIET_MOTION:
               dispatch_motion( window, event );
               break;

          case UIET_BUTTON:
               dispatch_button( window, event );
               break;

          case UIET_WHEEL:
               dispatch_wheel( window, event );
               break;

          case UIET_KEY:
               dispatch_key( window, event );
               break;

          case UIET_CHANNEL:
               dispatch_channel( window, event );
               break;

          default:
               D_ONCE( "unknown event type" );
               break;
     }

     return RS_OK;
}

/**************************************************************************************************/

static inline int
get_priority( DFBWindowCapabilities caps, DFBWindowStackingClass stacking )
{
     switch (stacking) {
          case DWSC_UPPER:
               return  1;

          case DWSC_MIDDLE:
               return  0;

          case DWSC_LOWER:
               return -1;

          default:
               D_BUG( "unknown stacking class" );
               break;
     }

     return 0;
}

static inline int
get_index( UniqueWindow *window )
{
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( fusion_vector_contains( &context->windows, window ) );

     return fusion_vector_index_of( &context->windows, window );
}

static DFBResult
update_window( UniqueWindow        *window,
               const DFBRegion     *region,
               DFBSurfaceFlipFlags  flags,
               bool                 complete )
{
     DFBRegion regions[32];
     int       num_regions;

     D_MAGIC_ASSERT( window, UniqueWindow );

     DFB_REGION_ASSERT_IF( region );

     if (complete || stret_region_visible( window->region, region, true, regions, 32, &num_regions )) {
          if (region) {
               DFBRegion area = DFB_REGION_INIT_TRANSLATED( region,
                                                            window->bounds.x,
                                                            window->bounds.y );

               unique_context_update( window->context, &area, 1, flags );
          }
          else {
               DFBRegion area = DFB_REGION_INIT_FROM_RECTANGLE( &window->bounds );
               unique_context_update( window->context, &area, 1, flags );
          }
     }
     else if (num_regions > 0)
          unique_context_update( window->context, regions, num_regions, flags );

     return DFB_OK;
}

static DFBResult
update_frame( UniqueWindow        *window,
              const DFBRegion     *region,
              DFBSurfaceFlipFlags  flags,
              bool                 complete )
{
     DFBRegion regions[32];
     int       num_regions;

     D_MAGIC_ASSERT( window, UniqueWindow );

     DFB_REGION_ASSERT_IF( region );

     if (complete || stret_region_visible( window->frame, region, true, regions, 32, &num_regions )) {
          if (region) {
               DFBRegion area = DFB_REGION_INIT_TRANSLATED( region,
                                                            window->full.x,
                                                            window->full.y );

               unique_context_update( window->context, &area, 1, flags );
          }
          else {
               DFBRegion area = DFB_REGION_INIT_FROM_RECTANGLE( &window->full );
               unique_context_update( window->context, &area, 1, flags );
          }
     }
     else if (num_regions > 0)
          unique_context_update( window->context, regions, num_regions, flags );

     return DFB_OK;
}

/**************************************************************************************************/
/**************************************************************************************************/

static void
insert_window( UniqueContext *context,
               UniqueWindow  *window )
{
     int           index;
     UniqueWindow *other;

     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( window, UniqueWindow );

     /*
      * Iterate from bottom to top,
      * stopping at the first window with a higher priority.
      */
     fusion_vector_foreach (other, index, context->windows) {
          D_MAGIC_ASSERT( other, UniqueWindow );

          if (other->priority > window->priority)
               break;
     }

     /* Insert the window at the acquired position. */
     fusion_vector_insert( &context->windows, window, index );

     stret_region_restack( window->frame, index );
}

static void
remove_window( UniqueWindow *window )
{
     int            index;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( fusion_vector_contains( &context->windows, window ) );

     index = fusion_vector_index_of( &context->windows, window );

     fusion_vector_remove( &context->windows, index );
}

/**************************************************************************************************/

static DFBResult
move_window( UniqueWindow *window,
             int           dx,
             int           dy )
{
     DFBWindowEvent  evt;
     UniqueContext  *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     dfb_rectangle_translate( &window->bounds, dx, dy );
     dfb_rectangle_translate( &window->full, dx, dy );

     stret_region_move( window->frame, dx, dy );

     if (D_FLAGS_IS_SET( window->flags, UWF_VISIBLE )) {
          DFBRegion region = { 0, 0, window->full.w - 1, window->full.h - 1 };

          if (dx > 0)
               region.x1 -= dx;
          else if (dx < 0)
               region.x2 -= dx;

          if (dy > 0)
               region.y1 -= dy;
          else if (dy < 0)
               region.y2 -= dy;

          update_frame( window, &region, DSFLIP_NONE, false );
     }

     /* Send new position */
     evt.type = DWET_POSITION;
     evt.x    = window->bounds.x;
     evt.y    = window->bounds.y;

     unique_window_post_event( window, &evt );

     return DFB_OK;
}

static DFBResult
resize_window( UniqueWindow *window,
               int           width,
               int           height )
{
     DFBResult       ret;
     DFBWindowEvent  evt;
     int             ow;
     int             oh;
     UniqueContext  *context;
     WMShared       *shared;
     DFBRectangle   *sizes;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     shared = context->shared;

     D_MAGIC_ASSERT( shared, WMShared );

     sizes = shared->foo_rects;

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     if (window->surface) {
          ret = dfb_surface_reformat( NULL, window->surface,
                                      width, height, window->surface->format );
          if (ret)
               return ret;
     }

     ow = window->bounds.w;
     oh = window->bounds.h;

     window->bounds.w = width;
     window->bounds.h = height;

     window->full.w  += width - ow;
     window->full.h  += height - oh;

     stret_region_resize( window->frame,  window->full.w,   window->full.h );
     stret_region_resize( window->region, window->bounds.w, window->bounds.h );

     update_foo_frame( window, window->context, window->shared );

     if (D_FLAGS_IS_SET( window->flags, UWF_VISIBLE )) {
          int dw = ow - width;
          int dh = oh - height;

          if (D_FLAGS_IS_SET( window->flags, UWF_DECORATED )) {
               int        bw = dw - sizes[UFI_NE].w + sizes[UFI_E].w;
               int        bh = dh - sizes[UFI_SW].h + sizes[UFI_S].h;
               DFBInsets *in = &window->insets;

               if (bw < 0) {
                    DFBRegion region = { in->l + width + bw, 0, in->l + width - 1, in->t - 1 };

                    update_frame( window, &region, 0, false );
               }

               if (bh < 0) {
                    DFBRegion region = { 0, in->t + height + bh, in->l - 1, in->t + height - 1 };

                    update_frame( window, &region, 0, false );
               }

               if (dw < 0)
                    dw = 0;

               if (dh < 0)
                    dh = 0;

               dw += in->r;
               dh += in->b;

               if (dw > 0) {
                    DFBRegion region = { in->l + width, 0, in->l + width + dw - 1, in->t + height - 1 };

                    update_frame( window, &region, 0, false );
               }

               if (dh > 0) {
                    DFBRegion region = { 0, in->t + height, in->l + width + dw - 1, in->t + height + dh - 1 };

                    update_frame( window, &region, 0, false );
               }
          }
          else {
               if (dw < 0)
                    dw = 0;

               if (dh < 0)
                    dh = 0;

               if (dw > 0) {
                    DFBRegion region = { width, 0, width + dw - 1, height - 1 };

                    update_frame( window, &region, 0, false );
               }

               if (dh > 0) {
                    DFBRegion region = { 0, height, width + dw - 1, height + dh - 1 };

                    update_frame( window, &region, 0, false );
               }
          }
     }

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w    = window->bounds.w;
     evt.h    = window->bounds.h;

     unique_window_post_event( window, &evt );

     unique_input_switch_update( context->input_switch, window->channel );
     //unique_context_update_focus( context );

     return DFB_OK;
}

static DFBResult
restack_window( UniqueWindow           *window,
                UniqueWindow           *relative,
                int                     relation,
                DFBWindowStackingClass  stacking )
{
     int            old;
     int            index;
     int            priority;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     D_MAGIC_ASSERT_IF( relative, UniqueWindow );

     D_ASSERT( relative == NULL || relative == window || relation != 0);

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     /* Change stacking class. */
     if (stacking != window->stacking) {
          window->stacking = stacking;
          window->priority = get_priority( window->caps, stacking );
     }

     /* Get the (new) priority. */
     priority = window->priority;

     /* Get the old index. */
     old = get_index( window );

     /* Calculate the desired index. */
     if (relative) {
          index = get_index( relative );

          if (relation > 0) {
               if (old < index)
                    index--;
          }
          else if (relation < 0) {
               if (old > index)
                    index++;
          }

          index += relation;

          if (index < 0)
               index = 0;
          else if (index > context->windows.count - 1)
               index = context->windows.count - 1;
     }
     else if (relation)
          index = context->windows.count - 1;
     else
          index = 0;

     /* Assure window won't be above any window with a higher priority. */
     while (index > 0) {
          int           below = (old < index) ? index : index - 1;
          UniqueWindow *other = fusion_vector_at( &context->windows, below );

          D_MAGIC_ASSERT( other, UniqueWindow );

          if (priority < other->priority)
               index--;
          else
               break;
     }

     /* Assure window won't be below any window with a lower priority. */
     while (index < context->windows.count - 1) {
          int           above = (old > index) ? index : index + 1;
          UniqueWindow *other = fusion_vector_at( &context->windows, above );

          D_MAGIC_ASSERT( other, UniqueWindow );

          if (priority > other->priority)
               index++;
          else
               break;
     }

     /* Return if index hasn't changed. */
     if (index == old)
          return DFB_OK;

     /* Actually change the stacking order now. */
     fusion_vector_move( &context->windows, old, index );

     stret_region_restack( window->frame, index );

     update_frame( window, NULL, DSFLIP_NONE, (index < old) );

     if (index < old)
          unique_input_switch_update( context->input_switch, window->channel );

     return DFB_OK;
}

static void
set_opacity( UniqueWindow *window,
             u8            opacity )
{
     u8             old;
     UniqueContext *context;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     old = window->opacity;

     if (!dfb_config->translucent_windows && opacity)
          opacity = 0xFF;

     if (old != opacity) {
          bool show = !old && opacity;
          bool hide = old && !opacity;

          window->opacity = opacity;

          if (show) {
               stret_region_enable( window->frame, SRF_ACTIVE );

               D_FLAGS_SET( window->flags, UWF_VISIBLE );
          }

          if (hide)
               D_FLAGS_CLEAR( window->flags, UWF_VISIBLE );

          if (! (window->options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING))) {
               if (opacity == 0xff && old != 0xff)
                    stret_region_enable( window->region, SRF_OPAQUE );
               else if (opacity != 0xff && old == 0xff)
                    stret_region_disable( window->region, SRF_OPAQUE );
          }

          update_frame( window, NULL, DSFLIP_NONE, false );


          /* Check focus after window appeared or disappeared */
          if (/*show ||*/ hide)
               unique_input_switch_update( context->input_switch, window->channel );
//               unique_context_update_focus( context );

          /* If window disappeared... */
          if (hide) {
               stret_region_disable( window->frame, SRF_ACTIVE );

               /* Ungrab pointer/keyboard, release focus */
               unique_input_switch_drop( context->input_switch, window->channel );
          }
     }
}

/**************************************************************************************************/

static void
foo_rects( UniqueWindow  *window,
           UniqueContext *context,
           WMShared      *shared,
           DFBRectangle  *ret_rects )
{
     int           width;
     int           height;
     DFBRectangle *sizes;

     D_MAGIC_ASSERT( window, UniqueWindow );
     D_MAGIC_ASSERT( context, UniqueContext );

     D_MAGIC_ASSERT( shared, WMShared );
     D_ASSERT( ret_rects != NULL );

     width  = window->full.w;
     height = window->full.h;

     sizes = shared->foo_rects;

     if (width <= sizes[UFI_NW].w + sizes[UFI_NE].w)
          width = sizes[UFI_NW].w + sizes[UFI_NE].w + 1;

     if (width <= sizes[UFI_SW].w + sizes[UFI_SE].w)
          width = sizes[UFI_SW].w + sizes[UFI_SE].w + 1;

     if (height <= sizes[UFI_NW].h + sizes[UFI_SW].h)
          height = sizes[UFI_NW].h + sizes[UFI_SW].h + 1;

     if (height <= sizes[UFI_NE].h + sizes[UFI_SE].h)
          height = sizes[UFI_NE].h + sizes[UFI_SE].h + 1;

     ret_rects[UFI_N].x  = sizes[UFI_NW].w;
     ret_rects[UFI_N].y  = 0;
     ret_rects[UFI_N].w  = width - sizes[UFI_NW].w - sizes[UFI_NE].w;
     ret_rects[UFI_N].h  = sizes[UFI_N].h;

     ret_rects[UFI_NE].x = width - sizes[UFI_NE].w;
     ret_rects[UFI_NE].y = 0;
     ret_rects[UFI_NE].w = sizes[UFI_NE].w;
     ret_rects[UFI_NE].h = sizes[UFI_NE].h;

     ret_rects[UFI_E].x  = width - sizes[UFI_E].w;
     ret_rects[UFI_E].y  = sizes[UFI_NE].h;
     ret_rects[UFI_E].w  = sizes[UFI_E].w;
     ret_rects[UFI_E].h  = height - sizes[UFI_NE].h - sizes[UFI_SE].h;

     ret_rects[UFI_SE].x = width - sizes[UFI_SE].w;
     ret_rects[UFI_SE].y = height - sizes[UFI_SE].h;
     ret_rects[UFI_SE].w = sizes[UFI_SE].w;
     ret_rects[UFI_SE].h = sizes[UFI_SE].h;

     ret_rects[UFI_S].x  = sizes[UFI_SW].w;
     ret_rects[UFI_S].y  = height - sizes[UFI_S].h;
     ret_rects[UFI_S].w  = width - sizes[UFI_SE].w - sizes[UFI_SW].w;
     ret_rects[UFI_S].h  = sizes[UFI_S].h;

     ret_rects[UFI_SW].x = 0;
     ret_rects[UFI_SW].y = height - sizes[UFI_SW].h;
     ret_rects[UFI_SW].w = sizes[UFI_SW].w;
     ret_rects[UFI_SW].h = sizes[UFI_SW].h;

     ret_rects[UFI_W].x  = 0;
     ret_rects[UFI_W].y  = sizes[UFI_NW].h;
     ret_rects[UFI_W].w  = sizes[UFI_W].w;
     ret_rects[UFI_W].h  = height - sizes[UFI_NW].h - sizes[UFI_SW].h;

     ret_rects[UFI_NW].x = 0;
     ret_rects[UFI_NW].y = 0;
     ret_rects[UFI_NW].w = sizes[UFI_NW].w;
     ret_rects[UFI_NW].h = sizes[UFI_NW].h;
}

static void
update_foo_frame( UniqueWindow  *window,
                  UniqueContext *context,
                  WMShared      *shared )
{
     int           i;
     DFBRectangle  rects[8];

     D_MAGIC_ASSERT( window, UniqueWindow );
     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( shared, WMShared );

     if (!window->foos[0])
          return;

     foo_rects( window, context, shared, rects );

     for (i=0; i<8; i++)
          window->foos[i]->bounds = DFB_REGION_INIT_FROM_RECTANGLE( &rects[i] );
}

static void
update_flags( UniqueWindow *window )
{
     int i;

     D_MAGIC_ASSERT( window, UniqueWindow );

     if (! (window->options & DWOP_GHOST)) {
          if (window->foos[0]) {
               for (i=0; i<8; i++)
                    window->foos[i]->flags |= SRF_INPUT;
          }

          window->region->flags |= SRF_INPUT;
     }
     else {
          if (window->foos[0]) {
               for (i=0; i<8; i++)
                    window->foos[i]->flags &= ~SRF_INPUT;
          }

          window->region->flags &= ~SRF_INPUT;
     }
}

static DFBResult
create_foos( UniqueWindow  *window,
             UniqueContext *context,
             WMShared      *shared )
{
     int               i;
     DFBResult         ret;
     DFBRectangle      rects[8];
     StretRegionFlags  flags = SRF_ACTIVE | SRF_OUTPUT;

     D_MAGIC_ASSERT( window, UniqueWindow );
     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( shared, WMShared );


     foo_rects( window, context, shared, rects );

     if (! (window->options & DWOP_GHOST))
          flags |= SRF_INPUT;

     for (i=0; i<8; i++) {
          ret = stret_region_create( shared->region_classes[URCI_FOO], window, i, flags, 1,
                                     DFB_RECTANGLE_VALS( &rects[i] ),
                                     window->frame, UNFL_FOREGROUND,
                                     context->shmpool, &window->foos[i] );
          if (ret)
               goto error;
     }

     return DFB_OK;

error:
     for (--i; i>0; --i)
          stret_region_destroy( window->foos[i] );

     return ret;
}

static DFBResult
create_regions( UniqueWindow *window )
{
     DFBResult         ret;
     UniqueContext    *context;
     WMShared         *shared;
     StretRegionFlags  flags = SRF_ACTIVE;

     D_MAGIC_ASSERT( window, UniqueWindow );

     context = window->context;

     D_MAGIC_ASSERT( context, UniqueContext );

     shared = context->shared;

     D_MAGIC_ASSERT( shared, WMShared );

     if (! (window->options & DWOP_GHOST))
          flags |= SRF_INPUT;

     if (! (window->caps & DWCAPS_INPUTONLY))
          flags |= SRF_OUTPUT;

     if (window->options & DWOP_SHAPED)
          flags |= SRF_SHAPED;

     /* Frame */
     ret = stret_region_create( shared->region_classes[URCI_FRAME], window, 0, SRF_NONE, _UNFL_NUM,
                                DFB_RECTANGLE_VALS( &window->full ),
                                context->root, UNRL_USER, context->shmpool, &window->frame );
     if (ret)
          return ret;

     /* Content */
     ret = stret_region_create( shared->region_classes[URCI_WINDOW], window, true, flags, 1,
                                window->insets.l, window->insets.t,
                                window->bounds.w, window->bounds.h,
                                window->frame, UNFL_CONTENT, context->shmpool, &window->region );
     if (ret) {
          stret_region_destroy( window->frame );
          return ret;
     }

     /* Foos */
     if (D_FLAGS_IS_SET( window->flags, UWF_DECORATED )) {
          ret = create_foos( window, context, shared );
          if (ret) {
               stret_region_destroy( window->region );
               stret_region_destroy( window->frame );
               return ret;
          }
     }

     return DFB_OK;
}


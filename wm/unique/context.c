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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/object.h>
#include <fusion/shmalloc.h>

#include <core/coretypes.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/windowstack.h>
#include <core/windows_internal.h>  /* FIXME */

#include <misc/conf.h>
#include <misc/util.h>

#include <unique/context.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Context, "UniQuE/Context", "UniQuE's Stack Context" );


static const React unique_context_globals[] = {
          NULL
};

/**************************************************************************************************/

static void send_key_event   ( UniqueContext       *context,
                               UniqueWindow        *window,
                               const DFBInputEvent *event );

static void send_button_event( UniqueContext       *context,
                               UniqueWindow        *window,
                               const DFBInputEvent *event );

/**************************************************************************************************/

static void handle_key_press     ( UniqueContext       *context,
                                   const DFBInputEvent *event );

static void handle_key_release   ( UniqueContext       *context,
                                   const DFBInputEvent *event );

static void handle_button_press  ( UniqueContext       *context,
                                   const DFBInputEvent *event );

static void handle_button_release( UniqueContext       *context,
                                   const DFBInputEvent *event );

static void handle_motion        ( UniqueContext       *context,
                                   int                  dx,
                                   int                  dy );

static void handle_wheel         ( UniqueContext       *context,
                                   int                  dz );

static void handle_axis_motion   ( UniqueContext       *context,
                                   const DFBInputEvent *event );

static bool update_focus         ( UniqueContext       *context );

/**************************************************************************************************/

static void
context_destructor( FusionObject *object, bool zombie )
{
     DirectLink    *l, *next;
     UniqueContext *context = (UniqueContext*) object;

     D_DEBUG_AT( UniQuE_Context, "destroying %p (stack %p)%s\n",
                 context, context->stack, zombie ? " (ZOMBIE)" : "");

     D_ASSUME( fusion_vector_is_empty( &context->windows ) );

     unique_context_notify( context, UCNF_DESTROYED );

     dfb_windowstack_lock( context->stack );
     dfb_wm_close_stack( context->stack, false );
     dfb_windowstack_unlock( context->stack );

     while (fusion_vector_has_elements( &context->windows )) {
          unique_window_destroy( fusion_vector_at( &context->windows, 0 ) );
     }

     stret_region_destroy( context->root );

     fusion_vector_destroy( &context->windows );

     dfb_surface_unlink( &context->surface );

     dfb_layer_region_unlink( &context->region );

     /* Free grabbed keys. */
     direct_list_foreach_safe (l, next, context->grabbed_keys)
          SHFREE( l );

     D_MAGIC_CLEAR( context );

     fusion_object_destroy( object );
}

FusionObjectPool *
unique_context_pool_create()
{
     return fusion_object_pool_create( "UniQuE Context Pool", sizeof(UniqueContext),
                                       sizeof(UniqueContextNotification), context_destructor );
}

/**************************************************************************************************/

DFBResult
unique_context_create( CoreWindowStack    *stack,
                       CoreLayerRegion    *region,
                       DFBDisplayLayerID   layer_id,
                       WMShared           *shared,
                       UniqueContext     **ret_context )
{
     int            i;
     DFBResult      ret;
     UniqueContext *context;

     D_ASSERT( stack != NULL );
     D_ASSERT( shared != NULL );
     D_ASSERT( ret_context != NULL );

     context = unique_wm_create_context();
     if (!context)
          return DFB_FUSION;

     context->stack    = stack;
     context->shared   = shared;
     context->layer_id = layer_id;
     context->color    = (DFBColor) { 0xc0, 0x30, 0x50, 0x80 };

     fusion_vector_init( &context->windows, 16 );

     for (i=0; i<D_ARRAY_SIZE(context->keys); i++)
          context->keys[i].code = -1;

     /* Create Root Region. */
     ret = stret_region_create( shared->classes[UCI_ROOT], context, 0,
                                SRF_ACTIVE | SRF_OUTPUT, _UNRL_NUM,
                                0, 0, INT_MAX, INT_MAX,
                                NULL, 0, &context->root );
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

     /* Activate Object. */
     fusion_object_activate( &context->object );

     /* Return new context. */
     *ret_context = context;

     return DFB_OK;


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

     D_MAGIC_ASSERT( context, UniqueContext );

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
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;


     for (i=0; i<count; i++) {
          const DFBRegion *update = &regions[i];

          /* Flip the updated region .*/
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
unique_context_process_input( UniqueContext       *context,
                              const DFBInputEvent *event )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );

     /* FIXME: handle multiple devices */
     if (event->flags & DIEF_BUTTONS)
          context->buttons = event->buttons;

     if (event->flags & DIEF_MODIFIERS)
          context->modifiers = event->modifiers;

     if (event->flags & DIEF_LOCKS)
          context->locks = event->locks;

     switch (event->type) {
          case DIET_KEYPRESS:
               handle_key_press( context, event );
               break;

          case DIET_KEYRELEASE:
               handle_key_release( context, event );
               break;

          case DIET_BUTTONPRESS:
               handle_button_press( context, event );
               break;

          case DIET_BUTTONRELEASE:
               handle_button_release( context, event );
               break;

          case DIET_AXISMOTION:
               handle_axis_motion( context, event );
               break;

          default:
               D_ONCE( "unknown input event type" );
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

DFBResult
unique_context_flush_keys( UniqueContext *context )
{
     int i;

     D_MAGIC_ASSERT( context, UniqueContext );

     for (i=0; i<D_ARRAY_SIZE(context->keys); i++) {
          if (context->keys[i].code != -1) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = context->keys[i].code;
               we.key_id     = context->keys[i].id;
               we.key_symbol = context->keys[i].symbol;

               unique_window_post_event( context->keys[i].owner, &we );

               context->keys[i].code = -1;
          }
     }

     return DFB_OK;
}

DFBResult
unique_context_update_focus( UniqueContext *context )
{
     D_MAGIC_ASSERT( context, UniqueContext );

     update_focus( context );

     return DFB_OK;
}

DFBResult
unique_context_switch_focus( UniqueContext *context,
                             UniqueWindow  *to )
{
     DFBWindowEvent  evt;
     UniqueWindow   *from;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_MAGIC_ASSERT_IF( to, UniqueWindow );

     from = context->focused_window;

     if (from == to)
          return DFB_OK;


     context->focused_window = NULL;

     if (from) {
          evt.type = DWET_LOSTFOCUS;

          unique_window_post_event( from, &evt );
     }


     context->focused_window = to;

     if (to) {
/*  FIXME        if (to->surface && to->surface->palette) {
               CoreSurface *surface;

               D_ASSERT( to->primary_region != NULL );

               if (dfb_layer_region_get_surface( to->primary_region, &surface ) == DFB_OK) {
                    if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                         dfb_surface_set_palette( surface, to->surface->palette );

                    dfb_surface_unref( surface );
               }
          }*/

          evt.type = DWET_GOTFOCUS;

          unique_window_post_event( to, &evt );
     }

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
     UniqueWindow    *window = NULL;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( ret_window != NULL );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (stack->cursor.enabled) {
          StretRegion *region;

          if (x < 0)
               x = stack->cursor.x;
          if (y < 0)
               y = stack->cursor.y;

          region = stret_region_at( context->root, x, y, SRF_INPUT );
          if (region)
               window = stret_region_data( region );
     }
     else {
          fusion_vector_foreach_reverse (window, i, context->windows)
               if (window->opacity && !(window->options & DWOP_GHOST) && !(window->caps & DWHC_TOPMOST))
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
          if (window->window->id == window_id) {
               /* don't hand out the cursor window */
               if (window->window->caps & DWHC_TOPMOST)
                    window = NULL;

               break;
          }
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

DFBResult
unique_context_warp_cursor( UniqueContext *context,
                            int            x,
                            int            y )
{
     int dx;
     int dy;

     D_MAGIC_ASSERT( context, UniqueContext );

     dx = x - context->stack->cursor.x;
     dy = y - context->stack->cursor.y;

     handle_motion( context, dx, dy );

     return DFB_OK;
}

/**************************************************************************************************/

static void
send_key_event( UniqueContext       *context,
                UniqueWindow        *window,
                const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     we.type       = (event->type == DIET_KEYPRESS) ? DWET_KEYDOWN : DWET_KEYUP;
     we.key_code   = event->key_code;
     we.key_id     = event->key_id;
     we.key_symbol = event->key_symbol;

     unique_window_post_event( window, &we );
}

static void
send_button_event( UniqueContext       *context,
                   UniqueWindow        *window,
                   const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( window, UniqueWindow );

     D_ASSERT( event != NULL );

     we.type   = (event->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     we.x      = context->stack->cursor.x - window->bounds.x;
     we.y      = context->stack->cursor.y - window->bounds.y;
     we.button = (context->wm_level & 2) ? (event->button + 2) : event->button;

     unique_window_post_event( window, &we );
}

/**************************************************************************************************/

static UniqueWindow *
get_keyboard_window( UniqueContext       *context,
                     const DFBInputEvent *event )
{
     DirectLink *l;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE );

     /* Check explicit key grabs first. */
     direct_list_foreach (l, context->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == event->key_symbol &&
              key->modifiers == context->modifiers)
               return key->owner;
     }

     /* Don't do implicit grabs on keys without a hardware index. */
     if (event->key_code == -1)
          return (context->keyboard_window ?
                  context->keyboard_window : context->focused_window);

     /* Implicitly grab (press) or ungrab (release) key. */
     if (event->type == DIET_KEYPRESS) {
          int           i;
          int           free_key = -1;
          UniqueWindow *window;

          /* Check active grabs. */
          for (i=0; i<8; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (context->keys[i].code == event->key_code)
                    return context->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && context->keys[i].code == -1)
                    free_key = i;
          }

          /* Key is not grabbed, check for explicit keyboard grab or focus. */
          window = context->keyboard_window ?
                   context->keyboard_window : context->focused_window;
          if (!window)
               return NULL;

          /* Check if a free array item was found. */
          if (free_key == -1) {
               D_WARN( "maximum number of owned keys reached" );
               return NULL;
          }

          /* Implicitly grab the key. */
          context->keys[free_key].symbol = event->key_symbol;
          context->keys[free_key].id     = event->key_id;
          context->keys[free_key].code   = event->key_code;
          context->keys[free_key].owner  = window;

          return window;
     }
     else {
          int i;

          /* Lookup owner and ungrab the key. */
          for (i=0; i<8; i++) {
               if (context->keys[i].code == event->key_code) {
                    context->keys[i].code = -1;

                    /* Return owner (NULL if destroyed). */
                    return context->keys[i].owner;
               }
          }
     }

     /* No owner for release event found, discard it. */
     return NULL;
}

static UniqueWindow *
window_at( UniqueContext *context,
           int            x,
           int            y )
{
     DFBResult     ret;
     UniqueWindow *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     ret = unique_context_window_at( context, x, y, &window );
     if (ret)
          return NULL;

     return window;
}

static bool
update_focus( UniqueContext *context )
{
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( context, UniqueContext );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     /* if pointer is not grabbed */
     if (!context->pointer_window) {
          UniqueWindow *before = context->entered_window;
          UniqueWindow *after  = window_at( context, -1, -1 );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    we.type = DWET_LEAVE;
                    we.x    = stack->cursor.x - before->bounds.x;
                    we.y    = stack->cursor.y - before->bounds.y;

                    unique_window_post_event( before, &we );
               }

               /* switch focus */
               unique_context_switch_focus( context, after );

               /* update pointer to window under the cursor */
               context->entered_window = after;

               /* send enter event */
               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->bounds.x;
                    we.y    = stack->cursor.y - after->bounds.y;

                    unique_window_post_event( after, &we );
               }

               return true;
          }
     }

     return false;
}

/**************************************************************************************************/
/**************************************************************************************************/

static bool
handle_wm_key( UniqueContext       *context,
               const DFBInputEvent *event )
{
     int         i, num;
     UniqueWindow *entered;
     UniqueWindow *focused;
     UniqueWindow *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( context->wm_level > 0 );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     entered = context->entered_window;
     focused = context->focused_window;

     switch (DFB_LOWER_CASE(event->key_symbol)) {
          case DIKS_SMALL_X:
               num = fusion_vector_size( &context->windows );

               if (context->wm_cycle <= 0)
                    context->wm_cycle = num;

               if (num) {
                    int looped = 0;
                    int index  = MIN( num, context->wm_cycle );

                    while (index--) {
                         window = fusion_vector_at( &context->windows, index );

                         if ((window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)) ||
                             ! VISIBLE_WINDOW(window->window) || window == context->focused_window)
                         {
                              if (index == 0 && !looped) {
                                   looped = 1;
                                   index  = num - 1;
                              }

                              continue;
                         }

                         unique_window_restack( window, NULL, 1 );
                         unique_window_request_focus( window );

                         break;
                    }

                    context->wm_cycle = index;
               }
               break;

          case DIKS_SMALL_S:
               fusion_vector_foreach (window, i, context->windows) {
                    if (! D_FLAGS_IS_SET( window->flags, UWF_VISIBLE ))
                         continue;

                    if (window->stacking != DWSC_MIDDLE)
                         continue;

                    if (D_FLAGS_IS_SET( window->options, DWOP_GHOST | DWOP_KEEP_STACKING ))
                         continue;

                    unique_window_restack( window, NULL, 1 );
                    unique_window_request_focus( window );

                    break;
               }
               break;

          case DIKS_SMALL_C:
               if (entered) {
                    DFBWindowEvent event;

                    event.type = DWET_CLOSE;

                    unique_window_post_event( entered, &event );
               }
               break;

          case DIKS_SMALL_E:
               update_focus( context );
               break;

          case DIKS_SMALL_A:
               if (focused && !(focused->options & DWOP_KEEP_STACKING)) {
                    unique_window_restack( focused, NULL, 0 );
                    update_focus( context );
               }
               break;

          case DIKS_SMALL_W:
               if (focused && !(focused->options & DWOP_KEEP_STACKING))
                    unique_window_restack( focused, NULL, 1 );
               break;

          case DIKS_SMALL_D:
               if (entered && !(entered->options & DWOP_INDESTRUCTIBLE))
                    dfb_window_destroy( entered->window );

               break;

          case DIKS_SMALL_P:
               /* Enable and show cursor. */
               dfb_windowstack_cursor_set_opacity( context->stack, 0xff );
               dfb_windowstack_cursor_enable( context->stack, true );

               /* Ungrab pointer. */
               context->pointer_window = NULL;

               /* TODO: set new cursor shape, the one current might be completely transparent */
               break;

          case DIKS_PRINT:
               if (dfb_config->screenshot_dir && focused && focused->surface)
                    dfb_surface_dump( focused->surface, dfb_config->screenshot_dir, "dfb_window" );

               break;

          default:
               return false;
     }

     return true;
}

static bool
is_wm_key( DFBInputDeviceKeySymbol key_symbol )
{
     switch (DFB_LOWER_CASE(key_symbol)) {
          case DIKS_SMALL_X:
          case DIKS_SMALL_S:
          case DIKS_SMALL_C:
          case DIKS_SMALL_E:
          case DIKS_SMALL_A:
          case DIKS_SMALL_W:
          case DIKS_SMALL_D:
          case DIKS_SMALL_P:
          case DIKS_PRINT:
               break;

          default:
               return false;
     }

     return true;
}


/**************************************************************************************************/

static void
handle_key_press( UniqueContext       *context,
                  const DFBInputEvent *event )
{
     UniqueWindow *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     if (context->wm_level) {
          switch (event->key_symbol) {
               case DIKS_META:
                    context->wm_level |= 1;
                    break;

               case DIKS_CONTROL:
                    context->wm_level |= 2;
                    break;

               case DIKS_ALT:
                    context->wm_level |= 4;
                    break;

               default:
                    if (handle_wm_key( context, event ))
                         return;

                    break;
          }
     }
     else if (event->key_symbol == DIKS_META) {
          context->wm_level |= 1;
          context->wm_cycle  = 0;
     }

     window = get_keyboard_window( context, event );
     if (window)
          send_key_event( context, window, event );
}

static void
handle_key_release( UniqueContext       *context,
                    const DFBInputEvent *event )
{
     UniqueWindow *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYRELEASE );

     if (context->wm_level) {
          switch (event->key_symbol) {
               case DIKS_META:
                    context->wm_level &= ~1;
                    break;

               case DIKS_CONTROL:
                    context->wm_level &= ~2;
                    break;

               case DIKS_ALT:
                    context->wm_level &= ~4;
                    break;

               default:
                    if (is_wm_key( event->key_symbol ))
                         return;

                    break;
          }
     }

     window = get_keyboard_window( context, event );
     if (window)
          send_key_event( context, window, event );
}

/**************************************************************************************************/

static void
handle_button_press( UniqueContext       *context,
                     const DFBInputEvent *event )
{
     CoreWindowStack *stack;
     UniqueWindow    *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONPRESS );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled)
          return;

     switch (context->wm_level) {
          case 1:
               window = context->entered_window;
               if (window && !(window->options & DWOP_KEEP_STACKING))
                    unique_window_restack( window, NULL, 1 );

               break;

          default:
               window = context->pointer_window ? context->pointer_window : context->entered_window;
               if (window)
                    send_button_event( context, window, event );

               break;
     }
}

static void
handle_button_release( UniqueContext       *context,
                       const DFBInputEvent *event )
{
     CoreWindowStack *stack;
     UniqueWindow    *window;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONRELEASE );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled)
          return;

     switch (context->wm_level) {
          case 1:
               break;

          default:
               window = context->pointer_window ? context->pointer_window : context->entered_window;
               if (window)
                    send_button_event( context, window, event );

               break;
     }
}

/**************************************************************************************************/

static void
handle_motion( UniqueContext *context,
               int            dx,
               int            dy )
{
     int               new_cx, new_cy;
     DFBWindowEvent    we;
     CoreWindowStack  *stack;
     CoreWindowConfig  config;

     D_MAGIC_ASSERT( context, UniqueContext );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled)
          return;

     new_cx = MIN( stack->cursor.x + dx, stack->cursor.region.x2);
     new_cy = MIN( stack->cursor.y + dy, stack->cursor.region.y2);

     new_cx = MAX( new_cx, stack->cursor.region.x1 );
     new_cy = MAX( new_cy, stack->cursor.region.y1 );

     if (new_cx == stack->cursor.x  &&  new_cy == stack->cursor.y)
          return;

     dx = new_cx - stack->cursor.x;
     dy = new_cy - stack->cursor.y;

     stack->cursor.x = new_cx;
     stack->cursor.y = new_cy;

     context->cursor.x += dx;
     context->cursor.y += dy;

     D_ASSERT( stack->cursor.window != NULL );

     dfb_window_move( stack->cursor.window, dx, dy, true );

     switch (context->wm_level) {
          case 7:
          case 6:
          case 5:
          case 4: {
                    UniqueWindow *window = context->entered_window;

                    if (window) {
                         int opacity = window->opacity + dx;

                         if (opacity < 8)
                              opacity = 8;
                         else if (opacity > 255)
                              opacity = 255;

                         config.opacity = opacity;

                         unique_window_set_config( window, &config, CWCF_OPACITY );
                    }

                    break;
               }

          case 3:
          case 2: {
                    UniqueWindow *window = context->entered_window;

                    if (window && !(window->options & DWOP_KEEP_SIZE)) {
                         int width  = window->bounds.w + dx;
                         int height = window->bounds.h + dy;

                         if (width  <   48) width  = 48;
                         if (height <   48) height = 48;
                         if (width  > 2048) width  = 2048;
                         if (height > 2048) height = 2048;

                         if (width != window->bounds.w || height != window->bounds.h) {
                              config.bounds.w = width;
                              config.bounds.h = height;

                              unique_window_set_config( window, &config, CWCF_SIZE );
                         }
                    }

                    break;
               }

          case 1: {
                    UniqueWindow *window = context->entered_window;

                    if (window && !(window->options & DWOP_KEEP_POSITION)) {
                         config.bounds.x = window->bounds.x + dx;
                         config.bounds.y = window->bounds.y + dy;

                         unique_window_set_config( window, &config, CWCF_POSITION );
                    }

                    break;
               }

          case 0:
               if (context->pointer_window) {
                    UniqueWindow *window = context->pointer_window;

                    we.type = DWET_MOTION;
                    we.x    = stack->cursor.x - window->bounds.x;
                    we.y    = stack->cursor.y - window->bounds.y;

                    unique_window_post_event( window, &we );
               }
               else {
                    if (!update_focus( context ) && context->entered_window) {
                         UniqueWindow *window = context->entered_window;

                         we.type = DWET_MOTION;
                         we.x    = stack->cursor.x - window->bounds.x;
                         we.y    = stack->cursor.y - window->bounds.y;

                         unique_window_post_event( window, &we );
                    }
               }

               break;

          default:
               ;
     }
}

static void
handle_wheel( UniqueContext *context,
              int            dz )
{
     UniqueWindow     *window;
     CoreWindowStack  *stack;
     CoreWindowConfig  config;

     D_MAGIC_ASSERT( context, UniqueContext );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled)
          return;

     window = context->pointer_window ? context->pointer_window : context->entered_window;
     if (!window)
          return;

     if (context->wm_level) {
          int opacity = window->opacity + dz*7;

          if (opacity < 0x01)
               opacity = 1;
          if (opacity > 0xFF)
               opacity = 0xFF;

          config.opacity = opacity;

          unique_window_set_config( window, &config, CWCF_OPACITY );
     }
     else {
          DFBWindowEvent we;

          we.type = DWET_WHEEL;
          we.x    = stack->cursor.x - window->bounds.x;
          we.y    = stack->cursor.y - window->bounds.y;
          we.step = dz;

          unique_window_post_event( window, &we );
     }
}

static void
handle_axis_motion( UniqueContext       *context,
                    const DFBInputEvent *event )
{
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_AXISMOTION );

     stack = context->stack;

     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled)
          return;


     if (event->flags & DIEF_AXISREL) {
          int rel = event->axisrel;

          /* handle cursor acceleration */
          if (rel > stack->cursor.threshold)
               rel += (rel - stack->cursor.threshold)
                      * stack->cursor.numerator
                      / stack->cursor.denominator;
          else if (rel < -stack->cursor.threshold)
               rel += (rel + stack->cursor.threshold)
                      * stack->cursor.numerator
                      / stack->cursor.denominator;

          switch (event->axis) {
               case DIAI_X:
                    handle_motion( context, rel, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( context, 0, rel );
                    break;
               case DIAI_Z:
                    handle_wheel( context, - event->axisrel );
                    break;
               default:
                    ;
          }
     }
     else if (event->flags & DIEF_AXISABS) {
          switch (event->axis) {
               case DIAI_X:
                    handle_motion( context, event->axisabs - stack->cursor.x, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( context, 0, event->axisabs - stack->cursor.y );
                    break;
               default:
                    ;
          }
     }
}


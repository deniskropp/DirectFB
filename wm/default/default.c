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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/surfaces.h>
#include <core/palette.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <misc/conf.h>

#include <core/wm_module.h>

DFB_WINDOW_MANAGER( default )

typedef struct {
     DirectLink                    link;

     DFBInputDeviceKeySymbol       symbol;
     DFBInputDeviceModifierMask    modifiers;

     CoreWindow                   *owner;
} GrabbedKey;

/**************************************************************************************************/

typedef struct {
     CoreDFB                      *core;
} WMData;

typedef struct {
     DFBInputDeviceButtonMask      buttons;
     DFBInputDeviceModifierMask    modifiers;
     DFBInputDeviceLockState       locks;

     int                           wm_level;
     int                           wm_cycle;
} StackData;

typedef struct {
     StackData                    *stack_data;
} WindowData;

/**************************************************************************************************/

static void
wm_get_info( CoreWMInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     snprintf( info->name, DFB_CORE_WM_INFO_NAME_LENGTH, "Default" );
     snprintf( info->vendor, DFB_CORE_WM_INFO_VENDOR_LENGTH, "Convergence GmbH" );

     info->wm_data_size     = sizeof(WMData);
     info->stack_data_size  = sizeof(StackData);
     info->window_data_size = sizeof(WindowData);
}

/**************************************************************************************************/

static DFBResult
wm_initialize( CoreDFB *core, void *wm_data )
{
     WMData *data = wm_data;

     data->core = core;

     return DFB_OK;
}

static DFBResult
wm_join( CoreDFB *core, void *wm_data )
{
     WMData *data = wm_data;

     data->core = core;

     return DFB_OK;
}

static DFBResult
wm_shutdown( bool emergency, void *wm_data )
{
     return DFB_OK;
}

static DFBResult
wm_leave( bool emergency, void *wm_data )
{
     return DFB_OK;
}

static DFBResult
wm_suspend( void *wm_data )
{
     return DFB_OK;
}

static DFBResult
wm_resume( void *wm_data )
{
     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
wm_init_stack( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     return DFB_OK;
}

static DFBResult
wm_close_stack( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data )
{
     return DFB_OK;
}

/**************************************************************************************************/

static void
post_event( CoreWindow     *window,
            StackData      *data,
            DFBWindowEvent *event )
{
     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     event->x         = window->stack->cursor.x - window->x;
     event->y         = window->stack->cursor.y - window->y;

     event->buttons   = data->buttons;
     event->modifiers = data->modifiers;
     event->locks     = data->locks;

     dfb_window_post_event( window, event );
}

static void
send_key_event( CoreWindow          *window,
                StackData           *data,
                const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_ASSERT( window != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     we.type       = (event->type == DIET_KEYPRESS) ? DWET_KEYDOWN : DWET_KEYUP;
     we.key_code   = event->key_code;
     we.key_id     = event->key_id;
     we.key_symbol = event->key_symbol;

     post_event( window, data, &we );
}

static void
send_button_event( CoreWindow          *window,
                   StackData           *data,
                   const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_ASSERT( window != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     we.type   = (event->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     we.button = (data->wm_level == 2) ? (event->button + 2) : event->button;

     post_event( window, data, &we );
}

/**************************************************************************************************/

static CoreWindow *
get_keyboard_window( CoreWindowStack     *stack,
                     StackData           *data,
                     const DFBInputEvent *event )
{
     DirectLink *l;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE );

     /* Check explicit key grabs first. */
     direct_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == event->key_symbol &&
              key->modifiers == data->modifiers)
               return key->owner;
     }

     /* Don't do implicit grabs on keys without a hardware index. */
     if (event->key_code == -1)
          return (stack->keyboard_window ?
                  stack->keyboard_window : stack->focused_window);

     /* Implicitly grab (press) or ungrab (release) key. */
     if (event->type == DIET_KEYPRESS) {
          int         i;
          int         free_key = -1;
          CoreWindow *window;

          /* Check active grabs. */
          for (i=0; i<8; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (stack->keys[i].code == event->key_code)
                    return stack->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && stack->keys[i].code == -1)
                    free_key = i;
          }

          /* Key is not grabbed, check for explicit keyboard grab or focus. */
          window = stack->keyboard_window ?
                   stack->keyboard_window : stack->focused_window;
          if (!window)
               return NULL;

          /* Check if a free array item was found. */
          if (free_key == -1) {
               D_WARN( "maximum number of owned keys reached" );
               return NULL;
          }

          /* Implicitly grab the key. */
          stack->keys[free_key].symbol = event->key_symbol;
          stack->keys[free_key].id     = event->key_id;
          stack->keys[free_key].code   = event->key_code;
          stack->keys[free_key].owner  = window;

          return window;
     }
     else {
          int i;

          /* Lookup owner and ungrab the key. */
          for (i=0; i<8; i++) {
               if (stack->keys[i].code == event->key_code) {
                    stack->keys[i].code = -1;

                    /* Return owner (NULL if destroyed). */
                    return stack->keys[i].owner;
               }
          }
     }

     /* No owner for release event found, discard it. */
     return NULL;
}

static CoreWindow*
window_at_pointer( CoreWindowStack *stack,
                   StackData       *data,
                   int              x,
                   int              y )
{
     int i;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     if (!stack->cursor.enabled) {
          for (i=stack->num_windows-1; i>=0; i--) {
               CoreWindow *window = stack->windows[i];

               if (window->opacity && !(window->options & DWOP_GHOST))
                    return window;
          }

          return NULL;
     }

     if (x < 0)
          x = stack->cursor.x;
     if (y < 0)
          y = stack->cursor.y;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (!(w->options & DWOP_GHOST) && w->opacity &&
              x >= w->x  &&  x < w->x + w->width &&
              y >= w->y  &&  y < w->y + w->height) {
               int wx = x - w->x;
               int wy = y - w->y;

               if ( !(w->options & DWOP_SHAPED)  ||
                    !(w->options &(DWOP_ALPHACHANNEL|DWOP_COLORKEYING))
                    || !w->surface ||
                    ((w->options & DWOP_OPAQUE_REGION) &&
                     (wx >= w->opaque.x1  &&  wx <= w->opaque.x2 &&
                      wy >= w->opaque.y1  &&  wy <= w->opaque.y2))) {
                    return w;
               }
               else {
                    void        *data;
                    int          pitch;
                    CoreSurface *surface = w->surface;

                    if ( dfb_surface_soft_lock( surface, DSLF_READ,
                                                &data, &pitch, true ) == DFB_OK ) {
                         if (w->options & DWOP_ALPHACHANNEL) {
                              int alpha = -1;

                              D_ASSERT( DFB_PIXELFORMAT_HAS_ALPHA( surface->format ) );

                              switch (surface->format) {
                                   case DSPF_AiRGB:
                                        alpha = 0xff - (*(__u32*)(data + 4 * wx +
                                                                  pitch * wy) >> 24);
                                        break;
                                   case DSPF_ARGB:
                                        alpha = *(__u32*)(data +
                                                          4 * wx + pitch * wy) >> 24;
                                        break;
                                   case DSPF_ARGB1555:
                                        alpha = *(__u16*)(data + 2 * wx +
                                                          pitch * wy) & 0x8000;
                                        alpha = alpha ? 0xff : 0x00;
                                        break;
                                   case DSPF_ALUT44:
                                        alpha = *(__u8*)(data +
                                                         wx + pitch * wy) & 0xf0;
                                        alpha |= alpha >> 4;
                                        break;
                                   case DSPF_LUT8: {
                                        CorePalette *palette = surface->palette;
                                        __u8         pix     = *((__u8*) data + wx +
                                                                 pitch * wy);

                                        if (palette && pix < palette->num_entries) {
                                             alpha = palette->entries[pix].a;
                                             break;
                                        }

                                        /* fall through */
                                   }

                                   default:
                                        break;
                              }

                              if (alpha) { /* alpha == -1 on error */
                                   dfb_surface_unlock( surface, true );
                                   return w;
                              }

                         }
                         if (w->options & DWOP_COLORKEYING) {
                              int pixel = 0;

                              switch (surface->format) {
                                   case DSPF_ARGB:
                                   case DSPF_AiRGB:
                                   case DSPF_RGB32:
                                        pixel = *(__u32*)(data +
                                                          4 * wx + pitch * wy)
                                                & 0x00ffffff;
                                        break;

                                   case DSPF_RGB24:   //endianess? boh...
                                        pixel = (*(__u32*)(data +
                                                           3 * wx + pitch * wy))
                                                & 0x00ffffff;
                                        break;

                                   case DSPF_RGB16:
                                        pixel = *(__u16*)(data + 2 * wx +
                                                          pitch * wy);
                                        break;

                                   case DSPF_ARGB1555:
                                        pixel = *(__u16*)(data + 2 * wx +
                                                          pitch * wy)
                                                & 0x7fff;
                                        break;

                                   case DSPF_RGB332:
                                   case DSPF_LUT8:
                                        pixel = *(__u8*)(data +
                                                         wx + pitch * wy);
                                        break;

                                   case DSPF_ALUT44:
                                        pixel = *(__u8*)(data +
                                                         wx + pitch * wy)
                                                & 0x0f;
                                        break;

                                   default:
                                        break;
                              }

                              if ( pixel != w->color_key ) {
                                   dfb_surface_unlock( surface, true );
                                   return w;
                              }

                         }

                         dfb_surface_unlock( surface, true );
                    }
               }
          }
     }

     return NULL;
}

bool
update_focus( CoreWindowStack *stack,
              StackData       *data )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     /* if pointer is not grabbed */
     if (!stack->pointer_window && !data->wm_level) {
          CoreWindow *before = stack->entered_window;
          CoreWindow *after  = window_at_pointer( stack, data, -1, -1 );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    we.type = DWET_LEAVE;
                    we.x    = stack->cursor.x - before->x;
                    we.y    = stack->cursor.y - before->y;

                    post_event( before, data, &we );
               }

               /* switch focus and send enter event */
               dfb_windowstack_switch_focus( stack, after );

               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->x;
                    we.y    = stack->cursor.y - after->y;

                    post_event( after, data, &we );
               }

               /* update pointer to window under the cursor */
               stack->entered_window = after;

               return true;
          }
     }

     return false;
}

/**************************************************************************************************/

static bool
handle_wm_key( CoreWindowStack     *stack,
               StackData           *data,
               const DFBInputEvent *event )
{
     int         i;
     CoreWindow *entered;
     CoreWindow *focused;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( data->wm_level > 0 );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     entered = stack->entered_window;
     focused = stack->focused_window;

     switch (DFB_LOWER_CASE(event->key_symbol)) {
          case DIKS_SMALL_X:
               if (data->wm_cycle <= 0)
                    data->wm_cycle = stack->num_windows;

               if (stack->num_windows) {
                    int looped = 0;
                    int index = MIN( stack->num_windows, data->wm_cycle );

                    while (index--) {
                         CoreWindow *window = stack->windows[index];

                         if ((window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)) ||
                             ! VISIBLE_WINDOW(window) || window == stack->focused_window)
                         {
                              if (index == 0 && !looped) {
                                   looped = 1;
                                   index = stack->num_windows - 1;
                              }

                              continue;
                         }

                         dfb_window_raisetotop( window );
                         dfb_window_request_focus( window );

                         break;
                    }

                    data->wm_cycle = index;
               }
               break;

          case DIKS_SMALL_S:
               for (i=0; i<stack->num_windows; i++) {
                    CoreWindow *window = stack->windows[i];

                    if (VISIBLE_WINDOW(window) && window->stacking == DWSC_MIDDLE &&
                        ! (window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)))
                    {
                         dfb_window_raisetotop( window );
                         dfb_window_request_focus( window );
                         break;
                    }
               }
               break;

          case DIKS_SMALL_C:
               if (entered) {
                    DFBWindowEvent event;

                    event.type = DWET_CLOSE;

                    post_event( entered, data, &event );
               }
               break;

          case DIKS_SMALL_E:
               dfb_windowstack_switch_focus( stack, window_at_pointer( stack, data, -1, -1 ) );
               break;

          case DIKS_SMALL_A:
               if (focused && !(focused->options & DWOP_KEEP_STACKING)) {
                    dfb_window_lowertobottom( focused );
                    dfb_windowstack_switch_focus( stack, window_at_pointer( stack, data, -1, -1 ) );
               }
               break;

          case DIKS_SMALL_W:
               if (focused && !(focused->options & DWOP_KEEP_STACKING))
                    dfb_window_raisetotop( stack->focused_window );
               break;

          case DIKS_SMALL_D:
               if (entered && !(entered->options & DWOP_INDESTRUCTIBLE))
                    dfb_window_destroy( entered );

               break;

          case DIKS_SMALL_P:
               dfb_windowstack_cursor_set_opacity( stack, 0xff );
               dfb_windowstack_cursor_enable( stack, true );

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

static DFBResult
handle_key_press( CoreWindowStack     *stack,
                  StackData           *data,
                  const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     switch (data->wm_level) {
          case 3:
          case 2:
               handle_wm_key( stack, data, event );
               break;

          case 1:
               switch (event->key_symbol) {
                    case DIKS_CONTROL:
                         data->wm_level = 2;
                         return DFB_OK;

                    case DIKS_ALT:
                         data->wm_level = 3;
                         return DFB_OK;

                    default:
                         if (handle_wm_key( stack, data, event ))
                              return DFB_OK;
               }

               /* fall through */

          case 0:
               if (event->key_symbol == DIKS_META)
                    data->wm_level = 1;

               window = get_keyboard_window( stack, data, event );
               if (window)
                    send_key_event( window, data, event );

               break;

          default:
               D_BUG( "unknown wm level" );
               return DFB_BUG;
     }

     return DFB_OK;
}

static DFBResult
handle_key_release( CoreWindowStack     *stack,
                    StackData           *data,
                    const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYRELEASE );

     switch (data->wm_level) {
          case 3:
               if (event->key_symbol == DIKS_ALT)
                    data->wm_level = 1;

               break;

          case 2:
               if (event->key_symbol == DIKS_CONTROL)
                    data->wm_level = 1;

               break;

          case 1:
               if (is_wm_key( event->key_symbol ))
                    break;

               if (event->key_symbol == DIKS_META) {
                    data->wm_level = 0;
                    data->wm_cycle = 0;
               }

               /* fall through */

          case 0:
               window = get_keyboard_window( stack, data, event );
               if (window)
                    send_key_event( window, data, event );

               break;

          default:
               D_BUG( "unknown wm level" );
               return DFB_BUG;
     }

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
handle_button_press( CoreWindowStack     *stack,
                     StackData           *data,
                     const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONPRESS );

     if (!stack->cursor.enabled)
          return DFB_OK;

     switch (data->wm_level) {
          case 1:
               window = stack->entered_window;
               if (window && !(window->options & DWOP_KEEP_STACKING))
                    dfb_window_raisetotop( stack->entered_window );

               break;

          case 3:
          case 2:
          case 0:
               window = stack->pointer_window ? stack->pointer_window : stack->entered_window;
               if (window)
                    send_button_event( window, data, event );

               break;

          default:
               D_BUG( "unknown wm level" );
               return DFB_BUG;
     }

     return DFB_OK;
}

static DFBResult
handle_button_release( CoreWindowStack     *stack,
                       StackData           *data,
                       const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONRELEASE );

     if (!stack->cursor.enabled)
          return DFB_OK;

     switch (data->wm_level) {
          case 1:
               break;

          case 3:
          case 2:
          case 0:
               window = stack->pointer_window ? stack->pointer_window : stack->entered_window;
               if (window)
                    send_button_event( window, data, event );

               break;

          default:
               D_BUG( "unknown wm level" );
               return DFB_BUG;
     }

     return DFB_OK;
}

/**************************************************************************************************/

static void
handle_motion( CoreWindowStack *stack,
               StackData       *data,
               int              dx,
               int              dy )
{
     int            new_cx, new_cy;
     DFBWindowEvent we;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

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

     D_ASSERT( stack->cursor.window != NULL );

     dfb_window_move( stack->cursor.window, dx, dy );

     switch (data->wm_level) {
          case 3: {
                    CoreWindow *window = stack->entered_window;

                    if (window) {
                         int opacity = window->opacity + dx;

                         if (opacity < 8)
                              opacity = 8;
                         else if (opacity > 255)
                              opacity = 255;

                         dfb_window_set_opacity( window, opacity );
                    }

                    break;
               }

          case 2: {
                    CoreWindow *window = stack->entered_window;

                    if (window && !(window->options & DWOP_KEEP_SIZE)) {
                         int width  = window->width  + dx;
                         int height = window->height + dy;

                         if (width  <   48) width  = 48;
                         if (height <   48) height = 48;
                         if (width  > 2048) width  = 2048;
                         if (height > 2048) height = 2048;

                         if (width != window->width || height != window->height)
                              dfb_window_resize( window, width, height );
                    }

                    break;
               }

          case 1: {
                    CoreWindow *window = stack->entered_window;

                    if (window && !(window->options & DWOP_KEEP_POSITION))
                         dfb_window_move( window, dx, dy );

                    break;
               }

          case 0:
               if (stack->pointer_window) {
                    we.type = DWET_MOTION;

                    post_event( stack->pointer_window, data, &we );
               }
               else {
                    if (!update_focus( stack, data ) && stack->entered_window) {
                         we.type = DWET_MOTION;

                         post_event( stack->entered_window, data, &we );
                    }
               }

               break;

          default:
               ;
     }
}

static void
handle_wheel( CoreWindowStack *stack,
              StackData       *data,
              int              dz )
{
     DFBWindowEvent we;
     CoreWindow *window = NULL;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     if (!stack->cursor.enabled)
          return;

     window = stack->pointer_window ? stack->pointer_window : stack->entered_window;

     if (window) {
          if (data->wm_level) {
               int opacity = window->opacity + dz*7;

               if (opacity < 0x01)
                    opacity = 1;
               if (opacity > 0xFF)
                    opacity = 0xFF;

               dfb_window_set_opacity( window, opacity );
          }
          else {
               we.type = DWET_WHEEL;
               we.step = dz;

               post_event( window, data, &we );
          }
     }
}

static DFBResult
handle_axis_motion( CoreWindowStack     *stack,
                    StackData           *data,
                    const DFBInputEvent *event )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_AXISMOTION );

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
                    handle_motion( stack, data, rel, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( stack, data, 0, rel );
                    break;
               case DIAI_Z:
                    handle_wheel( stack, data, - event->axisrel );
                    break;
               default:
                    ;
          }
     }
     else if (event->flags & DIEF_AXISABS) {
          switch (event->axis) {
               case DIAI_X:
                    handle_motion( stack, data, event->axisabs - stack->cursor.x, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( stack, data, 0, event->axisabs - stack->cursor.y );
                    break;
               default:
                    ;
          }
     }

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
wm_process_input( CoreWindowStack     *stack,
                  void                *wm_data,
                  void                *stack_data,
                  const DFBInputEvent *event )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( event != NULL );

     D_HEAVYDEBUG( "WM/Default: Processing input event (device %d, type 0x%08x, flags 0x%08x)...\n",
                   event->device_id, event->type, event->flags );

     /* FIXME: handle multiple devices */
     if (event->flags & DIEF_BUTTONS)
          data->buttons = event->buttons;

     if (event->flags & DIEF_MODIFIERS)
          data->modifiers = event->modifiers;

     if (event->flags & DIEF_LOCKS)
          data->locks = event->locks;

     switch (event->type) {
          case DIET_KEYPRESS:
               return handle_key_press( stack, stack_data, event );

          case DIET_KEYRELEASE:
               return handle_key_release( stack, stack_data, event );

          case DIET_BUTTONPRESS:
               return handle_button_press( stack, stack_data, event );

          case DIET_BUTTONRELEASE:
               return handle_button_release( stack, stack_data, event );

          case DIET_AXISMOTION:
               return handle_axis_motion( stack, stack_data, event );

          default:
               D_ONCE( "unknown input event type" );
               break;
     }

     return DFB_UNSUPPORTED;
}

/**************************************************************************************************/

static DFBResult
wm_window_at( CoreWindowStack  *stack,
              void             *wm_data,
              void             *stack_data,
              int               x,
              int               y,
              CoreWindow      **ret_window )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     *ret_window = window_at_pointer( stack, stack_data, x, y );

     return DFB_OK;
}

static DFBResult
wm_warp_cursor( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data,
                int              x,
                int              y )
{
     int dx, dy;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     dx = x - stack->cursor.x;
     dy = y - stack->cursor.y;

     handle_motion( stack, stack_data, dx, dy );

     return DFB_OK;
}

static DFBResult
wm_update_focus( CoreWindowStack *stack,
                 void            *wm_data,
                 void            *stack_data )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     update_focus( stack, stack_data );

     return DFB_OK;
}


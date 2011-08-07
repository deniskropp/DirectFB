/*
   (c) Copyright 2001-2007  directfb.org
   (c) Copyright 2000-2004  convergence (integrated) media GmbH.

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
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>

#include <gfx/convert.h>

#include <misc/util.h>

#include <sawman_config.h>

#include "sawman_window.h"

D_DEBUG_DOMAIN( SaWMan_Draw, "SaWMan/Draw", "SaWMan window manager drawing" );

/**********************************************************************************************************************/

void sawman_draw_cursor    ( CoreWindowStack *stack,
                             CardState       *state,
                             CoreSurface     *surface,
                             DFBRegion       *region,
                             int              x,
                             int              y )
{
     DFBRectangle            src;
     DFBRectangle            clip;
     DFBSurfaceBlittingFlags flags = DSBLIT_BLEND_ALPHACHANNEL;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     D_ASSUME( stack->cursor.opacity > 0 );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 stack, DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     /* Initialize source rectangle. */
     src.x = region->x1 - x + stack->cursor.hot.x;
     src.y = region->y1 - y + stack->cursor.hot.y;
     src.w = region->x2 - region->x1 + 1;
     src.h = region->y2 - region->y1 + 1;

     /* Initialize source clipping rectangle */
     clip.x = clip.y = 0;
     clip.w = stack->cursor.surface->config.size.w;
     clip.h = stack->cursor.surface->config.size.h;

     /* Intersect rectangles */
     if (!dfb_rectangle_intersect( &src, &clip ))
          return;

     /* Set destination. */
     if (surface) {
          state->destination  = surface;
          state->modified    |= SMF_DESTINATION;
     }

     /* Use global alpha blending. */
     if (stack->cursor.opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          if (stack->cursor.surface->config.caps & DSCAPS_PREMULTIPLIED) {
               /* Need to premultiply source with Ac? */
               flags |= DSBLIT_SRC_PREMULTCOLOR;

               dfb_state_set_src_blend( state, DSBF_ONE );
          }
          else
               dfb_state_set_src_blend( state, DSBF_SRCALPHA );

          /* Set opacity as blending factor. */
          if (state->color.a != stack->cursor.opacity) {
               state->color.a   = stack->cursor.opacity;
               state->modified |= SMF_COLOR;
          }
     }

     /* Different compositing methods depending on destination format. */
     if (flags & DSBLIT_BLEND_ALPHACHANNEL) {
          if (DFB_PIXELFORMAT_HAS_ALPHA( state->destination->config.format )) {
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
               if (! (stack->cursor.surface->config.caps & DSCAPS_PREMULTIPLIED))
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
               if (stack->cursor.surface->config.caps & DSCAPS_PREMULTIPLIED) {
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

     /* Set clipping region. */
     dfb_state_set_clip( state, region );

     /* Blit from the window to the region being updated. */
     dfb_gfxcard_blit( &src, region->x1, region->y1, state );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;

     /* Reset destination. */
     if (surface) {
          state->destination  = NULL;
          state->modified    |= SMF_DESTINATION;
     }
}

static void
draw_border( SaWManWindow    *sawwin,
             CardState       *state,
             const DFBRegion *region,
             int              thickness,
             bool             right_eye )
{
     int                     i;
     DFBRegion               old_clip;
     DFBRectangle            rects[thickness];
     CoreWindow             *window;
     const SaWManBorderInit *border;
     const DFBColor         *colors;
     const int              *indices;
     unsigned int            num_colors;
     unsigned int            num_indices;
     int                     offset;

     window = sawwin->window;
     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %p, %d,%d-%dx%d, %d )\n", __FUNCTION__,
                 sawwin, state, DFB_RECTANGLE_VALS_FROM_REGION( region ), thickness );

     if (thickness > sawwin->bounds.w / 2)
          thickness = sawwin->bounds.w / 2;

     if (thickness > sawwin->bounds.h / 2)
          thickness = sawwin->bounds.h / 2;

     /* Check thickness. */
     if (thickness < 1)
          return;

     /* Initialize border rectangles. */
     rects[0] = sawwin->bounds;

     /* Translate for stereo effect. */
     offset = window->config.z;
     offset *= right_eye ? -1 : 1;
     dfb_rectangle_translate( &rects[0], offset, 0 );

     for (i=1; i<thickness; i++) {
          rects[i].x = rects[i-1].x + 1;
          rects[i].y = rects[i-1].y + 1;
          rects[i].w = rects[i-1].w - 2;
          rects[i].h = rects[i-1].h - 2;
     }

     /* Save clipping region. */
     old_clip = state->clip;

     /* Change clipping region. */
     dfb_state_set_clip( state, region );

     border = &sawman_config->borders[sawman_window_priority(sawwin)];

     if (window->flags & CWF_FOCUSED) {
          colors      = border->focused;
          indices     = border->focused_index;
          num_colors  = D_ARRAY_SIZE(border->focused);
          num_indices = D_ARRAY_SIZE(border->focused_index);
     }
     else {
          colors      = border->unfocused;
          indices     = border->unfocused_index;
          num_colors  = D_ARRAY_SIZE(border->unfocused);
          num_indices = D_ARRAY_SIZE(border->unfocused_index);
     }

     /* Draw border rectangles. */
     for (i=0; i<thickness; i++) {
          dfb_state_set_color_or_index( state,
                                        &colors[i*num_colors/thickness],
                                        indices[i*num_indices/thickness] );

          dfb_gfxcard_drawrectangle( &rects[i], state );
     }

     /* Restore clipping region. */
     dfb_state_set_clip( state, &old_clip );
}

static void
draw_window( SaWManTier   *tier,
             SaWManWindow *sawwin,
             SaWManWindow *sawwin2,
             CardState    *state,
             DFBRegion    *region,
             bool          alpha_channel,
             bool          right_eye )
{
     SaWMan                  *sawman;
     CoreWindow              *window;
     DFBSurfaceBlittingFlags  flags = DSBLIT_NOFX;
     DFBRectangle             dst;
     DFBRectangle             src;
     DFBRegion                clip;
     DFBRegion                old_clip;
     int                      offset;
     DFBSurfaceStereoEye      eye;
     DFBSurfaceStereoEye      old_eye = 0; // silence compiler

     D_MAGIC_ASSERT( sawwin,  SaWManWindow );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     sawman = sawwin->sawman;
     window = sawwin->window;
     dst    = sawwin->dst;
     src    = sawwin->src;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( window != NULL );
     D_ASSERT( window->surface != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin, DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     /* Modify dst for stereo offset. */
     offset = window->config.z;
     offset *= right_eye ? -1 : 1;
     dfb_rectangle_translate( &dst, offset, 0 );

     /* Setup clipping region. */
     clip = *region;

     if (!dfb_region_rectangle_intersect( &clip, &dst ))
          return;

     /* Backup clipping region. */
     old_clip = state->clip;

     /* Use per pixel alpha blending. */
     if (alpha_channel && (window->config.options & DWOP_ALPHACHANNEL))
          flags |= DSBLIT_BLEND_ALPHACHANNEL;

     /* Use global alpha blending. */
     if (window->config.opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          if (window->surface->config.caps & DSCAPS_PREMULTIPLIED) {
               /* Need to premultiply source with Ac? */
               flags |= DSBLIT_SRC_PREMULTCOLOR;

               dfb_state_set_src_blend( state, DSBF_ONE );
          }
          else
               dfb_state_set_src_blend( state, DSBF_SRCALPHA );

          /* Set opacity as blending factor. */
          if (state->color.a != window->config.opacity) {
               state->color.a   = window->config.opacity;
               state->modified |= SMF_COLOR;
          }
     }

     /* if we specified some kind of color, we will colorise. no DWCAPS_COLOR here. */
     if (window->config.color.a != 0) {
          DFBColor c;
          
          flags |= DSBLIT_COLORIZE;

          c   = window->config.color;
          c.a = state->color.a;

          if (! DFB_COLOR_EQUAL( c, state->color )) {
               state->color     = c;
               state->modified |= SMF_COLOR;
          }
     }

     /* Use source color keying. */
     if (window->config.options & DWOP_COLORKEYING) {
          flags |= DSBLIT_SRC_COLORKEY;

          D_DEBUG_AT( SaWMan_Draw, "  -> key 0x%08x\n", window->config.color_key );

          /* Set window color key. */
          dfb_state_set_src_colorkey( state, window->config.color_key );
     }

     /* Use automatic deinterlacing. */
     if (window->surface->config.caps & DSCAPS_INTERLACED)
          flags |= DSBLIT_DEINTERLACE;

     /* Different compositing methods depending on destination format. */
     if (flags & DSBLIT_BLEND_ALPHACHANNEL) {
          if (DFB_PIXELFORMAT_HAS_ALPHA( state->destination->config.format )) {
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
               if (! (window->surface->config.caps & DSCAPS_PREMULTIPLIED))
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
               if (window->surface->config.caps & DSCAPS_PREMULTIPLIED) {
                    /* Need to premultiply source with Ac? */
                    if (flags & DSBLIT_BLEND_COLORALPHA)
                         flags |= DSBLIT_SRC_PREMULTCOLOR;

                    dfb_state_set_src_blend( state, DSBF_ONE );
               }
               else
                    dfb_state_set_src_blend( state, DSBF_SRCALPHA );
          }
     }

     /* Use color (key) protection if layer is keyed. */
     if (tier->context->config.options & DLOP_SRC_COLORKEY) {
          flags |= DSBLIT_COLORKEY_PROTECT;

          dfb_state_set_colorkey( state, &tier->key );
     }

     /* Set blitting flags. */
     dfb_state_set_blitting_flags( state, flags );

     /* Set render options. */
     if (sawman->scaling_mode == SWMSM_SMOOTH_SW)
          dfb_state_set_render_options( state, DSRO_SMOOTH_DOWNSCALE | DSRO_SMOOTH_UPSCALE );
     else
          dfb_state_set_render_options( state, DSRO_NONE );

     /* Set blitting source. */
     state->source    = window->surface;
     state->modified |= SMF_SOURCE;

     /* Save current eye */
     if (window->caps & DWCAPS_STEREO) {
          old_eye = dfb_surface_get_stereo_eye( window->surface );
          eye = right_eye ? DSSE_RIGHT : DSSE_LEFT;
          dfb_surface_set_stereo_eye( window->surface, eye );
     }

     D_DEBUG_AT( SaWMan_Draw, "  [][] %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &clip ) );

     /* Change clipping region. */
     dfb_state_set_clip( state, &clip );

     D_DEBUG_AT( SaWMan_Draw, "    => %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS( &dst ), DFB_RECTANGLE_VALS( &src ) );

#ifndef OLD_COREWINDOWS_STRUCTURE
     if (sawwin2) {
          CoreWindow   *window2;
          DFBRectangle *src2;
          DFBPoint      p1,p2;
     
          D_MAGIC_ASSERT( sawwin2, SaWManWindow );
          window2 = sawwin2->window;
          D_ASSERT( window2 != NULL );
          D_ASSERT( window2->surface != NULL );
          src2 = &sawwin2->src;

          state->source2  = window2->surface;
          state->modified = SMF_SOURCE2;

          p1.x = src2->x;
          p1.y = src2->y;
          p2.x = sawwin->dst.x;
          p2.y = sawwin->dst.y;
          dfb_gfxcard_batchblit2( &src, &p1, &p2, 1, state );
     }
     else
#endif
     {
          /* Scale window to the screen clipped by the region being updated. */
          dfb_gfxcard_stretchblit( &src, &dst, state );
     }

     /* Restore clipping region. */
     dfb_state_set_clip( state, &old_clip );

     /* Restore eye. */
     if (window->caps & DWCAPS_STEREO)
          dfb_surface_set_stereo_eye( window->surface, old_eye );
}

static void
draw_window_color( SaWManWindow *sawwin,
                   CardState    *state,
                   DFBRegion    *region,
                   bool          alpha_channel,
                   bool          right_eye )
{
     SaWMan                  *sawman;
     CoreWindow              *window;
     DFBSurfaceDrawingFlags   flags = DSDRAW_NOFX;
     DFBRectangle             dst;
     DFBRegion                clip;
     DFBRegion                old_clip;
     DFBColor                 color;
     int                      offset;


     sawman = sawwin->sawman;
     window = sawwin->window;
     dst    = sawwin->dst;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin, DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     color = window->config.color;

     /* Modify dst for stereo offset. */
     offset = window->config.z;
     offset *= right_eye ? -1 : 1;
     dfb_rectangle_translate( &dst, offset, 0 );

     /* Setup clipping region. */
     clip = *region;

     if (!dfb_region_rectangle_intersect( &clip, &dst ))
          return;

     /* Backup clipping region. */
     old_clip = state->clip;

     /* Use per pixel alpha blending. */
     if (alpha_channel && (window->config.options & DWOP_ALPHACHANNEL))
          flags |= DSDRAW_BLEND;

     /* we assume the passed color is never premultiplied */
     flags |= DSDRAW_SRC_PREMULTIPLY;

     /* when not opaque, we simply adjust the color */
     if (window->config.opacity != 0xFF) {
          flags   |= DSDRAW_BLEND;
          color.a  = (color.a * window->config.opacity) >> 8;
     }

     dfb_state_set_drawing_flags( state, flags );

     if (DFB_PIXELFORMAT_IS_INDEXED( state->destination->config.format )) {
          unsigned int i = dfb_palette_search( state->destination->palette,
                                               color.r, color.g, color.b, color.a );
          dfb_state_set_color_index( state, i );
     }
     else
          dfb_state_set_color( state, &color );

     D_DEBUG_AT( SaWMan_Draw, "  [][] %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &clip ) );

     /* Change clipping region. */
     dfb_state_set_clip( state, &clip );

     D_DEBUG_AT( SaWMan_Draw, "    => %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS( &dst ) );

     dfb_gfxcard_fillrectangles( &dst, 1, state );

     /* Restore clipping region. */
     dfb_state_set_clip( state, &old_clip );
}

void
sawman_draw_window( SaWManTier   *tier,
                    SaWManWindow *sawwin,
                    CardState    *state,
                    DFBRegion    *pregion,
                    bool          alpha_channel,
                    bool          right_eye )
{
     CoreWindow      *window;
     DFBRegion        xregion = *pregion;
     DFBRegion       *region  = &xregion;
     int              border;
     bool             input;
     int              offset;
     DFBRectangle     dst;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     window = sawwin->window;

     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin, DFB_RECTANGLE_VALS_FROM_REGION( pregion ) );

     border = sawman_window_border( sawwin );

     /* if input only, we only draw the border */
     input = (window->caps & DWCAPS_INPUTONLY) || (window->config.options & DWOP_INPUTONLY);
     
     offset = window->config.z;
     offset *= right_eye ? -1 : 1;

     dst = sawwin->dst;
     dfb_rectangle_translate( &dst, offset, 0 );

     if (!input) {
          D_DEBUG_AT( SaWMan_Draw, "  -> dst     %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &dst ) );

          if (dfb_region_intersect( region,
                                    sawwin->bounds.x + border + offset,
                                    sawwin->bounds.y + border,
                                    sawwin->bounds.x + sawwin->bounds.w - border - 1 + offset,
                                    sawwin->bounds.y + sawwin->bounds.h - border - 1 ))
          {
               D_DEBUG_AT( SaWMan_Draw, "  -> region  %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( region ) );

               if (dfb_region_rectangle_intersect( region, &dst )) {
                    if( window->surface )
                         draw_window( tier, sawwin, 0, state, region, alpha_channel, right_eye );
                    else if( window->caps & DWCAPS_COLOR )
                         draw_window_color( sawwin, state, region, alpha_channel, right_eye );
               }
          }
     }

     if (border)
          draw_border( sawwin, state, pregion, border, right_eye );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

void
sawman_draw_two_windows( SaWManTier   *tier,
                         SaWManWindow *sawwin1,
                         SaWManWindow *sawwin2,
                         CardState    *state,
                         DFBRegion    *pregion,
                         bool          right_eye )
{
     CoreWindow      *window1;
     CoreWindow      *window2;
     CoreWindowStack *stack;
     DFBRegion        xregion = *pregion;
     DFBRegion       *region  = &xregion;

     D_MAGIC_ASSERT( sawwin1, SaWManWindow );
     D_MAGIC_ASSERT( sawwin2, SaWManWindow );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     window1 = sawwin1->window;
     window2 = sawwin2->window;
     stack  = sawwin1->stack;

     D_ASSERT( window1 != NULL );
     D_ASSERT( window2 != NULL );
     D_ASSERT( stack != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin1, sawwin2, DFB_RECTANGLE_VALS_FROM_REGION( pregion ) );

     /* we assume that the windows are both visible and overlapping */
     bool color1 = window1->caps & DWCAPS_COLOR;
     bool color2 = window2->caps & DWCAPS_COLOR;

     /* if input only, we only draw the border */
     bool input1 = (window1->caps & DWCAPS_INPUTONLY) || (window1->config.options & DWOP_INPUTONLY);
     bool input2 = (window2->caps & DWCAPS_INPUTONLY) || (window2->config.options & DWOP_INPUTONLY);
     
     int border1 = sawman_window_border( sawwin1 );
     int border2 = sawman_window_border( sawwin2 );

     if (color1 || color2) {
          /* window 1 */
          if (color1)
               draw_window_color( sawwin1, state, region, false, right_eye );
          else if (!input1)
               draw_window( tier, sawwin1, 0, state, region, false, right_eye );

          if (border1)
               draw_border( sawwin1, state, pregion, border1, right_eye );

          /* window 2 */
          if (color2)
               draw_window_color( sawwin2, state, region, true, right_eye );
          else if (!input2)
               draw_window( tier, sawwin2, 0, state, region, true, right_eye );
     }
     else {
#ifndef OLD_COREWINDOWS_STRUCTURE
          /* TODO: using the card capabilities will not work if
             BLIT2 functionality is state dependant */
          CardCapabilities caps;
          dfb_gfxcard_get_capabilities( &caps );

          /* scaling disallowed */
          if (    (sawwin1->src.w == sawwin1->dst.w)
               && (sawwin1->src.h == sawwin1->dst.h) 
               && (caps.accel & DFXL_BLIT2) )
          {
               D_ASSUME ( border1 == 0 );
               draw_window( tier, sawwin1, sawwin2, state, region, true, right_eye );
          }
          else
#endif
          {
               draw_window( tier, sawwin1, 0, state, region, false, right_eye );
               if (border1)
                    draw_border( sawwin1, state, pregion, border1, right_eye );

               draw_window( tier, sawwin2, 0, state, region, true, right_eye );
          }
     }

     if (border2)
          draw_border( sawwin2, state, pregion, border2, right_eye );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}


void
sawman_draw_background( SaWManTier *tier, CardState *state, DFBRegion *region )
{
     DFBRectangle     dst;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( tier, SaWManTier );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__, tier,
                 DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     stack = tier->stack;
     D_ASSERT( stack != NULL );

     D_ASSERT( stack->bg.image != NULL || (stack->bg.mode != DLBM_IMAGE &&
                                           stack->bg.mode != DLBM_TILE) );

     /* Initialize destination rectangle. */
     dfb_rectangle_from_region( &dst, region );

     switch (stack->bg.mode) {
          case DLBM_COLOR: {
               /* Set the background color. */
               dfb_state_set_color_or_index( state, &stack->bg.color, stack->bg.color_index );

               if (stack->bg.color_index < 0 || !state->destination->palette)
                    D_DEBUG_AT( SaWMan_Draw, "  -> fill %02x %02x %02x %02x [%d]\n",
                                stack->bg.color.a, stack->bg.color.r, stack->bg.color.g, stack->bg.color.b,
                                stack->bg.color_index );
               else
                    D_DEBUG_AT( SaWMan_Draw, "  -> fill %02x %02x %02x %02x [%d] -> %02x %02x %02x %02x\n",
                                stack->bg.color.a, stack->bg.color.r, stack->bg.color.g, stack->bg.color.b,
                                stack->bg.color_index,
                                state->destination->palette->entries[stack->bg.color_index].a,
                                state->destination->palette->entries[stack->bg.color_index].r,
                                state->destination->palette->entries[stack->bg.color_index].g,
                                state->destination->palette->entries[stack->bg.color_index].b );

               /* Simply fill the background. */
               dfb_gfxcard_fillrectangles( &dst, 1, state );

               break;
          }

          case DLBM_IMAGE: {
               CoreSurface *bg = stack->bg.image;

               /* Set blitting source. */
               state->source    = bg;
               state->modified |= SMF_SOURCE;

               /* Set blitting flags. */
               dfb_state_set_blitting_flags( state, DSBLIT_NOFX );

               /* Check the size of the background image. */
               if (bg->config.size.w == stack->width && bg->config.size.h == stack->height) {
                    /* Simple blit for 100% fitting background image. */
                    dfb_gfxcard_blit( &dst, dst.x, dst.y, state );
               }
               else {
                    DFBRegion    old_clip = state->clip;
                    DFBRectangle src      = { 0, 0, bg->config.size.w, bg->config.size.h };

                    /* Change clipping region. */
                    dfb_state_set_clip( state, region );

                    /*
                     * Scale image to fill the whole screen
                     * clipped to the region being updated.
                     */
                    dst.x = 0;
                    dst.y = 0;
                    dst.w = stack->width;
                    dst.h = stack->height;

                    /* Stretch blit for non fitting background images. */
                    dfb_gfxcard_stretchblit( &src, &dst, state );

                    /* Restore clipping region. */
                    dfb_state_set_clip( state, &old_clip );
               }

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_TILE: {
               CoreSurface  *bg       = stack->bg.image;
               DFBRegion     old_clip = state->clip;
               DFBRectangle  src      = { 0, 0, bg->config.size.w, bg->config.size.h };

               /* Set blitting source. */
               state->source    = bg;
               state->modified |= SMF_SOURCE;

               /* Set blitting flags. */
               dfb_state_set_blitting_flags( state, DSBLIT_NOFX );

               /* Change clipping region. */
               dfb_state_set_clip( state, region );

               /* Tiled blit (aligned). */
               dfb_gfxcard_tileblit( &src,
                                     (region->x1 / src.w) * src.w,
                                     (region->y1 / src.h) * src.h,
                                     (region->x2 / src.w + 1) * src.w,
                                     (region->y2 / src.h + 1) * src.h,
                                     state );

               /* Restore clipping region. */
               dfb_state_set_clip( state, &old_clip );

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_DONTCARE:
               break;

          default:
               D_BUG( "unknown background mode" );
               break;
     }
}


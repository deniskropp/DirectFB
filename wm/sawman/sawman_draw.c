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
#include <sawman_manager.h>

#include "stretch_algos.h"

D_DEBUG_DOMAIN( SaWMan_Draw, "SaWMan/Draw", "SaWMan window manager drawing" );

/**********************************************************************************************************************/

static DFBResult
smooth_stretchblit( CardState          *state,
                    const DFBRectangle *sr,
                    const DFBRectangle *dr,
                    const StretchAlgo  *algo )
{
     DFBResult              ret;
     void                  *src;
     void                  *dst;
     CoreSurfaceBuffer     *source;
     CoreSurfaceBuffer     *destination;
     DFBRegion              clip;
     CoreSurfaceBufferLock  slock;
     CoreSurfaceBufferLock  dlock;

     D_ASSERT( state != NULL );
     D_ASSERT( state->source != NULL );
     D_ASSERT( state->destination != NULL );

     source      = dfb_surface_get_buffer( state->source, CSBR_FRONT );
     destination = dfb_surface_get_buffer( state->destination, CSBR_BACK );
     clip        = state->clip;

     switch (destination->format) {
          case DSPF_RGB16:
               switch (source->format) {
                    case DSPF_RGB16:
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
                              if (!algo->func_rgb16_keyed)
                                   return DFB_UNSUPPORTED;
                         }
                         else if (!algo->func_rgb16)
                              return DFB_UNSUPPORTED;

                         break;

                    case DSPF_LUT8:
                         D_ASSERT( state->source->palette != NULL );

                         if (!algo->func_rgb16_indexed)
                              return DFB_UNSUPPORTED;

                         break;

                    case DSPF_RGB32:
                         if (!algo->func_rgb16_from32)
                              return DFB_UNSUPPORTED;

                         break;

                    default:
                         return DFB_UNSUPPORTED;
               }
               break;

          case DSPF_ARGB4444:
               switch (source->format) {
                    case DSPF_ARGB4444:
                         if (!algo->func_argb4444)
                              return DFB_UNSUPPORTED;

                         break;

                    default:
                         return DFB_UNSUPPORTED;
               }
               break;

          default:
               return DFB_UNSUPPORTED;
     }


     ret = dfb_surface_buffer_lock( source, CSAF_CPU_READ, &slock );
     if (ret) {
          D_DERROR( ret, "SaWMan/Draw: %s(): Could not lock source buffer!\n", __FUNCTION__ );
          return ret;
     }

     ret = dfb_surface_buffer_lock( destination, CSAF_CPU_WRITE, &dlock );
     if (ret) {
          D_DERROR( ret, "SaWMan/Draw: %s(): Could not lock source buffer!\n", __FUNCTION__ );
          dfb_surface_buffer_unlock( &slock );
          return ret;
     }

     src = slock.addr + DFB_BYTES_PER_LINE( source->format, sr->x ) + sr->y * slock.pitch;
     dst = dlock.addr + DFB_BYTES_PER_LINE( destination->format, dr->x ) + dr->y * dlock.pitch;

     dfb_region_translate( &clip, - dr->x, - dr->y );

     switch (destination->format) {
          case DSPF_RGB16:
               switch (source->format) {
                    case DSPF_RGB16:
                         if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                              algo->func_rgb16_keyed( dst, dlock.pitch, src, slock.pitch, sr->w, sr->h, dr->w, dr->h, &clip,
                                                      state->dst_colorkey, state->src_colorkey );
                         else
                              algo->func_rgb16( dst, dlock.pitch, src, slock.pitch, sr->w, sr->h, dr->w, dr->h, &clip, state->dst_colorkey );
                         break;

                    case DSPF_LUT8:
                         D_ASSERT( state->source->palette != NULL );
                         algo->func_rgb16_indexed( dst, dlock.pitch, src, slock.pitch, sr->w, sr->h, dr->w, dr->h, &clip, state->dst_colorkey,
                                                   state->source->palette->entries );
                         break;

                    case DSPF_RGB32:
                         algo->func_rgb16_from32( dst, dlock.pitch, src, slock.pitch, sr->w, sr->h, dr->w, dr->h, &clip, state->dst_colorkey );
                         break;

                    default:
                         D_BUG( "unsupported source format %s", dfb_pixelformat_name(source->format) );
               }
               break;

          case DSPF_ARGB4444:
               switch (source->format) {
                    case DSPF_ARGB4444:
                         algo->func_argb4444( dst, dlock.pitch, src, slock.pitch, sr->w, sr->h, dr->w, dr->h, &clip, state->dst_colorkey );
                         break;

                    default:
                         D_BUG( "unsupported source format %s", dfb_pixelformat_name(source->format) );
               }
               break;

          default:
               D_BUG( "unsupported destination format %s", dfb_pixelformat_name(destination->format) );
     }

     dfb_surface_buffer_unlock( &dlock );
     dfb_surface_buffer_unlock( &slock );

     return DFB_OK;
}

/**********************************************************************************************************************/

void
sawman_draw_cursor( CoreWindowStack *stack, CardState *state, DFBRegion *region )
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
     src.x = region->x1 - stack->cursor.x + stack->cursor.hot.x;
     src.y = region->y1 - stack->cursor.y + stack->cursor.hot.y;
     src.w = region->x2 - region->x1 + 1;
     src.h = region->y2 - region->y1 + 1;

     /* Initialize source clipping rectangle */
     clip.x = clip.y = 0;
     clip.w = stack->cursor.surface->config.size.w;
     clip.h = stack->cursor.surface->config.size.h;

     /* Intersect rectangles */
     if (!dfb_rectangle_intersect( &src, &clip ))
          return;

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

     /* Blit from the window to the region being updated. */
     dfb_gfxcard_blit( &src, region->x1, region->y1, state );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

static void
draw_border( SaWManWindow    *sawwin,
             CardState       *state,
             const DFBRegion *region,
             int              thickness )
{
     int                     i;
     DFBRegion               clip;
     DFBRectangle            rects[thickness];
     CoreWindow             *window;
     const SaWManBorderInit *border;
     const DFBColor         *colors;
     const int              *indices;
     unsigned int            num_colors;
     unsigned int            num_indices;

     window = sawwin->window;
     D_ASSERT( window != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %p, %d,%d-%dx%d, %d )\n", __FUNCTION__,
                 sawwin, state, DFB_RECTANGLE_VALS_FROM_REGION( region ), thickness );

     if (thickness > window->config.bounds.w / 2)
          thickness = window->config.bounds.w / 2;

     if (thickness > window->config.bounds.h / 2)
          thickness = window->config.bounds.h / 2;

     /* Check thickness. */
     if (thickness < 1)
          return;

     /* Initialize border rectangles. */
     rects[0] = window->config.bounds;

     for (i=1; i<thickness; i++) {
          rects[i].x = rects[i-1].x + 1;
          rects[i].y = rects[i-1].y + 1;
          rects[i].w = rects[i-1].w - 2;
          rects[i].h = rects[i-1].h - 2;
     }

     /* Save clipping region. */
     clip = state->clip;

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
     dfb_state_set_clip( state, &clip );
}

static void
draw_window( SaWManWindow *sawwin,
             CardState    *state,
             DFBRegion    *region,
             bool          alpha_channel )
{
     SaWMan                  *sawman;
     CoreWindow              *window;
     DFBSurfaceBlittingFlags  flags = DSBLIT_NOFX;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     sawman = sawwin->sawman;
     window = sawwin->window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( window != NULL );
     D_ASSERT( window->surface != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin, DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     /* Use per pixel alpha blending. */
     if (alpha_channel && (window->config.options & DWOP_ALPHACHANNEL))
          flags |= DSBLIT_BLEND_ALPHACHANNEL;

     /* Use global alpha blending. */
     if (window->config.opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          /* Set opacity as blending factor. */
          if (state->color.a != window->config.opacity) {
               state->color.a   = window->config.opacity;
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

     /* Set blitting flags. */
     dfb_state_set_blitting_flags( state, flags );

     /* Set blitting source. */
     state->source    = window->surface;
     state->modified |= SMF_SOURCE;

     DFBRegion clip = state->clip;
     DFBRegion new_clip = *region;

     dfb_region_rectangle_intersect( &new_clip, &sawwin->dst );

     /* Change clipping region. */
     dfb_state_set_clip( state, &new_clip );

     if (!sawman->fast_mode && (window->config.options & DWOP_SCALE)
         /*(window_config->bounds.w != window->surface->config.size.w  ||
          window_config->bounds.h != window->surface->config.size.h || sawman->color_keyed)*/)
     {
          DFBResult    ret = DFB_UNSUPPORTED;
          DFBRectangle dst = sawwin->dst;
          DFBRectangle src = sawwin->src;

          /* Scale window to the screen clipped by the region being updated. */
          if (sawman->scaling_mode == SWMSM_SMOOTH_SW) {
               if (dst.w == src.w && dst.h == src.h)
                    ret = smooth_stretchblit( state, &src, &dst, &wm_stretch_simple );
               else if (dst.w < window->surface->config.size.w && dst.h < window->surface->config.size.h)
                    ret = smooth_stretchblit( state, &src, &dst, &wm_stretch_down );
               else {
                    src.w--;
                    src.h--;

                    ret = smooth_stretchblit( state, &src, &dst, &wm_stretch_up );
               }
          }

          /* Standard scaling selected or fallback for smooth scaling required. */
          if (ret)
               dfb_gfxcard_stretchblit( &src, &dst, state );
     }
     else {
          DFBRectangle src = sawwin->src;

          /* Blit from the window to the region being updated. */
          dfb_gfxcard_blit( &src, sawwin->dst.x, sawwin->dst.y, state );
     }

     /* Restore clipping region. */
     dfb_state_set_clip( state, &clip );
}

void
sawman_draw_window( SaWManWindow *sawwin,
                    CardState    *state,
                    DFBRegion    *pregion,
                    bool          alpha_channel )
{
     CoreWindow      *window;
     CoreWindowStack *stack;
     DFBRegion        xregion = *pregion;
     DFBRegion       *region  = &xregion;
     int              border;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( region );

     window = sawwin->window;
     stack  = sawwin->stack;

     D_ASSERT( window != NULL );
     D_ASSERT( stack != NULL );

     D_DEBUG_AT( SaWMan_Draw, "%s( %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 sawwin, DFB_RECTANGLE_VALS_FROM_REGION( pregion ) );

     border = sawman_window_border( sawwin );

     if (window->surface &&
         dfb_region_intersect( region,
                               window->config.bounds.x + border,
                               window->config.bounds.y + border,
                               window->config.bounds.x + window->config.bounds.w - border - 1,
                               window->config.bounds.y + window->config.bounds.h - border - 1 ) &&
         dfb_region_rectangle_intersect( region, &sawwin->dst )
         )
          draw_window( sawwin, state, region, alpha_channel );


     if (border)
          draw_border( sawwin, state, pregion, border );

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
                    DFBRegion    clip = state->clip;
                    DFBRectangle src  = { 0, 0, bg->config.size.w, bg->config.size.h };

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
                    dfb_state_set_clip( state, &clip );
               }

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_TILE: {
               CoreSurface  *bg   = stack->bg.image;
               DFBRegion     clip = state->clip;
               DFBRectangle  src  = { 0, 0, bg->config.size.w, bg->config.size.h };

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
               dfb_state_set_clip( state, &clip );

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


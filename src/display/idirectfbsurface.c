/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
#include <string.h>

#include <math.h>

#include <malloc.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/fonts.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <media/idirectfbfont.h>

#include <display/idirectfbsurface.h>
#include <display/idirectfbpalette.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/convert.h>
#include <gfx/util.h>


static ReactionResult
IDirectFBSurface_listener( const void *msg_data, void *ctx );


void
IDirectFBSurface_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data = (IDirectFBSurface_data*)thiz->priv;

     if (data->surface)
          dfb_surface_detach( data->surface,
                              IDirectFBSurface_listener, thiz );

     dfb_state_set_destination( &data->state, NULL );
     dfb_state_set_source( &data->state, NULL );

     dfb_state_destroy( &data->state );

     dfb_surface_unref( data->surface );

     if (data->font)
          data->font->Release (data->font);

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBSurface_AddRef( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Release( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (--data->ref == 0)
          IDirectFBSurface_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IDirectFBSurface_GetPixelFormat( IDirectFBSurface      *thiz,
                                 DFBSurfacePixelFormat *format )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!format)
          return DFB_INVARG;

     *format = data->surface->format;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetAccelerationMask( IDirectFBSurface    *thiz,
                                      IDirectFBSurface    *source,
                                      DFBAccelerationMask *mask )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!mask)
          return DFB_INVARG;

     if (source) {
          IDirectFBSurface_data *src_data = (IDirectFBSurface_data*)source->priv;

          dfb_state_set_source( &data->state, src_data->surface );
     }

     dfb_gfxcard_state_check( &data->state, DFXL_FILLRECTANGLE );
     dfb_gfxcard_state_check( &data->state, DFXL_DRAWRECTANGLE );
     dfb_gfxcard_state_check( &data->state, DFXL_DRAWLINE );
     dfb_gfxcard_state_check( &data->state, DFXL_FILLTRIANGLE );

     if (source) {
          dfb_gfxcard_state_check( &data->state, DFXL_BLIT );
          dfb_gfxcard_state_check( &data->state, DFXL_STRETCHBLIT );
     }

     *mask = data->state.accel;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSize( IDirectFBSurface *thiz,
                          unsigned int     *width,
                          unsigned int     *height )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->area.wanted.w;

     if (height)
          *height = data->area.wanted.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetVisibleRectangle( IDirectFBSurface *thiz,
                                      DFBRectangle     *rect )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!rect)
          return DFB_INVARG;

     rect->x = data->area.current.x - data->area.wanted.x;
     rect->y = data->area.current.y - data->area.wanted.y;
     rect->w = data->area.current.w;
     rect->h = data->area.current.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetCapabilities( IDirectFBSurface       *thiz,
                                  DFBSurfaceCapabilities *caps )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!caps)
          return DFB_INVARG;

     *caps = data->caps;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPalette( IDirectFBSurface  *thiz,
                             IDirectFBPalette **interface )
{
     DFBResult         ret;
     CoreSurface      *surface;
     IDirectFBPalette *palette;

     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!surface->palette)
          return DFB_UNSUPPORTED;

     if (!interface)
          return DFB_INVARG;

     DFB_ALLOCATE_INTERFACE( palette, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( palette, surface->palette );
     if (ret)
          return ret;

     *interface = palette;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Lock( IDirectFBSurface *thiz,
                       DFBSurfaceLockFlags flags,
                       void **ptr, int *pitch )
{
     int front;
     DFBResult ret;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!flags || !ptr || !pitch)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     front = (flags & DSLF_WRITE) ? 0 : 1;

     ret = dfb_surface_soft_lock( data->surface, flags, ptr, pitch, front );
     if (ret)
          return ret;

     (__u8*)(*ptr) += data->area.current.y * (*pitch) +
                      data->area.current.x *
                      DFB_BYTES_PER_PIXEL(data->surface->format);

     data->locked = front + 1;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Unlock( IDirectFBSurface *thiz )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->locked)
          dfb_surface_unlock( data->surface, data->locked - 1 );

     data->locked = 0;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Flip( IDirectFBSurface    *thiz,
                       const DFBRegion     *region,
                       DFBSurfaceFlipFlags  flags )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked)
          return DFB_LOCKED;

     if (!(data->caps & DSCAPS_FLIPPING))
          return DFB_UNSUPPORTED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (flags & DSFLIP_BLIT  ||  region  ||  data->caps & DSCAPS_SUBSURFACE) {
          if (region) {
               DFBRegion    reg  = *region;
               DFBRectangle rect = data->area.current;

               reg.x1 += data->area.wanted.x;
               reg.x2 += data->area.wanted.x;
               reg.y1 += data->area.wanted.y;
               reg.y2 += data->area.wanted.y;

               if (dfb_rectangle_intersect_by_unsafe_region( &rect, &reg ))
                    dfb_back_to_front_copy( data->surface, &rect );
          }
          else {
               DFBRectangle rect = data->area.current;

               dfb_back_to_front_copy( data->surface, &rect );
          }
     }
     else
          dfb_surface_flip_buffers( data->surface );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Clear( IDirectFBSurface *thiz,
                        __u8 r, __u8 g, __u8 b, __u8 a )
{
     DFBColor                old_color;
     DFBSurfaceDrawingFlags  old_flags;
     DFBRectangle            rect;
     CoreSurface            *surface;
     
     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     /* save current color and drawing flags */
     old_color = data->state.color;
     old_flags = data->state.drawingflags;

     /* set drawing flags */
     if (old_flags != DSDRAW_NOFX) {
          data->state.drawingflags  = DSDRAW_NOFX;
          data->state.modified     |= SMF_DRAWING_FLAGS;
     }
     
     /* set color */
     data->state.color.r = r;
     data->state.color.g = g;
     data->state.color.b = b;
     data->state.color.a = a;
     
     if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          data->state.color_index = dfb_palette_search( surface->palette,
                                                        r, g, b, a );
     
     data->state.modified |= SMF_COLOR;
     
     /* fill the visible rectangle */
     rect = data->area.current;
     dfb_gfxcard_fillrectangle( &rect, &data->state );

     /* restore drawing flags */
     if (old_flags != DSDRAW_NOFX) {
          data->state.drawingflags  = old_flags;
          data->state.modified     |= SMF_DRAWING_FLAGS;
     }

     /* restore color */
     data->state.color     = old_color;
     data->state.modified |= SMF_COLOR;
     
     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetClip( IDirectFBSurface *thiz, const DFBRegion *clip )
{
     DFBRegion newclip;

     INTERFACE_GET_DATA(IDirectFBSurface)


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (clip) {
          newclip = *clip;

          newclip.x1 += data->area.wanted.x;
          newclip.x2 += data->area.wanted.x;
          newclip.y1 += data->area.wanted.y;
          newclip.y2 += data->area.wanted.y;

          if (!dfb_unsafe_region_rectangle_intersect( &newclip,
                                                      &data->area.wanted ))
               return DFB_INVARG;

          data->clip_set = 1;
          data->clip_wanted = newclip;

          if (!dfb_region_rectangle_intersect( &newclip, &data->area.current ))
               return DFB_INVAREA;
     }
     else {
          newclip.x1 = data->area.current.x;
          newclip.y1 = data->area.current.y;
          newclip.x2 = data->area.current.x + data->area.current.w - 1;
          newclip.y2 = data->area.current.y + data->area.current.h - 1;

          data->clip_set = 0;
     }

     data->state.clip      = newclip;
     data->state.modified |= SMF_CLIP;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColor( IDirectFBSurface *thiz,
                           __u8 r, __u8 g, __u8 b, __u8 a )
{
     CoreSurface *surface;
     
     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     data->state.color.a = a;
     data->state.color.r = r;
     data->state.color.g = g;
     data->state.color.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          data->state.color_index = dfb_palette_search( surface->palette,
                                                        r, g, b, a );

     data->state.modified |= SMF_COLOR;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColorIndex( IDirectFBSurface *thiz,
                                unsigned int      index )
{
     CoreSurface *surface;
     CorePalette *palette;

     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     if (! DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          return DFB_UNSUPPORTED;

     palette = surface->palette;
     if (!palette)
          return DFB_UNSUPPORTED;

     if (index > palette->num_entries)
          return DFB_INVARG;

     data->state.color        = palette->entries[index];
     data->state.color_index  = index;
     data->state.modified    |= SMF_COLOR;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  src )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->state.src_blend != src) {
          switch (src) {
               case DSBF_ZERO:
               case DSBF_ONE:
               case DSBF_SRCCOLOR:
               case DSBF_INVSRCCOLOR:
               case DSBF_SRCALPHA:
               case DSBF_INVSRCALPHA:
               case DSBF_DESTALPHA:
               case DSBF_INVDESTALPHA:
               case DSBF_DESTCOLOR:
               case DSBF_INVDESTCOLOR:
               case DSBF_SRCALPHASAT:
                    data->state.src_blend = src;
                    data->state.modified |= SMF_SRC_BLEND;
                    return DFB_OK;
          }

          return DFB_INVARG;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  dst )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->state.dst_blend != dst) {
          switch (dst) {
               case DSBF_ZERO:
               case DSBF_ONE:
               case DSBF_SRCCOLOR:
               case DSBF_INVSRCCOLOR:
               case DSBF_SRCALPHA:
               case DSBF_INVSRCALPHA:
               case DSBF_DESTALPHA:
               case DSBF_INVDESTALPHA:
               case DSBF_DESTCOLOR:
               case DSBF_INVDESTCOLOR:
               case DSBF_SRCALPHASAT:
                    data->state.dst_blend = dst;
                    data->state.modified |= SMF_SRC_BLEND;
                    return DFB_OK;
          }

          return DFB_INVARG;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetPorterDuff( IDirectFBSurface         *thiz,
                                DFBSurfacePorterDuffRule  rule )
{
     DFBSurfaceBlendFunction src;
     DFBSurfaceBlendFunction dst;

     INTERFACE_GET_DATA(IDirectFBSurface)


     switch (rule) {
          case DSPD_NONE:
               src = DSBF_SRCALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_CLEAR:
               src = DSBF_ZERO;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC:
               src = DSBF_ONE;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC_OVER:
               src = DSBF_ONE;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST_OVER:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ONE;
               break;
          case DSPD_SRC_IN:
               src = DSBF_DESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_IN:
               src = DSBF_ZERO;
               dst = DSBF_SRCALPHA;
               break;
          case DSPD_SRC_OUT:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_OUT:
               src = DSBF_ZERO;
               dst = DSBF_INVSRCALPHA;
               break;
          default:
               return DFB_INVARG;
     }

     if (data->state.src_blend != src) {
          data->state.src_blend = src;
          data->state.modified |= SMF_SRC_BLEND;
     }

     if (data->state.dst_blend != dst) {
          data->state.dst_blend = dst;
          data->state.modified |= SMF_DST_BLEND;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKey( IDirectFBSurface *thiz,
                                 __u8              r,
                                 __u8              g,
                                 __u8              b )
{
     CoreSurface *surface;

     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     data->src_key.r = r;
     data->src_key.g = g;
     data->src_key.b = b;
     
     if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          data->src_key.value = dfb_palette_search( surface->palette,
                                                    r, g, b, 0x80 );
     else
          data->src_key.value = color_to_pixel( surface->format, r, g, b );

     /* The new key won't be applied to this surface's state.
        The key will be taken by the destination surface to apply it
        to its state when source color keying is used. */

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKey( IDirectFBSurface *thiz,
                                 __u8              r,
                                 __u8              g,
                                 __u8              b )
{
     CoreSurface *surface;

     INTERFACE_GET_DATA(IDirectFBSurface)

     surface = data->surface;
     if (!surface)
          return DFB_DESTROYED;

     data->dst_key.r = r;
     data->dst_key.g = g;
     data->dst_key.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
          data->dst_key.value = dfb_palette_search( surface->palette,
                                                    r, g, b, 0x80 );
     else
          data->dst_key.value = color_to_pixel( surface->format, r, g, b );

     if (data->state.dst_colorkey != data->dst_key.value) {
          data->state.dst_colorkey = data->dst_key.value;
          data->state.modified |= SMF_DST_COLORKEY;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetFont( IDirectFBSurface *thiz,
                          IDirectFBFont    *font )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->locked)
          return DFB_LOCKED;

     if (!font)
          return DFB_INVARG;

     if (data->font)
          data->font->Release (data->font);

     font->AddRef (font);
     data->font = font;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetFont( IDirectFBSurface  *thiz,
                          IDirectFBFont    **font )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->font)
          return DFB_MISSINGFONT;

     if (!font)
          return DFB_INVARG;

     data->font->AddRef (data->font);
     *font = data->font;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDrawingFlags( IDirectFBSurface       *thiz,
                                  DFBSurfaceDrawingFlags  flags )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->state.drawingflags != flags) {
          data->state.drawingflags = flags;
          data->state.modified |= SMF_DRAWING_FLAGS;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillRectangle( IDirectFBSurface *thiz,
                                int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     dfb_gfxcard_fillrectangle( &rect, &data->state );

     return DFB_OK;
}


static DFBResult
IDirectFBSurface_DrawLine( IDirectFBSurface *thiz,
                           int x1, int y1, int x2, int y2 )
{
     DFBRegion line = { x1, y1, x2, y2 };

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     line.x1 += data->area.wanted.x;
     line.x2 += data->area.wanted.x;
     line.y1 += data->area.wanted.y;
     line.y2 += data->area.wanted.y;

     dfb_gfxcard_drawlines( &line, 1, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawLines( IDirectFBSurface *thiz,
                            const DFBRegion  *lines,
                            unsigned int      num_lines )
{
     DFBRegion *local_lines = alloca(sizeof(DFBRegion) * num_lines);

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!lines || !num_lines)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int i;

          for (i=0; i<num_lines; i++) {
               local_lines[i].x1 = lines[i].x1 + data->area.wanted.x;
               local_lines[i].x2 = lines[i].x2 + data->area.wanted.x;
               local_lines[i].y1 = lines[i].y1 + data->area.wanted.y;
               local_lines[i].y2 = lines[i].y2 + data->area.wanted.y;
          }
     }
     else
          /* clipping may modify lines, so we copy them */
          memcpy( local_lines, lines, sizeof(DFBRegion) * num_lines );

     dfb_gfxcard_drawlines( local_lines, num_lines, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawRectangle( IDirectFBSurface *thiz,
                                int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     dfb_gfxcard_drawrectangle( &rect, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTriangle( IDirectFBSurface *thiz,
                               int x1, int y1,
                               int x2, int y2,
                               int x3, int y3 )
{
     DFBTriangle tri = { x1, y1, x2, y2, x3, y3 };

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     tri.x1 += data->area.wanted.x;
     tri.y1 += data->area.wanted.y;
     tri.x2 += data->area.wanted.x;
     tri.y2 += data->area.wanted.y;
     tri.x3 += data->area.wanted.x;
     tri.y3 += data->area.wanted.y;

     dfb_gfxcard_filltriangle( &tri, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetBlittingFlags( IDirectFBSurface        *thiz,
                                   DFBSurfaceBlittingFlags  flags )
{
     INTERFACE_GET_DATA(IDirectFBSurface)

     if (data->state.blittingflags != flags) {
          data->state.blittingflags = flags;
          data->state.modified |= SMF_BLITTING_FLAGS;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Blit( IDirectFBSurface   *thiz,
                       IDirectFBSurface   *source,
                       const DFBRectangle *sr,
                       int dx, int dy )
{
     DFBRectangle srect;
     IDirectFBSurface_data *src_data;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;


     if (sr) {
          if (sr->w < 1  ||  sr->h < 1)
               return DFB_OK;

          srect = *sr;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (sr->x + src_data->area.wanted.x);
          dy += srect.y - (sr->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     dfb_state_set_source( &data->state, src_data->surface );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY) {
          if (data->state.src_colorkey != src_data->src_key.value) {
               data->state.src_colorkey = src_data->src_key.value;
               data->state.modified |= SMF_SRC_COLORKEY;
          }
     }

     dfb_gfxcard_blit( &srect,
                       data->area.wanted.x + dx,
                       data->area.wanted.y + dy, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_TileBlit( IDirectFBSurface   *thiz,
                           IDirectFBSurface   *source,
                           const DFBRectangle *sr,
                           int dx, int dy )
{
     DFBRectangle srect;
     IDirectFBSurface_data *src_data;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;


     if (sr) {
          if (sr->w < 1  ||  sr->h < 1)
               return DFB_OK;

          srect = *sr;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (sr->x + src_data->area.wanted.x);
          dy += srect.y - (sr->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     dfb_state_set_source( &data->state, src_data->surface );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY) {
          if (data->state.src_colorkey != src_data->src_key.value) {
               data->state.src_colorkey = src_data->src_key.value;
               data->state.modified |= SMF_SRC_COLORKEY;
          }
     }

     dx %= srect.w;
     if (dx > 0)
       dx -= srect.w;

     dy %= srect.h;
     if (dy > 0)
       dy -= srect.h;

     dfb_gfxcard_tileblit( &srect,
                           data->area.wanted.x + dx,
                           data->area.wanted.y + dy,
                           data->area.wanted.x + data->area.wanted.w,
                           data->area.wanted.y + data->area.wanted.h, 
                           &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_StretchBlit( IDirectFBSurface   *thiz,
                              IDirectFBSurface   *source,
                              const DFBRectangle *source_rect,
                              const DFBRectangle *destination_rect )
{
     DFBRectangle srect, drect;
     IDirectFBSurface_data *src_data;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;


     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;


     src_data = (IDirectFBSurface_data*)source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;


     /* do destination rectangle */
     if (destination_rect) {
          if (destination_rect->w < 1  ||  destination_rect->h < 1)
               return DFB_INVARG;

          drect = *destination_rect;

          drect.x += data->area.wanted.x;
          drect.y += data->area.wanted.y;
     }
     else
          drect = data->area.wanted;

     /* do source rectangle */
     if (source_rect) {
          if (source_rect->w < 1  ||  source_rect->h < 1)
               return DFB_INVARG;

          srect = *source_rect;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;
     }
     else
          srect = src_data->area.wanted;


     /* clipping of the source rectangle must be applied to the destination */
     {
          DFBRectangle orig_src = srect;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          if (srect.x != orig_src.x)
               drect.x += (int)( (srect.x - orig_src.x) *
                                 (drect.w / (float)orig_src.w) + 0.5f);

          if (srect.y != orig_src.y)
               drect.y += (int)( (srect.y - orig_src.y) *
                                 (drect.h / (float)orig_src.h) + 0.5f);

          if (srect.w != orig_src.w)
               drect.w = DFB_ICEIL(drect.w * (srect.w / (float)orig_src.w));
          if (srect.h != orig_src.h)
               drect.h = DFB_ICEIL(drect.h * (srect.h / (float)orig_src.h));
     }

     dfb_state_set_source( &data->state, src_data->surface );

     /* fetch the source color key from the source if necessary */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY) {
          if (data->state.src_colorkey != src_data->src_key.value) {
               data->state.src_colorkey = src_data->src_key.value;
               data->state.modified |= SMF_SRC_COLORKEY;
          }
     }

     dfb_gfxcard_stretchblit( &srect, &drect, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawString( IDirectFBSurface *thiz,
                             const char *text, int bytes,
                             int x, int y,
                             DFBSurfaceTextFlags flags )
{
     IDirectFBFont_data *font_data;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!text)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;


     if (bytes < 0)
          bytes = strlen (text);

     if (bytes == 0)
          return DFB_OK;

     if (!(flags & DSTF_TOP)) {
          int offset = 0;

          data->font->GetAscender (data->font, &offset);
          y -= offset;

          if ((flags & DSTF_BOTTOM)) {
               offset = 0;
               data->font->GetDescender (data->font, &offset);
               y += offset;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          int width = 0;

          if (data->font->GetStringWidth (data->font, text, bytes, &width) == DFB_OK) {
               if (flags & DSTF_RIGHT) {
                    x -= width;
               }
               else if (flags & DSTF_CENTER) {
                    x -= width >> 1;
               }
          }
     }

     font_data = (IDirectFBFont_data *)data->font->priv;

     dfb_gfxcard_drawstring( text, bytes,
                             data->area.wanted.x + x, data->area.wanted.y + y,
                             font_data->font, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawGlyph( IDirectFBSurface *thiz,
                            unsigned int index, int x, int y,
                            DFBSurfaceTextFlags flags )
{
     IDirectFBFont_data *font_data;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!index)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;

     if (!(flags & DSTF_TOP)) {
          int offset = 0;

          data->font->GetAscender (data->font, &offset);
          y -= offset;

          if ((flags & DSTF_BOTTOM)) {
               offset = 0;
               data->font->GetDescender (data->font, &offset);
               y += offset;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          int advance;

          if (data->font->GetGlyphExtents (data->font,
                                           index, NULL, &advance) == DFB_OK) {
               if (flags & DSTF_RIGHT) {
                    x -= advance;
               }
               else if (flags & DSTF_CENTER) {
                    x -= advance >> 1;
               }
          }
     }

     font_data = (IDirectFBFont_data *)data->font->priv;

     dfb_gfxcard_drawglyph( index,
                            data->area.wanted.x + x, data->area.wanted.y + y,
                            font_data->font, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSubSurface( IDirectFBSurface    *thiz,
                                const DFBRectangle  *rect,
                                IDirectFBSurface   **surface )
{
     DFBRectangle wanted, granted;

     INTERFACE_GET_DATA(IDirectFBSurface)

     /* Check arguments */
     if (!data->surface)
          return DFB_DESTROYED;

     if (!surface)
          return DFB_INVARG;

     /* Compute wanted rectangle */
     if (rect) {
          wanted = *rect;

          wanted.x += data->area.wanted.x;
          wanted.y += data->area.wanted.y;

          if (wanted.w <= 0 || wanted.h <= 0) {
               wanted.w = 0;
               wanted.h = 0;
          }
     }
     else
          wanted = data->area.wanted;

     /* Compute granted rectangle */
     granted = wanted;

     dfb_rectangle_intersect( &granted, &data->area.granted );

     /* Allocate and construct */
     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Construct( *surface, &wanted, &granted,
                                        data->surface,
                                        data->caps | DSCAPS_SUBSURFACE );
}

static DFBResult
IDirectFBSurface_GetGL( IDirectFBSurface   *thiz,
                        IDirectFBGL       **interface )
{
     DFBResult ret;
     DFBInterfaceFuncs *funcs = NULL;

     INTERFACE_GET_DATA(IDirectFBSurface)

     if (!data->surface)
          return DFB_DESTROYED;

     if (!interface)
          return DFB_INVARG;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;


     ret = DFBGetInterface( &funcs, "IDirectFBGL", NULL, NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( (void**)interface );
     if (ret)
          return ret;

     ret = funcs->Construct( *interface, thiz );
     if (ret)
          *interface = NULL;

     return ret;
}

/******/

DFBResult IDirectFBSurface_Construct( IDirectFBSurface       *thiz,
                                      DFBRectangle           *wanted,
                                      DFBRectangle           *granted,
                                      CoreSurface            *surface,
                                      DFBSurfaceCapabilities  caps )
{
     DFBRectangle rect = { 0, 0, surface->width, surface->height };

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBSurface)

     data->ref = 1;
     data->caps = caps;

     if (dfb_surface_ref( surface ) != FUSION_SUCCESS) {
          DFB_DEALLOCATE_INTERFACE(thiz);
          return DFB_FAILURE;
     }

     /* The area that was requested */
     if (wanted)
          data->area.wanted = *wanted;
     else
          data->area.wanted = rect;

     /* The area that will never be exceeded */
     if (granted)
          data->area.granted = *granted;
     else
          data->area.granted = data->area.wanted;

     /* The currently accessible rectangle */
     data->area.current = data->area.granted;
     dfb_rectangle_intersect( &data->area.current, &rect );

     data->surface = surface;

     dfb_state_init( &data->state );
     dfb_state_set_destination( &data->state, surface );

     dfb_surface_attach( surface, IDirectFBSurface_listener, thiz );

     data->state.clip.x1 = data->area.current.x;
     data->state.clip.y1 = data->area.current.y;
     data->state.clip.x2 = data->area.current.x + data->area.current.w - 1;
     data->state.clip.y2 = data->area.current.y + data->area.current.h - 1;
     data->state.dst_blend = DSBF_INVSRCALPHA;
     data->state.src_blend = DSBF_SRCALPHA;
     data->state.modified = SMF_ALL;

     thiz->AddRef = IDirectFBSurface_AddRef;
     thiz->Release = IDirectFBSurface_Release;

     thiz->GetCapabilities = IDirectFBSurface_GetCapabilities;
     thiz->GetSize = IDirectFBSurface_GetSize;
     thiz->GetVisibleRectangle = IDirectFBSurface_GetVisibleRectangle;
     thiz->GetPixelFormat = IDirectFBSurface_GetPixelFormat;
     thiz->GetAccelerationMask = IDirectFBSurface_GetAccelerationMask;

     thiz->GetPalette = IDirectFBSurface_GetPalette;
     
     thiz->Lock = IDirectFBSurface_Lock;
     thiz->Unlock = IDirectFBSurface_Unlock;
     thiz->Flip = IDirectFBSurface_Flip;
     thiz->Clear = IDirectFBSurface_Clear;

     thiz->SetClip = IDirectFBSurface_SetClip;
     thiz->SetColor = IDirectFBSurface_SetColor;
     thiz->SetColorIndex = IDirectFBSurface_SetColorIndex;
     thiz->SetSrcBlendFunction = IDirectFBSurface_SetSrcBlendFunction;
     thiz->SetDstBlendFunction = IDirectFBSurface_SetDstBlendFunction;
     thiz->SetPorterDuff = IDirectFBSurface_SetPorterDuff;
     thiz->SetSrcColorKey = IDirectFBSurface_SetSrcColorKey;
     thiz->SetDstColorKey = IDirectFBSurface_SetDstColorKey;

     thiz->SetBlittingFlags = IDirectFBSurface_SetBlittingFlags;
     thiz->Blit = IDirectFBSurface_Blit;
     thiz->TileBlit = IDirectFBSurface_TileBlit;
     thiz->StretchBlit = IDirectFBSurface_StretchBlit;

     thiz->SetDrawingFlags = IDirectFBSurface_SetDrawingFlags;
     thiz->FillRectangle = IDirectFBSurface_FillRectangle;
     thiz->DrawLine = IDirectFBSurface_DrawLine;
     thiz->DrawLines = IDirectFBSurface_DrawLines;
     thiz->DrawRectangle = IDirectFBSurface_DrawRectangle;
     thiz->FillTriangle = IDirectFBSurface_FillTriangle;

     thiz->SetFont = IDirectFBSurface_SetFont;
     thiz->GetFont = IDirectFBSurface_GetFont;
     thiz->DrawString = IDirectFBSurface_DrawString;
     thiz->DrawGlyph = IDirectFBSurface_DrawGlyph;

     thiz->GetSubSurface = IDirectFBSurface_GetSubSurface;

     thiz->GetGL = IDirectFBSurface_GetGL;

     return DFB_OK;
}


/* internal */

static ReactionResult
IDirectFBSurface_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     IDirectFBSurface        *thiz         = (IDirectFBSurface*)ctx;
     IDirectFBSurface_data   *data         = (IDirectFBSurface_data*)thiz->priv;
     CoreSurface             *surface      = data->surface;

     if (notification->flags & CSNF_DESTROY) {
          data->surface = NULL;
          return RS_REMOVE;
     }

     if (notification->flags & CSNF_SIZEFORMAT) {
          DFBRectangle rect = { 0, 0, surface->width, surface->height };

          if (data->caps & DSCAPS_SUBSURFACE) {
               data->area.current = data->area.granted;
               dfb_rectangle_intersect( &data->area.current, &rect );
          }
          else
               data->area.wanted = data->area.granted = data->area.current = rect;

          /* Reset clip to avoid crashes caused by drawing out of bounds. */
          if (data->clip_set)
               thiz->SetClip( thiz, &data->clip_wanted );
          else
               thiz->SetClip( thiz, NULL );
     }

     return RS_OK;
}


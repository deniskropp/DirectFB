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
#include <string.h>

#include <math.h>

#include <malloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/fonts.h>

#include <misc/util.h>
#include <gfx/util.h>

#include <media/idirectfbfont.h>
#include "idirectfbsurface.h"

#include <directfb_internals.h>



void IDirectFBSurface_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data = (IDirectFBSurface_data*)thiz->priv;

     state_set_destination( &data->state, NULL );
     state_set_source( &data->state, NULL );
     
     surface_remove_listener( data->surface, IDirectFBSurface_listener, thiz );
     
     if (!(data->caps & DSCAPS_SUBSURFACE))
          surface_destroy( data->surface );

     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

DFBResult IDirectFBSurface_AddRef( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFBSurface_Release( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBSurface_Destruct( thiz );
     }

     return DFB_OK;
}


DFBResult IDirectFBSurface_GetPixelFormat( IDirectFBSurface *thiz,
                                           DFBSurfacePixelFormat *format )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     if (!format)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *format = data->surface->format;

     return DFB_OK;
}

DFBResult IDirectFBSurface_GetAccelerationMask( IDirectFBSurface    *thiz,
                                                IDirectFBSurface    *source,
                                                DFBAccelerationMask *mask )
{
     IDirectFBSurface_data *data;

     if (!thiz || !mask)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (source) {
          IDirectFBSurface_data *src_data = (IDirectFBSurface_data*)source->priv;

          state_set_source( &data->state, src_data->surface );
     }

     gfxcard_state_check( &data->state, DFXL_FILLRECTANGLE );
     gfxcard_state_check( &data->state, DFXL_DRAWRECTANGLE );
     gfxcard_state_check( &data->state, DFXL_DRAWLINE );

     if (source) {
          gfxcard_state_check( &data->state, DFXL_BLIT );
          gfxcard_state_check( &data->state, DFXL_STRETCHBLIT );
     }

     *mask = data->state.accel;

     return DFB_OK;
}

DFBResult IDirectFBSurface_GetSize( IDirectFBSurface *thiz,
                                    unsigned int *width, unsigned int *height )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->req_rect.w;

     if (height)
          *height = data->req_rect.h;

     return DFB_OK;
}

DFBResult IDirectFBSurface_GetVisibleRectangle( IDirectFBSurface *thiz,
                                                DFBRectangle     *rect )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!rect)
          return DFB_INVARG;

     rect->x = data->clip_rect.x - data->req_rect.x;
     rect->y = data->clip_rect.y - data->req_rect.y;
     rect->w = data->clip_rect.w;
     rect->h = data->clip_rect.h;

     return DFB_OK;
}

DFBResult IDirectFBSurface_GetCapabilities( IDirectFBSurface *thiz,
                                            DFBSurfaceCapabilities *caps )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!caps)
          return DFB_INVARG;

     *caps = data->caps;

     return DFB_OK;
}

DFBResult IDirectFBSurface_Lock( IDirectFBSurface *thiz,
                                 DFBSurfaceLockFlags flags,
                                 void **ptr, int *pitch )
{
     int front;
     DFBResult ret;
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!flags || !ptr || !pitch)
          return DFB_INVARG;

     front = (flags & DSLF_WRITE) ? 0 : 1;

     ret = surface_soft_lock( data->surface, flags, ptr, pitch, front );
     if (ret)
          return ret;

     /* FIXME: clip_rect has no effect here,
        application will write into non visible region if req/clip differ */
     
     (__u8*)(*ptr) += *pitch * data->req_rect.y + data->req_rect.x *
                                    BYTES_PER_PIXEL(data->surface->format);

     data->locked = front + 1;

     return DFB_OK;
}

DFBResult IDirectFBSurface_Unlock( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          surface_unlock( data->surface, data->locked - 1 );

     data->locked = 0;

     return DFB_OK;
}

DFBResult IDirectFBSurface_Flip( IDirectFBSurface *thiz, DFBRegion *region,
                                 DFBSurfaceFlipFlags flags )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (!(data->caps & DSCAPS_FLIPPING))
          return DFB_UNSUPPORTED;

     if (flags & DSFLIP_WAITFORSYNC) {
          return DFB_INVARG;
     }

     if (flags & DSFLIP_BLIT  ||  region  ||  data->caps & DSCAPS_SUBSURFACE) {
          if (region) {
               DFBRegion reg = *region;
               DFBRectangle rect = data->req_rect;

               reg.x1 += rect.x;
               reg.x2 += rect.x;
               reg.y1 += rect.y;
               reg.y2 += rect.y;

               if (rectangle_intersect_by_unsafe_region( &rect, &reg ) &&
                   rectangle_intersect( &rect, &data->clip_rect ))
                    back_to_front_copy( data->surface, &rect );
          }
          else {
               DFBRectangle rect = data->clip_rect;

               back_to_front_copy( data->surface, &rect );
          }
     }
     else
          surface_flip_buffers( data->surface );

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetClip( IDirectFBSurface *thiz, DFBRegion *clip )
{
     DFBRegion newclip;
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (clip) {
          newclip = *clip;

          newclip.x1 += data->req_rect.x;
          newclip.x2 += data->req_rect.x;
          newclip.y1 += data->req_rect.y;
          newclip.y2 += data->req_rect.y;

          if (!unsafe_region_rectangle_intersect( &newclip, &data->clip_rect ))
               return DFB_INVARG;
     }
     else {
          newclip.x1 = data->clip_rect.x;
          newclip.y1 = data->clip_rect.y;
          newclip.x2 = data->clip_rect.x + data->clip_rect.w - 1;
          newclip.y2 = data->clip_rect.y + data->clip_rect.h - 1;
     }

     if (memcmp( &data->state.clip, &newclip, sizeof(DFBRegion) )) {
          data->state.clip = newclip;
          data->state.modified |= SMF_CLIP;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetColor( IDirectFBSurface *thiz,
                                     __u8 r, __u8 g, __u8 b, __u8 a )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->state.color.a = a;
     data->state.color.r = r;
     data->state.color.g = g;
     data->state.color.b = b;

     data->state.modified |= SMF_COLOR;

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetSrcBlendFunction( IDirectFBSurface *thiz,
                                                DFBSurfaceBlendFunction src )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

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
               break;
          default:
               return DFB_INVARG;
     }

     if (data->state.src_blend != src) {
          data->state.src_blend = src;
          data->state.modified |= SMF_SRC_BLEND;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetDstBlendFunction( IDirectFBSurface *thiz,
                                                DFBSurfaceBlendFunction dst )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

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
               break;
          default:
               return DFB_INVARG;
     }

     if (data->state.dst_blend != dst) {
          data->state.dst_blend = dst;
          data->state.modified |= SMF_SRC_BLEND;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetPorterDuff( IDirectFBSurface *thiz,
                                          DFBSurfacePorterDuffRule rule )
{
     DFBSurfaceBlendFunction src;
     DFBSurfaceBlendFunction dst;
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

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

DFBResult IDirectFBSurface_SetSrcColorKey( IDirectFBSurface *thiz,
                                           unsigned int key )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->state.src_colorkey != key) {
          data->state.src_colorkey = key;
          data->state.modified |= SMF_SRC_COLORKEY;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetDstColorKey( IDirectFBSurface *thiz,
                                           unsigned int key )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->state.dst_colorkey != key) {
          data->state.dst_colorkey = key;
          data->state.modified |= SMF_DST_COLORKEY;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetFont( IDirectFBSurface *thiz,
                                    IDirectFBFont *font )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (!font)
          return DFB_INVARG;

     {
          IDirectFBFont_data *font_data = (IDirectFBFont_data*)font->priv;
          data->font = font_data->font;
          /* FIXME: AddRef */
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetDrawingFlags( IDirectFBSurface *thiz,
                                            DFBSurfaceDrawingFlags flags )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->state.drawingflags != flags) {
          data->state.drawingflags = flags;
          data->state.modified |= SMF_DRAWING_FLAGS;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_FillRectangle( IDirectFBSurface *thiz,
                                          int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->req_rect.x;
     rect.y += data->req_rect.y;

     gfxcard_fillrectangle( &rect, &data->state );

     return DFB_OK;
}


DFBResult IDirectFBSurface_DrawLine( IDirectFBSurface *thiz,
                                     int x1, int y1, int x2, int y2 )
{
     DFBRegion line = { x1, y1, x2, y2 };
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     line.x1 += data->req_rect.x;
     line.x2 += data->req_rect.x;
     line.y1 += data->req_rect.y;
     line.y2 += data->req_rect.y;

     gfxcard_drawline( &line, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_DrawRectangle( IDirectFBSurface *thiz,
                                          int x, int y, int w, int h )
{
     DFBRectangle rect = { x, y, w, h };
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (w<=0 || h<=0)
          return DFB_INVARG;

     rect.x += data->req_rect.x;
     rect.y += data->req_rect.y;

     gfxcard_drawrectangle( &rect, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_FillTriangle( IDirectFBSurface *thiz,
                                         int x1, int y1,
                                         int x2, int y2,
                                         int x3, int y3 )
{
     DFBTriangle tri = { x1, y1, x2, y2, x3, y3 };
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     tri.x1 += data->req_rect.x;
     tri.y1 += data->req_rect.y;
     tri.x2 += data->req_rect.x;
     tri.y2 += data->req_rect.y;
     tri.x3 += data->req_rect.x;
     tri.y3 += data->req_rect.y;

     gfxcard_filltriangle( &tri, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_SetBlittingFlags( IDirectFBSurface *thiz,
                                             DFBSurfaceBlittingFlags flags )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->state.blittingflags != flags) {
          data->state.blittingflags = flags;
          data->state.modified |= SMF_BLITTING_FLAGS;
     }

     return DFB_OK;
}

DFBResult IDirectFBSurface_Blit( IDirectFBSurface *thiz,
                                 IDirectFBSurface *source,
                                 DFBRectangle *sr,
                                 int dx, int dy )
{
     DFBRectangle srect;
     IDirectFBSurface_data *src_data = (IDirectFBSurface_data*)source->priv;
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (sr) {
          if (sr->w < 1  ||  sr->h < 1)
               return DFB_OK;

          srect = *sr;

          srect.x += src_data->req_rect.x;
          srect.y += src_data->req_rect.y;

          if (!rectangle_intersect( &srect, &src_data->clip_rect ))
               return DFB_OK;

          dx += srect.x - (sr->x + src_data->req_rect.x);
          dy += srect.y - (sr->y + src_data->req_rect.y);
     }
     else {
          srect = src_data->clip_rect;
          
          dx += srect.x - src_data->req_rect.x;
          dy += srect.y - src_data->req_rect.y;
     }

     state_set_source( &data->state, src_data->surface );

     gfxcard_blit( &srect,
                   data->req_rect.x + dx, data->req_rect.y + dy, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_StretchBlit( IDirectFBSurface *thiz,
                                        IDirectFBSurface *source,
                                        DFBRectangle *source_rect,
                                        DFBRectangle *destination_rect )
{
     DFBRectangle srect, drect;
     IDirectFBSurface_data *data;
     IDirectFBSurface_data *src_data = (IDirectFBSurface_data*)source->priv;


     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;


     /* do destination rectangle */
     if (destination_rect) {
          drect = *destination_rect;

          drect.x += data->req_rect.x;
          drect.y += data->req_rect.y;
     }
     else
          drect = data->req_rect;
     
     if (drect.w < 1  ||  drect.h < 1)
          return DFB_OK;
     
     
     /* do source rectangle */
     if (source_rect) {
          srect = *source_rect;

          srect.x += src_data->req_rect.x;
          srect.y += src_data->req_rect.y;
     }
     else
          srect = src_data->req_rect;

     if (srect.w < 1  ||  srect.h < 1)
          return DFB_OK;
     
     
     /* clipping of the source rectangle must be applied to the destination */
     {
          DFBRectangle orig_src = srect;

          if (!rectangle_intersect( &srect, &src_data->clip_rect ))
               return DFB_OK;

          if (srect.x != orig_src.x)
               drect.x += (int)( (srect.x - orig_src.x) *
                                 (drect.w / (float)orig_src.w) + 0.5f);

          if (srect.y != orig_src.y)
               drect.y += (int)( (srect.y - orig_src.y) *
                                 (drect.h / (float)orig_src.h) + 0.5f);

          if (srect.w != orig_src.w)
               drect.w = ceil(drect.w * (srect.w / (float)orig_src.w));
          if (srect.h != orig_src.h)
               drect.h = ceil(drect.h * (srect.h / (float)orig_src.h));
     }

     state_set_source( &data->state, src_data->surface );

     gfxcard_stretchblit( &srect, &drect, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_DrawString( IDirectFBSurface *thiz,
                                       const char *text, int x, int y,
                                       DFBSurfaceTextFlags flags )
{
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (!(flags & DSTF_TOP))
          y -= data->font->ascender;

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          CoreGlyphData       *glyph;
          const unsigned char *string = text;
          unichar              prev   = 0;
          unichar              current;
          int                  width  = 0;
          int                  kerning;

          while (*string) {
               current = utf8_get_char (string);
               
               if (fonts_get_glyph_data (data->font, current, &glyph) == DFB_OK) {
                  
                    if (prev && data->font->GetKerning && 
                        (* data->font->GetKerning) (data->font, prev, current, &kerning) == DFB_OK) {
                         width += kerning;
                    }
                    width += glyph->advance;

                    prev = current;
                    string = utf8_next_char (string);
               }
          }
          if (flags & DSTF_RIGHT) {
               x -= width;
          }
          else if (flags & DSTF_CENTER) {
               x -= width >> 1;
          }
     }

     gfxcard_drawstring( text, data->req_rect.x + x, data->req_rect.y + y,
                         data->font, &data->state );

     return DFB_OK;
}

DFBResult IDirectFBSurface_GetSubSurface( IDirectFBSurface       *thiz,
                                          DFBRectangle           *rect,
                                          IDirectFBSurface       **surface )
{
     DFBRectangle req, clip;
     IDirectFBSurface_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBSurface_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->locked)
          return DFB_LOCKED;

     if (rect) {
          if (rect->w < 0  ||  rect->h < 0)
               return DFB_INVARG;

          req = *rect;

          req.x += data->req_rect.x;
          req.y += data->req_rect.y;
     }
     else
          req = data->req_rect;

     clip = req;

     if (!rectangle_intersect( &clip, &data->clip_rect ))
          return DFB_INVARG;

     DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

     return IDirectFBSurface_Construct( *surface, &req, &clip,
                                        data->surface,
                                        data->caps | DSCAPS_SUBSURFACE );
}

/******/

DFBResult IDirectFBSurface_Construct( IDirectFBSurface       *thiz,
                                      DFBRectangle           *req_rect,
                                      DFBRectangle           *clip_rect,
                                      CoreSurface            *surface,
                                      DFBSurfaceCapabilities  caps )
{
     IDirectFBSurface_data *data;

     data = (IDirectFBSurface_data*)malloc( sizeof(IDirectFBSurface_data) );
     memset( data, 0, sizeof(IDirectFBSurface_data) );
     thiz->priv = data;

     data->ref = 1;
     data->caps = caps;

     if (req_rect)
          data->req_rect = *req_rect;
     else {
          data->req_rect.x = 0;
          data->req_rect.y = 0;
          data->req_rect.w = surface->width;
          data->req_rect.h = surface->height;
     }

     if (clip_rect)
        data->clip_rect = *clip_rect;
     else
        data->clip_rect = data->req_rect;

     data->surface = surface;
     data->font = fonts_get_default();

     /* all other values got zero */
     state_set_destination( &data->state, surface );

     surface_install_listener( surface,
                               IDirectFBSurface_listener, CSN_DESTROY, thiz );

     data->state.clip.x1 = data->clip_rect.x;
     data->state.clip.y1 = data->clip_rect.y;
     data->state.clip.x2 = data->clip_rect.x + data->clip_rect.w - 1;
     data->state.clip.y2 = data->clip_rect.y + data->clip_rect.h - 1;
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

     thiz->Lock = IDirectFBSurface_Lock;
     thiz->Unlock = IDirectFBSurface_Unlock;
     thiz->Flip = IDirectFBSurface_Flip;

     thiz->SetClip = IDirectFBSurface_SetClip;
     thiz->SetColor = IDirectFBSurface_SetColor;
     thiz->SetSrcBlendFunction = IDirectFBSurface_SetSrcBlendFunction;
     thiz->SetDstBlendFunction = IDirectFBSurface_SetDstBlendFunction;
     thiz->SetPorterDuff = IDirectFBSurface_SetPorterDuff;
     thiz->SetSrcColorKey = IDirectFBSurface_SetSrcColorKey;
     thiz->SetDstColorKey = IDirectFBSurface_SetDstColorKey;

     thiz->SetBlittingFlags = IDirectFBSurface_SetBlittingFlags;
     thiz->Blit = IDirectFBSurface_Blit;
     thiz->StretchBlit = IDirectFBSurface_StretchBlit;

     thiz->SetDrawingFlags = IDirectFBSurface_SetDrawingFlags;
     thiz->FillRectangle = IDirectFBSurface_FillRectangle;
     thiz->DrawLine = IDirectFBSurface_DrawLine;
     thiz->DrawRectangle = IDirectFBSurface_DrawRectangle;
     thiz->FillTriangle = IDirectFBSurface_FillTriangle;

     thiz->SetFont = IDirectFBSurface_SetFont;
     thiz->DrawString = IDirectFBSurface_DrawString;

     thiz->GetSubSurface = IDirectFBSurface_GetSubSurface;

     return DFB_OK;
}


SLResult IDirectFBSurface_listener( CoreSurface  *surface,
                                    unsigned int  flags,
                                    void         *ctx )
{
     IDirectFBSurface      *thiz = (IDirectFBSurface*)ctx;
     IDirectFBSurface_data *data = (IDirectFBSurface_data*)thiz->priv;

     if (data) {
          DEBUGMSG("IDirectFBSurface: "
                   "CoreSurface vanished! I'm dead now!!!\n");

          thiz->Unlock( thiz );
     
          free( data );
          thiz->priv = NULL;
     }
     
     return SL_REMOVE;
}


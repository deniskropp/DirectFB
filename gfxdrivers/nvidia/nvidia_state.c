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

#include <stdio.h>
#include <stdlib.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/system.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"
#include "nvidia_objects.h"
#include "nvidia_state.h"


#define NVIDIA_IS_SET( flag )  ((nvdev->set & SMF_##flag) == SMF_##flag)

#define NVIDIA_SET( flag )      nvdev->set |= SMF_##flag

#define NVIDIA_UNSET( flag )    nvdev->set &= ~SMF_##flag



void nv_set_destination( NVidiaDriverData *nvdrv,
                         NVidiaDeviceData *nvdev,
                         CardState        *state )
{
     CoreSurface   *surface     = state->destination;
     SurfaceBuffer *buffer      = surface->back_buffer;
     volatile u8   *mmio        = nvdrv->mmio_base;
     u32            dst_offset;
     u32            dst_pitch;
     u32            src_pitch;
     u32            depth_offset;
     u32            depth_pitch;
     
     if (NVIDIA_IS_SET( DESTINATION ))
          return;
          
     dst_offset = (buffer->video.offset + nvdev->fb_offset) & ~63;
     dst_pitch  = buffer->video.pitch & ~31;
     src_pitch  = (nvdev->src_pitch & ~31) ? : 32; // align to 32, maybe system buffer pitch
     
     if (surface->caps & DSCAPS_DEPTH) {
          depth_offset = surface->depth_buffer->video.offset;
          depth_offset = (depth_offset + nvdev->fb_offset) & ~63;
          depth_pitch  = surface->depth_buffer->video.pitch & ~63;
     } else {
          depth_offset = 0;
          depth_pitch  = 64;
     }

     if (nvdev->dst_format != buffer->format) {
          u32   sformat2D = 0;
          u32   sformat3D = 0;
          u32   cformat   = 0;
          bool  dst_422 = false;

          switch (buffer->format) {
               case DSPF_A8:
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_RGB332:
                    sformat2D = SURFACES2D_FORMAT_Y8;
                    cformat   = RECT_COLOR_FORMAT_Y32;
                    break;
               case DSPF_RGB555:
                    sformat2D = SURFACES2D_FORMAT_X1R5G5B5;
                    sformat3D = SURFACES3D_FORMAT_COLOR_X1R5G5B5;
                    cformat   = RECT_COLOR_FORMAT_A1Y15;
                    break;
               case DSPF_ARGB1555:
                    sformat2D = SURFACES2D_FORMAT_A1R5G5B5;
                    sformat3D = SURFACES3D_FORMAT_COLOR_A1R5G5B5;
                    cformat   = RECT_COLOR_FORMAT_A1Y15;
                    break;
               case DSPF_RGB16:
                    sformat2D = SURFACES2D_FORMAT_R5G6B5;
                    sformat3D = SURFACES3D_FORMAT_COLOR_R5G6B5;
                    cformat   = RECT_COLOR_FORMAT_Y16;
                    break;
               case DSPF_RGB32:
                    sformat2D = SURFACES2D_FORMAT_X8R8G8B8;
                    sformat3D = SURAFCES3D_FORMAT_COLOR_X8R8G8B8;
                    cformat   = RECT_COLOR_FORMAT_Y32;
                    break;
               case DSPF_ARGB:
                    sformat2D = SURFACES2D_FORMAT_A8R8G8B8;
                    sformat3D = SURFACES3D_FORMAT_COLOR_A8R8G8B8;
                    cformat   = 0x0D;
                    break;
               case DSPF_YUY2:
                    sformat2D = SURFACES2D_FORMAT_A8R8G8B8;
                    cformat   = 0x12;
                    dst_422 = true;
                    break;
               case DSPF_UYVY:
                    sformat2D = SURFACES2D_FORMAT_A8R8G8B8;
                    cformat   = 0x13;
                    dst_422 = true;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          if (sformat2D == SURFACES2D_FORMAT_A8R8G8B8) {     
               /* need to set color format manually */
               nv_waitidle( nvdrv, nvdev );

               nv_out32( mmio, PRAMIN + (ADDR_RECTANGLE << 4) + 4, cformat << 8 );
               nv_out32( mmio, PRAMIN + (ADDR_TRIANGLE  << 4) + 4, cformat << 8 );
               nv_out32( mmio, PRAMIN + (ADDR_LINE      << 4) + 4, cformat << 8 );

               nv_assign_object( nvdrv, nvdev, 
                                 SUBC_RECTANGLE, OBJ_RECTANGLE, true );
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_TRIANGLE, OBJ_TRIANGLE, true );
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_LINE, OBJ_LINE, true );
          } else {
               nv_begin( SUBC_RECTANGLE, RECT_COLOR_FORMAT, 1 );
               nv_outr( cformat );
               
               nv_begin( SUBC_TRIANGLE, TRI_COLOR_FORMAT, 1 );
               nv_outr( cformat );
               
               nv_begin( SUBC_LINE, LINE_COLOR_FORMAT, 1 );
               nv_outr( cformat );
          }

          nv_assign_object( nvdrv, nvdev, 
                            SUBC_SURFACES2D, OBJ_SURFACES2D, false );

          nv_begin( SUBC_SURFACES2D, SURFACES2D_FORMAT, 2 );
          nv_outr( sformat2D );
          nv_outr( (dst_pitch << 16) | (src_pitch & 0xFFFF) );
          nv_begin( SUBC_SURFACES2D, SURFACES2D_DST_OFFSET, 1 );
          nv_outr( dst_offset );

          if (nvdev->enabled_3d && sformat3D) { 
               sformat3D |= SURFACES3D_FORMAT_TYPE_PITCH;
               
               nv_assign_object( nvdrv, nvdev, 
                                 SUBC_SURFACES3D, OBJ_SURFACES3D, false );
               
               if (surface->caps & DSCAPS_DEPTH) {
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_FORMAT, 1 );
                    nv_outr( sformat3D );
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_PITCH, 3 );
                    nv_outr( (depth_pitch << 16) | (dst_pitch & 0xFFFF) );
                    nv_outr( dst_offset );
                    nv_outr( depth_offset );
                    
                    nvdev->state3d[1].control |= TXTRI_CONTROL_Z_ENABLE;
               } 
               else {
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_FORMAT, 1 );
                    nv_outr( sformat3D );
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_PITCH, 2 );
                    nv_outr( (depth_pitch << 16) | (dst_pitch & 0xFFFF) );
                    nv_outr( dst_offset );
                    
                    nvdev->state3d[1].control &= ~TXTRI_CONTROL_Z_ENABLE;
               }
          }
          
          if (nvdev->dst_422 != dst_422) {
               NVIDIA_UNSET( CLIP );
               NVIDIA_UNSET( BLITTING_FLAGS );
               nvdev->dst_422 = dst_422;
          }
          
          NVIDIA_UNSET( COLOR );
          NVIDIA_UNSET( DST_BLEND );
     }
     else {
          nv_assign_object( nvdrv, nvdev,
                            SUBC_SURFACES2D, OBJ_SURFACES2D, false );

          nv_begin( SUBC_SURFACES2D, SURFACES2D_PITCH, 1 );
          nv_outr( (dst_pitch << 16) | (src_pitch & 0xFFFF) );
          nv_begin( SUBC_SURFACES2D, SURFACES2D_DST_OFFSET, 1 );
          nv_outr( dst_offset );

          if (nvdev->enabled_3d) {
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_SURFACES3D, OBJ_SURFACES3D, false );

               if (surface->caps & DSCAPS_DEPTH) {
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_PITCH, 3 );
                    nv_outr( (depth_pitch << 16) | (dst_pitch & 0xFFFF) );
                    nv_outr( dst_offset );
                    nv_outr( depth_offset );
                    
                    nvdev->state3d[1].control |= TXTRI_CONTROL_Z_ENABLE;
               }
               else {
                    nv_begin( SUBC_SURFACES3D, SURFACES3D_PITCH, 2 );
                    nv_outr( (depth_pitch << 16) | (dst_pitch & 0xFFFF) );
                    nv_outr( dst_offset );
                    
                    nvdev->state3d[1].control &= ~TXTRI_CONTROL_Z_ENABLE;
               }
          }
     }
     
     nvdev->dst_format = buffer->format;
     nvdev->dst_offset = dst_offset;
     nvdev->dst_pitch  = dst_pitch;
     
     NVIDIA_SET( DESTINATION );
}

void nv_set_source( NVidiaDriverData *nvdrv,
                    NVidiaDeviceData *nvdev,
                    CardState        *state )
{
     CoreSurface   *surface = state->source;
     SurfaceBuffer *buffer  = surface->front_buffer;
          
     if (NVIDIA_IS_SET( SOURCE )) {
          if ((state->blittingflags & DSBLIT_DEINTERLACE) ==
              (nvdev->blittingflags & DSBLIT_DEINTERLACE))
               return;
     }
          
     if (buffer->policy == CSP_SYSTEMONLY) { 
          if (!nvdev->src_system) {
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_IMAGEBLT, OBJ_IMAGEBLT, false );
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_STRETCHEDIMAGE, OBJ_STRETCHEDIMAGE, false );
               
               NVIDIA_UNSET( BLITTING_FLAGS );
          }

          nvdev->src_address = buffer->system.addr;
          nvdev->src_pitch   = buffer->system.pitch;
          nvdev->src_system  = true;
     }
     else {
          u32 src_offset = (buffer->video.offset + nvdev->fb_offset) & ~63;
          u32 src_pitch  = buffer->video.pitch & ~31;

          nv_assign_object( nvdrv, nvdev,
                            SUBC_SURFACES2D, OBJ_SURFACES2D, false );
          
          nv_begin( SUBC_SURFACES2D, SURFACES2D_PITCH, 2 );
          nv_outr( (nvdev->dst_pitch << 16) | (src_pitch & 0xFFFF) );
          nv_outr( src_offset );

          if (nvdev->src_system) {
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_SCREENBLT, OBJ_SCREENBLT, false );
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_SCALEDIMAGE, OBJ_SCALEDIMAGE, false );
                          
               NVIDIA_UNSET( BLITTING_FLAGS );
          }
               
          nvdev->src_offset = src_offset;
          nvdev->src_pitch  = src_pitch;
          nvdev->src_system = false;
     }

     nvdev->src_width  = surface->width;
     nvdev->src_height = surface->height;

     if (state->blittingflags & DSBLIT_DEINTERLACE) {
          nvdev->src_height /= 2;
          if (surface->caps & DSCAPS_SEPARATED) {
               if (surface->field) {
                    nvdev->src_address += nvdev->src_height * nvdev->src_pitch;
                    nvdev->src_offset  += nvdev->src_height * nvdev->src_pitch;
               }
          } else {
               if (surface->field) {
                    nvdev->src_address += nvdev->src_pitch;
                    nvdev->src_offset  += nvdev->src_pitch;
               }
               nvdev->src_pitch *= 2;
          }
          nvdev->src_interlaced = true;
     } else
          nvdev->src_interlaced = false;
     
     if (nvdev->enabled_3d) {
          u32 size_u = direct_log2( surface->width  ) & 0xF;
          u32 size_v = direct_log2( surface->height ) & 0xF;

          nvdev->state3d[1].offset  = nvdev->fb_offset + nvdev->buf_offset[1];
          nvdev->state3d[1].format &= 0xFF00FFFF;
          nvdev->state3d[1].format |= (size_u << 16) | (size_v << 20);
     }

     if (nvdev->src_format != buffer->format) {
          NVIDIA_UNSET( SRC_BLEND );
          NVIDIA_UNSET( BLITTING_FLAGS );
     }
     nvdev->src_format = buffer->format;
     
     NVIDIA_SET( SOURCE );
}

void nv_set_clip( NVidiaDriverData *nvdrv,
                  NVidiaDeviceData *nvdev,
                  CardState        *state )
{
     DFBRectangle *cr = &nvdev->clip;
     
     if (NVIDIA_IS_SET( CLIP ))
          return;

     cr->x = state->clip.x1;
     cr->y = state->clip.y1;
     cr->w = state->clip.x2 - state->clip.x1 + 1;
     cr->h = state->clip.y2 - state->clip.y1 + 1;

     if (nvdev->dst_422) {
          cr->x =  cr->x / 2;
          cr->w = (cr->w / 2) ? : 1;
     }

     nv_begin( SUBC_CLIP, CLIP_TOP_LEFT, 2 );
     nv_outr( (cr->y << 16) | (cr->x & 0xFFFF) );
     nv_outr( (cr->h << 16) | (cr->w & 0xFFFF) );

     NVIDIA_SET( CLIP );
}

void nv_set_drawing_color( NVidiaDriverData *nvdrv,
                           NVidiaDeviceData *nvdev,
                           CardState        *state )
{
     DFBColor color   = state->color;
     int      y, u, v;
     
     if (NVIDIA_IS_SET( DRAWING_COLOR ) && NVIDIA_IS_SET( DRAWING_FLAGS ))
          return;
     
     switch (nvdev->dst_format) {
          case DSPF_A8:
               nvdev->color2d = color.a;
               break;
          case DSPF_LUT8:
               nvdev->color2d = state->color_index;
               break;
          case DSPF_ALUT44:
               nvdev->color2d = (state->color_index & 0x0F) |
                                (state->color.a     & 0xF0);
               break;
          case DSPF_RGB332:
               nvdev->color2d = PIXEL_RGB332( color.r,
                                              color.g,
                                              color.b );
               break;
          case DSPF_RGB555:
          case DSPF_ARGB1555:
               nvdev->color2d = PIXEL_ARGB1555( color.a,
                                                color.r,
                                                color.g,
                                                color.b );
               break;
          case DSPF_RGB16:
               nvdev->color2d = PIXEL_RGB16( color.r,
                                             color.g,
                                             color.b );
               break;
          case DSPF_RGB32:
               nvdev->color2d = PIXEL_RGB32( color.r,
                                             color.g,
                                             color.b );
               break;
          case DSPF_ARGB:
               nvdev->color2d = PIXEL_ARGB( color.a,
                                            color.r,
                                            color.g,
                                            color.b );
               break;

          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               nvdev->color2d = PIXEL_YUY2( y, u, v );
               break;

          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               nvdev->color2d = PIXEL_UYVY( y, u, v );
               break;

          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }
     
     if (nvdev->dst_format == DSPF_ARGB1555) {
          nv_assign_object( nvdrv, nvdev,
                            SUBC_SURFACES2D, OBJ_SURFACES2D, false );
          
          nv_begin( SUBC_SURFACES2D, SURFACES2D_FORMAT, 1 );
          nv_outr( (nvdev->color2d & 0x8000) 
                   ? SURFACES2D_FORMAT_A1R5G5B5
                   : SURFACES2D_FORMAT_X1R5G5B5 );
     }
  
     if (state->drawingflags & DSDRAW_BLEND) {
          if (!nvdev->enabled_3d) {
               if (!nvdev->beta1_set || nvdev->beta1_val != (color.a << 23)) {
                    nv_assign_object( nvdrv, nvdev, 
                                      SUBC_BETA1, OBJ_BETA1, false );
                    
                    nv_begin( SUBC_BETA1, BETA1_FACTOR, 1 );
                    nv_outr( color.a << 23 );

                    nvdev->beta1_val = color.a << 23;
                    nvdev->beta1_set = true;
               }
          } 
          else {
               nvdev->color3d = PIXEL_ARGB( color.a, color.r,
                                            color.g, color.b );
          }
     }
     
     NVIDIA_SET  ( DRAWING_COLOR );
     NVIDIA_UNSET( BLITTING_COLOR );
}

void nv_set_blitting_color( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state )
{
     DFBColor color = state->color;
     
     if (NVIDIA_IS_SET( BLITTING_COLOR ) && NVIDIA_IS_SET( BLITTING_FLAGS ))
          return;
                    
     if (state->src_blend == DSBF_ONE ||
         state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR)) {       
          nvdev->color3d = (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                           ? (color.a << 24) : 0xFF000000;
          
          if (state->blittingflags & DSBLIT_COLORIZE &&
              state->blittingflags & (DSBLIT_SRC_PREMULTCOLOR | DSBLIT_BLEND_COLORALPHA)) {
               nvdev->color3d |= PIXEL_RGB32( color.r * color.a / 0xFF,
                                              color.g * color.a / 0xFF,
                                              color.b * color.a / 0xFF );
          }
          else if (state->blittingflags & DSBLIT_COLORIZE) {
               nvdev->color3d |= PIXEL_RGB32( color.r, color.g, color.b );
          }
          else if (state->blittingflags & (DSBLIT_SRC_PREMULTCOLOR | DSBLIT_BLEND_COLORALPHA)) {
               nvdev->color3d |= PIXEL_RGB32( color.a, color.a, color.a );
          }
          else {
               nvdev->color3d |= 0x00FFFFFF;
          }

          if (!nvdev->beta4_set || nvdev->beta4_val != nvdev->color3d) {
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_BETA4, OBJ_BETA4, false );
                                 
               nv_begin( SUBC_BETA4, BETA4_FACTOR, 1 );
               nv_outr( nvdev->color3d );

               nvdev->beta4_val = nvdev->color3d;
               nvdev->beta4_set = true;
          }
     }
     else if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_BLEND_ALPHACHANNEL)) {
          u32 beta1;
          
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               nvdev->color3d = (color.a << 24) | 0x00FFFFFF;
               beta1          =  color.a << 23;
          } else {
               nvdev->color3d = 0xFFFFFFFF;
               beta1          = 0x7F800000;
          }
          
          if (!nvdev->beta1_set || nvdev->beta1_val != beta1) {
               nv_assign_object( nvdrv, nvdev,
                                 SUBC_BETA1, OBJ_BETA1, false );

               nv_begin( SUBC_BETA1, BETA1_FACTOR, 1 );
               nv_outr( beta1 );

               nvdev->beta1_val = beta1;
               nvdev->beta1_set = true;
          }
     }
     
     NVIDIA_SET  ( BLITTING_COLOR );
     NVIDIA_UNSET( DRAWING_COLOR );
}

void nv_set_blend_function( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state )
{
     DFBSurfaceBlendFunction sblend, dblend;
     
     if (NVIDIA_IS_SET( SRC_BLEND ) && NVIDIA_IS_SET( DST_BLEND ))
          return;
       
     sblend = state->src_blend;
     dblend = state->dst_blend;
     
#if 0
     if (!DFB_PIXELFORMAT_HAS_ALPHA(nvdev->src_format)) {
          if (sblend == DSBF_SRCALPHA)
               sblend = DSBF_ONE;
          else if (sblend == DSBF_INVSRCALPHA)
               sblend = DSBF_ZERO;
               
          if (dblend == DSBF_SRCALPHA)
               dblend = DSBF_ONE;
          else if (dblend == DSBF_INVSRCALPHA)
               dblend = DSBF_ZERO;
     }
#endif
     if (!DFB_PIXELFORMAT_HAS_ALPHA(nvdev->dst_format)) {
          if (sblend == DSBF_DESTALPHA)
               sblend = DSBF_ONE;
          else if (sblend == DSBF_INVDESTALPHA)
               sblend = DSBF_ZERO;
               
          if (dblend == DSBF_DESTALPHA)
               dblend = DSBF_ONE;
          else if (dblend == DSBF_INVDESTALPHA)
               dblend = DSBF_ZERO;
     }
          
     nvdev->state3d[0].blend &= 0x00FFFFFF;
     nvdev->state3d[0].blend |= (sblend << 24) | (dblend << 28);
     nvdev->state3d[1].blend &= 0x00FFFFFF;
     nvdev->state3d[1].blend |= (sblend << 24) | (dblend << 28);
         
     if (!NVIDIA_IS_SET( SRC_BLEND ))
          NVIDIA_UNSET( BLITTING_FLAGS );
     NVIDIA_SET( SRC_BLEND );
     NVIDIA_SET( DST_BLEND );
}

void nv_set_drawingflags( NVidiaDriverData *nvdrv,
                          NVidiaDeviceData *nvdev,
                          CardState        *state )
{
     if (NVIDIA_IS_SET( DRAWING_FLAGS ))
          return;
          
     if (!nvdev->enabled_3d) {
          u32 operation;
          
          if (state->drawingflags & DSDRAW_BLEND)
               operation = OPERATION_BLEND;
          else
               operation = OPERATION_SRCCOPY;
          
          if (nvdev->drawing_operation != operation) {
               nv_begin( SUBC_RECTANGLE, RECT_OPERATION, 1 );
               nv_outr( operation );
               
               nv_begin( SUBC_TRIANGLE, TRI_OPERATION, 1 );
               nv_outr( operation );
               
               nv_begin( SUBC_LINE, LINE_OPERATION, 1 );
               nv_outr( operation );
               
               nvdev->drawing_operation = operation;
          }
     }

     nvdev->drawingflags = state->drawingflags;
     
     NVIDIA_SET( DRAWING_FLAGS );
}

void nv_set_blittingflags( NVidiaDriverData *nvdrv,
                           NVidiaDeviceData *nvdev,
                           CardState        *state )
{
     u32  operation;
     bool src_alpha;
     
     if (NVIDIA_IS_SET( BLITTING_FLAGS ))
          return;
          
     operation = (nvdev->arch > NV_ARCH_04)
                 ? OPERATION_SRCCOPY : OPERATION_COPY;
     src_alpha = !(  state->blittingflags & DSBLIT_BLEND_COLORALPHA  && 
                   !(state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) );
     
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL)) {
          if (state->src_blend == DSBF_ONE ||
              state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR))
               operation = OPERATION_BLEND_PREMULTIPLIED;
          else
               operation = OPERATION_BLEND;
     }
     else if (state->blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR)) {
          operation = OPERATION_COLOR_MULTIPLY;
     }

     if (nvdev->src_system) {
          switch (nvdev->src_format) {
               case DSPF_RGB555:
                    nvdev->system_format = IBLIT_COLOR_FORMAT_X1R5G5B5;
                    break;
               case DSPF_ARGB1555:
                    nvdev->system_format = src_alpha
                                           ? IBLIT_COLOR_FORMAT_A1R5G5B5 
                                           : IBLIT_COLOR_FORMAT_X1R5G5B5;
                    break;
               case DSPF_RGB16:
                    nvdev->system_format = IBLIT_COLOR_FORMAT_R5G6B5;
                    break;
               case DSPF_RGB32:
                    nvdev->system_format = IBLIT_COLOR_FORMAT_X8R8G8B8;
                    break;
               case DSPF_ARGB:
                    nvdev->system_format = src_alpha 
                                           ? IBLIT_COLOR_FORMAT_A8R8G8B8 
                                           : IBLIT_COLOR_FORMAT_X8R8G8B8;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }
 
          if (nvdev->system_operation != operation) {
               nv_begin( SUBC_IMAGEBLT, IBLIT_OPERATION, 1 );
               nv_outr( operation );
               
               nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_OPERATION, 1 );
               nv_outr( operation );
               
               nvdev->system_operation = operation;
          }
     }
     else {
          switch (nvdev->src_format) {
               case DSPF_A8:
                    nvdev->scaler_format = SCALER_COLOR_FORMAT_AY8;
                    break;
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_RGB332:
                    nvdev->scaler_format = SCALER_COLOR_FORMAT_Y8;
                    break;
               case DSPF_RGB555:
                    nvdev->scaler_format = SCALER_COLOR_FORMAT_X1R5G5B5;
                    break;
               case DSPF_ARGB1555:
                    nvdev->scaler_format = src_alpha
                                           ? SCALER_COLOR_FORMAT_A1R5G5B5
                                           : SCALER_COLOR_FORMAT_X1R5G5B5;
                    break;
               case DSPF_RGB16:
                    nvdev->scaler_format = SCALER_COLOR_FORMAT_R5G6B5;
                    break;
               case DSPF_RGB32:
                    nvdev->scaler_format = SCALER_COLOR_FORMAT_X8R8G8B8;
                    break;
               case DSPF_ARGB:
                    nvdev->scaler_format = src_alpha
                                           ? SCALER_COLOR_FORMAT_A8R8G8B8
                                           : SCALER_COLOR_FORMAT_X8R8G8B8;
                    break;
               case DSPF_YUY2:
                    nvdev->scaler_format = nvdev->dst_422
                                           ? SCALER_COLOR_FORMAT_A8R8G8B8
                                           : SCALER_COLOR_FORMAT_V8YB8U8YA8;
                    break;
               case DSPF_UYVY:
                    nvdev->scaler_format = nvdev->dst_422
                                           ? SCALER_COLOR_FORMAT_A8R8G8B8
                                           : SCALER_COLOR_FORMAT_YB8V8YA8U8;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }
          
          if (nvdev->scaler_operation != operation) {
               nv_begin( SUBC_SCALEDIMAGE, SCALER_OPERATION, 1 );
               nv_outr( operation );
               
               nvdev->scaler_operation = operation;
          }
     }
     
     if (nvdev->enabled_3d) {
          nvdev->state3d[1].format &= 0xFFFFF0FF;
          nvdev->state3d[1].blend  &= 0xFF00FFF0;

          switch (nvdev->src_format) {
               case DSPF_RGB555:
                    nvdev->state3d[1].format |= TXTRI_FORMAT_COLOR_X1R5G5B5;
                    break;
               case DSPF_ARGB1555:
                    nvdev->state3d[1].format |= TXTRI_FORMAT_COLOR_A1R5G5B5;
                    break;
               case DSPF_A8:
               case DSPF_ARGB:
                    nvdev->state3d[1].format |= TXTRI_FORMAT_COLOR_A4R4G4B4;
                    break;
               default:
                    nvdev->state3d[1].format |= TXTRI_FORMAT_COLOR_R5G6B5;
                    break;
          }
          
          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE | 
                                      DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_SRC_PREMULTCOLOR)) {                         
               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                    nvdev->state3d[1].blend |= 
                         TXTRI_BLEND_TEXTUREMAPBLEND_MODULATEALPHA;
               else
                    nvdev->state3d[1].blend |= 
                         TXTRI_BLEND_TEXTUREMAPBLEND_MODULATE;

               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                           DSBLIT_BLEND_ALPHACHANNEL))
                    nvdev->state3d[1].blend |= TXTRI_BLEND_ALPHABLEND_ENABLE;
          } else
               nvdev->state3d[1].blend |= TXTRI_BLEND_TEXTUREMAPBLEND_COPY;
     }
 
     nvdev->blittingflags = state->blittingflags;
     
     NVIDIA_SET( BLITTING_FLAGS );
}


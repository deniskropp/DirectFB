/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan82@cheapnet.it>.

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

#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/system.h>
#include <core/screens.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>
#include <core/layers.h>

#include "config.h"

DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"
#include "nvidia_tables.h"



#define NV_FIFO_FREE( nvdev, ptr, space )                        \
{                                                                \
     (nvdev)->waitfifo_sum += (space);                           \
     (nvdev)->waitfifo_calls++;                                  \
                                                                 \
     if ((nvdev)->fifo_space < (space)) {                        \
          do {                                                   \
               (nvdev)->fifo_space = (ptr)->FifoFree >> 2;       \
               (nvdev)->fifo_waitcycles++;                       \
          } while ((nvdev)->fifo_space < (space));               \
     }                                                           \
     else {                                                      \
          (nvdev)->fifo_cache_hits++;                            \
     }                                                           \
                                                                 \
     (nvdev)->fifo_space -= (space);                             \
}

#define NV_LOAD_TABLE( dest, table )                             \
{                                                                \
     int _i = 0;                                                 \
     for (; _i < sizeof(nv##table) / sizeof(nv##table[0]); _i++) \
          dest[nv##table[_i][0]] = nv##table[_i][1];             \
}

/* TNT */
#define NV4_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)

/* TNT2 or newer */
#define NV5_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV5_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV5_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA)

#define NV5_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


static inline void nv_waitidle( NVidiaDriverData *nvdrv,
                                NVidiaDeviceData *nvdev )
{
     while (nvdrv->PGRAPH[0x1C0] & 1) {
          nvdev->idle_waitcycles++;
     }
}

static void nvEngineSync( void *drv, void *dev )
{
     nv_waitidle( (NVidiaDriverData*) drv, (NVidiaDeviceData*) dev );
}

static void nvCheckState( void *drv, void *dev,
                          CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     switch (destination->format) {
       case DSPF_ARGB1555:
       case DSPF_RGB16:
       case DSPF_RGB32:
       case DSPF_ARGB:
               break;
       case DSPF_YUY2:
       case DSPF_UYVY:
               if (accel != DFXL_BLIT)
                    return;
               break;
       default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting flags first */
          if (state->blittingflags & ~nvdev->supported_blittingflags)
               return;

          /* FIXME: RGB16 */
          if (accel == DFXL_STRETCHBLIT ||
              source->format != destination->format) {
               switch (source->format) {
                    case DSPF_ARGB1555:
                    case DSPF_RGB32:
                    case DSPF_ARGB:
                    case DSPF_YUY2:
                    case DSPF_UYVY:
                         if ((destination->format == DSPF_YUY2  || 
                              destination->format == DSPF_UYVY) &&
                              destination->format != source->format)
                              return;
                         break;
                    default:
                         return;
               }
          }

          state->accel |= nvdev->supported_blittingfunctions;
     }
     else {
          /* check unsupported drawing flags first */
          if (state->drawingflags & ~nvdev->supported_drawingflags)
               return;

          if (state->drawingflags & DSDRAW_BLEND) {
               switch (state->src_blend) {
                    case DSBF_SRCALPHA:
                         break;
                    default:
                         return;
               }

               switch (state->dst_blend) {
                    case DSBF_INVSRCALPHA:
                         break;
                    default:
                         return;
               }
          }
            
          state->accel |= nvdev->supported_drawingfunctions;
     }
}

static inline void nv_set_clip( NVidiaDriverData *nvdrv,
                                NVidiaDeviceData *nvdev,
                                DFBRegion        *clip )
{
     volatile NVClip *Clip   = nvdrv->Clip;
     int              width  = clip->x2 - clip->x1 + 1;
     int              height = clip->y2 - clip->y1 + 1;

     NV_FIFO_FREE( nvdev, Clip, 2 )
     Clip->TopLeft     = (clip->y1 << 16) | clip->x1;
     Clip->WidthHeight = (height << 16) | width;
}

static void nvSetState( void *drv, void *dev,
                        GraphicsDeviceFuncs *funcs,
                        CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     volatile __u32   *PRAMIN = nvdrv->PRAMIN;
     volatile __u32   *FIFO   = nvdrv->FIFO;
     
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               state->set |= DFXL_FILLTRIANGLE |
                             DFXL_FILLRECTANGLE |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE |
                             DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (state->modified & SMF_DESTINATION) {
          /* set offset & pitch */
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x640/4] = state->destination->back_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x670/4] = state->destination->back_buffer->video.pitch;

          if (!nvdev->destination ||
              state->destination->format != nvdev->destination->format) {
               /* change objects pixelformat */
               switch (state->destination->format) {
                    case DSPF_ARGB1555:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB1555 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB1555 )
                         break;
            
                    case DSPF_RGB16:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB16 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB16 )
                         break;
            
                    case DSPF_RGB32:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB32 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
                         break;

                    case DSPF_ARGB:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB )
                         break;

                    case DSPF_YUY2:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_YUY2 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
                         break;

                    case DSPF_UYVY:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_UYVY )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
                         break;
            
                    default:
                         D_BUG( "unexpected pixelformat" );
                         break;
               }

               /* put objects into subchannels */
               NV_LOAD_TABLE( FIFO, FIFO )
          }
       
          nvdev->destination = state->destination;
     }

     if ((state->modified & SMF_SOURCE) && state->source) {
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x644/4] = state->source->front_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x674/4] = state->source->front_buffer->video.pitch;
          nvdev->source   = state->source;
     }

     if (state->modified & (SMF_CLIP | SMF_DESTINATION))
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (state->modified & SMF_DRAWING_FLAGS) {
          if (state->drawingflags == DSDRAW_BLEND)
               nvdev->drawfx = 0x00000002;
          else
               nvdev->drawfx = 0x00000000;
     }

     if (state->modified & SMF_BLITTING_FLAGS) {
          if (state->blittingflags == DSBLIT_BLEND_COLORALPHA)
               nvdev->blitfx = 0x00000002;
          else
               nvdev->blitfx = 0x00000000;
     }

     if (state->modified & (SMF_COLOR | SMF_DESTINATION)) {
          switch (state->destination->format) {
               case DSPF_ARGB1555:
                    nvdev->color = PIXEL_ARGB1555( state->color.a,
                                                   state->color.r,
                                                   state->color.g,
                                                   state->color.b );
                    break;

               case DSPF_RGB16:
                    nvdev->color = PIXEL_RGB16( state->color.r,
                                                state->color.g,
                                                state->color.b );
                    break;

               case DSPF_RGB32:
                    nvdev->color = PIXEL_RGB32( state->color.r,
                                                state->color.g,
                                                state->color.b );
                    break;

               case DSPF_ARGB:
                    nvdev->color = PIXEL_ARGB( state->color.a,
                                               state->color.r,
                                               state->color.g,
                                               state->color.b );
                    break;

               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;

               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          /* set alpha value if using transparency */
          if (nvdev->drawfx || nvdev->blitfx)
               PGRAPH[0x608/4] = state->color.a << 23;
     }

     state->modified = 0;
}

static bool nvFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData     *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev     = (NVidiaDeviceData*) dev;
     volatile NVRectangle *Rectangle = nvdrv->Rectangle;

     if (nvdev->supported_drawingflags & DSDRAW_BLEND) {
          NV_FIFO_FREE( nvdev, Rectangle, 1 )
          Rectangle->SetOperation = nvdev->drawfx;
     }

     NV_FIFO_FREE( nvdev, Rectangle, 3 )
     Rectangle->Color        = nvdev->color;
     Rectangle->TopLeft      = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight  = (rect->h << 16) | rect->w;

     return true;
}

static bool nvFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData    *nvdrv    = (NVidiaDriverData*) drv;
     NVidiaDeviceData    *nvdev    = (NVidiaDeviceData*) dev;
     volatile NVTriangle *Triangle = nvdrv->Triangle;

     if (nvdev->supported_drawingflags & DSDRAW_BLEND) {
          NV_FIFO_FREE( nvdev, Triangle, 1 )
          Triangle->SetOperation = nvdev->drawfx;
     }

     NV_FIFO_FREE( nvdev, Triangle, 4 )
     Triangle->Color          = nvdev->color;
     Triangle->TrianglePoint0 = (tri->y1 << 16) | tri->x1;
     Triangle->TrianglePoint1 = (tri->y2 << 16) | tri->x2;
     Triangle->TrianglePoint2 = (tri->y3 << 16) | tri->x3;

     return true;
}

static bool nvDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData     *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev     = (NVidiaDeviceData*) dev;
     volatile NVRectangle *Rectangle = nvdrv->Rectangle;

     if (nvdev->supported_drawingflags & DSDRAW_BLEND) {
          NV_FIFO_FREE( nvdev, Rectangle, 1 )
          Rectangle->SetOperation = nvdev->drawfx;
     }

     NV_FIFO_FREE( nvdev, Rectangle, 9 )

     Rectangle->Color        = nvdev->color;

     Rectangle->TopLeft      = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight  = (1 << 16) | rect->w;

     Rectangle->TopLeft      = ((rect->y + rect->h - 1) << 16) | rect->x;
     Rectangle->WidthHeight  = (1 << 16) | rect->w;

     Rectangle->TopLeft      = ((rect->y + 1) << 16) | rect->x;
     Rectangle->WidthHeight  = ((rect->h - 2) << 16) | 1;

     Rectangle->TopLeft      = ((rect->y + 1) << 16) | (rect->x + rect->w - 1);
     Rectangle->WidthHeight  = ((rect->h - 2) << 16) | 1;

     return true;
}

static bool nvDrawLine( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     volatile NVLine  *Line  = nvdrv->Line;

     if (nvdev->supported_drawingflags & DSDRAW_BLEND) {
          NV_FIFO_FREE( nvdev, Line, 1 )
          Line->SetOperation = nvdev->drawfx;
     }

     NV_FIFO_FREE( nvdev, Line, 3 )
     Line->Color         = nvdev->color;
     Line->Lin[0].point0 = (line->y1 << 16) | line->x1;
     Line->Lin[0].point1 = (line->y2 << 16) | line->x2;

     return true;
}

static bool nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{

     NVidiaDriverData       *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData       *nvdev       = (NVidiaDeviceData*) dev;
     volatile NVScaledImage *ScaledImage = nvdrv->ScaledImage;
     CoreSurface            *source      = nvdev->source;
     __u32                   format      = 0;
    
     switch (source->format) {
          case DSPF_ARGB1555:
               format = 0x00000002;
               break;
          /* FIXME: RGB16 */
          case DSPF_RGB32:
          case DSPF_ARGB:
               format = 0x00000004;
               break;
          case DSPF_YUY2:
               format = 0x00000005;
               break;
          case DSPF_UYVY:
               format = 0x00000006;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return false;
     }

     if (nvdev->supported_blittingflags & DSBLIT_BLEND_COLORALPHA) {
          NV_FIFO_FREE( nvdev, ScaledImage, 2 )
          ScaledImage->SetColorFormat = format;
          ScaledImage->SetOperation   = nvdev->blitfx;
     } else {
          NV_FIFO_FREE( nvdev, ScaledImage, 1 )
          ScaledImage->SetColorFormat = format;
     }

     NV_FIFO_FREE( nvdev, ScaledImage, 6 )
     ScaledImage->ClipPoint      = (dr->y << 16) | dr->x;
     ScaledImage->ClipSize       = (dr->h << 16) | dr->w;
     ScaledImage->ImageOutPoint  = (dr->y << 16) | dr->x;
     ScaledImage->ImageOutSize   = (dr->h << 16) | dr->w;
     ScaledImage->DuDx           = (sr->w << 20) / dr->w;
     ScaledImage->DvDy           = (sr->h << 20) / dr->h;

     NV_FIFO_FREE( nvdev, ScaledImage, 4 )
     ScaledImage->ImageInSize    = (source->height << 16) | source->width;
     ScaledImage->ImageInFormat  = source->front_buffer->video.pitch;
     ScaledImage->ImageInOffset  = (source->front_buffer->video.offset & 0x1FFFFFF) + 
                                   (sr->y * source->front_buffer->video.pitch)      +
				   (sr->x * DFB_BITS_PER_PIXEL( source->format ) >> 3);
     ScaledImage->ImageInPoint   = 0; /* how does it work ??... */
     
     return true;
}

static bool nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData     *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev = (NVidiaDeviceData*) dev;
     volatile NVScreenBlt *Blt   = nvdrv->Blt;
     
     if (nvdev->source->format != nvdev->destination->format) {
          DFBRectangle dr = { dx, dy, rect->w, rect->h };
          return nvStretchBlit( drv, dev, rect, &dr );
     }

     if (nvdev->supported_blittingflags & DSBLIT_BLEND_COLORALPHA) {
          NV_FIFO_FREE( nvdev, Blt, 1 )
          Blt->SetOperation = nvdev->blitfx;
     }

     NV_FIFO_FREE( nvdev, Blt, 3 )
     Blt->TopLeftSrc   = (rect->y << 16) | rect->x;
     Blt->TopLeftDst   = (dy << 16) | dx;
     Blt->WidthHeight  = (rect->h << 16) | rect->w;

     return true;
}


#if 0
#define NV_VERTEX3D( n, x, y, z, w, col, spc, s, t ) \
{\
     NV_FIFO_FREE( nvdev, TexTri, 8 );\
     TexTri->Vertex[n].sx       = x;\
     TexTri->Vertex[n].sy       = y;\
     TexTri->Vertex[n].sz       = z;\
     TexTri->Vertex[n].rhw      = w;\
     TexTri->Vertex[n].color    = col;\
     TexTri->Vertex[n].specular = spc;\
     TexTri->Vertex[n].ts       = s;\
     TexTri->Vertex[n].tt       = t;\
}

static void
nv_texture_triangle( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev,
           DFBVertex *v0, DFBVertex *v1, DFBVertex *v2 )
{
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;

     NV_VERTEX3D( 0, v0->x, v0->y, v0->z, v0->w, 0, 0, v0->s, v0->t )
     NV_VERTEX3D( 1, v1->x, v1->y, v1->z, v1->w, 0, 0, v1->s, v1->t )
     NV_VERTEX3D( 2, v2->x, v2->y, v2->z, v2->w, 0, 0, v2->s, v2->t )

     NV_FIFO_FREE( nvdev, TexTri, 1 );

     TexTri->DrawTriangle3D = (2 << 8) | (1 << 4) | 0;
}
#endif

static bool
nvTextureTriangles( void *drv, void *dev, DFBVertex *vertices,
                    int num, DFBTriangleFormation formation )
{
#if 0
     NVidiaDriverData              *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData              *nvdev  = (NVidiaDeviceData*) dev;
     volatile NVTexturedTriangle05 *TexTri = nvdrv->TexTri;
     int                            wl2    = direct_log2( nvdev->source->width );
     int                            hl2    = direct_log2( nvdev->source->height );
     int                            i;

     NV_FIFO_FREE( nvdev, TexTri, 7 );
     TexTri->ColorKeyValue = 0;
     TexTri->TextureOffset = nvdev->source->front_buffer->video.offset & 0x1FFFFFFF;
     TexTri->TextureFormat = (wl2 << 16) | (hl2 << 20);
     TexTri->TextureFilter = 0;
     TexTri->Blend         = 0;
     TexTri->Control       = 0;
     TexTri->FogColor      = 0xffffffff;
 
     switch (formation)
     {
          case DTTF_LIST:
               for (i = 0; i < num; i += 3)
                    nv_texture_triangle( nvdrv, nvdev,
                         &vertices[i], &vertices[i+1], &vertices[i+2] );
               break;

          case DTTF_STRIP:
               nv_texture_triangle( nvdrv, nvdev, 
                         &vertices[0], &vertices[1], &vertices[2] );
               for (i = 0; i < num; i++)
                    nv_texture_triangle( nvdrv, nvdev, 
                         &vertices[i-2], &vertices[i-1], &vertices[i] );
               break;

          case DTTF_FAN:
          nv_texture_triangle( nvdrv, nvdev,
                         &vertices[0], &vertices[1], &vertices[2] );
               for (i = 0; i < num; i++)
                    nv_texture_triangle( nvdrv, nvdev,
                         &vertices[0], &vertices[i-1], &vertices[i] );
               break;

          default:
               return false;
     }
#endif
     return true;
}
     

static void nvAfterSetVar( void *drv, void *dev )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     volatile __u32   *PRAMIN = nvdrv->PRAMIN;
     volatile __u32   *FIFO   = nvdrv->FIFO;
     
     /* write objects configuration */
     NV_LOAD_TABLE( PRAMIN, PRAMIN )

     /* set objects pixelformat */
     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_ARGB1555:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB1555 )
               break;
          case DSPF_RGB16:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB16 )
               break;
          case DSPF_RGB32:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
               break;
          case DSPF_ARGB:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB )
               break;
          default:
               break;
     }

     /* put objects into subchannels */
     NV_LOAD_TABLE( FIFO, FIFO )
}


/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_NV4:
          case FB_ACCEL_NV5:
               return 1;
     }

     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "nVidia TNT/TNT2/GeForce Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 2;

     info->driver_data_size = sizeof (NVidiaDriverData);
     info->device_data_size = sizeof (NVidiaDeviceData);
}


#define PCI_LIST  "/proc/bus/pci/devices"

static void
nv_find_architecture( NVidiaDriverData *nvdrv )
{
     FILE *fp;
     char  buf[512];
     __u32 device;
     __u32 vendor;
     __u32 model;
     __u32 arch = 0;

     /* TODO: sysfs support */

     fp = fopen( PCI_LIST, "r" );
     if (!fp) {
          D_PERROR( "DirectFB/NVidia: couldn't access " PCI_LIST );
          return;
     }

     while (fgets( buf, 512, fp )) {
          if (sscanf( buf, "%04x\t%04x%04x", &device, &vendor, &model ) != 3)
               continue;
          if (vendor != 0x10DE )
               continue;

          switch (model) {
               case 0x0020:
                    arch = NV_ARCH04;
                    break;
               case 0x0028 ... 0x00A0:
                    arch = NV_ARCH05;
                    break;
               case 0x0100 ... 0x0103:
                    arch = NV_ARCH10;
                    break;
               case 0x0110 ... 0x0153:
                    arch = NV_ARCH11;
                    break;
               case 0x0200 ... 0x0203:
                    arch = NV_ARCH20;
                    break;
               default:
                    break;
          }

          if (arch) {
               nvdrv->arch = arch;
               D_INFO( "DirectFB/NVidia: "
                       "found nVidia Architecture %02x\n", arch );
          } else
               D_INFO( "DirectFB/NVidia: "
                       "assuming nVidia Architecture 04\n" );

          break;
     }

     fclose( fp );
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     nvdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;

     nvdrv->device = device;
     nvdrv->arch   = NV_ARCH04;

     nv_find_architecture( nvdrv );

     nvdrv->PVIDEO = (volatile __u32*) (nvdrv->mmio_base + 0x008000);
     nvdrv->PFB    = (volatile __u32*) (nvdrv->mmio_base + 0x100000);
     nvdrv->PGRAPH = (volatile __u32*) (nvdrv->mmio_base + 0x400000);
     nvdrv->PRAMIN = (volatile __u32*) (nvdrv->mmio_base + 0x710000);
     nvdrv->FIFO   = (volatile __u32*) (nvdrv->mmio_base + 0x800000);

     nvdrv->Rop         = (volatile NVRop                *) &nvdrv->FIFO[0x0000/4];
     nvdrv->Clip        = (volatile NVClip               *) &nvdrv->FIFO[0x2000/4];
     nvdrv->Pattern     = (volatile NVPattern            *) &nvdrv->FIFO[0x4000/4];
     nvdrv->Triangle    = (volatile NVTriangle           *) &nvdrv->FIFO[0x6000/4];
     nvdrv->ScaledImage = (volatile NVScaledImage        *) &nvdrv->FIFO[0x8000/4];
     nvdrv->Rectangle   = (volatile NVRectangle          *) &nvdrv->FIFO[0xA000/4];
     nvdrv->Line        = (volatile NVLine               *) &nvdrv->FIFO[0xC000/4];
     nvdrv->Blt         = (volatile NVScreenBlt          *) &nvdrv->FIFO[0xE000/4];
     //nvdrv->TexTri      = (volatile NVTexturedTriangle05 *) &nvdrv->FIFO[0xE000/4];
     
     funcs->CheckState    = nvCheckState;
     funcs->SetState      = nvSetState;
     funcs->EngineSync    = nvEngineSync;
     funcs->AfterSetVar   = nvAfterSetVar;

     funcs->FillRectangle    = nvFillRectangle;
     funcs->FillTriangle     = nvFillTriangle;
     funcs->DrawRectangle    = nvDrawRectangle;
     funcs->DrawLine         = nvDrawLine;
     funcs->Blit             = nvBlit;
     funcs->StretchBlit      = nvStretchBlit;
     funcs->TextureTriangles = nvTextureTriangles;

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &nvidiaOverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "TNT/TNT2/GeForce" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nVidia" );

     if (nvdrv->arch >= NV_ARCH05) {
          nvdev->supported_drawingflags      = NV5_SUPPORTED_DRAWINGFLAGS;
          nvdev->supported_drawingfunctions  = NV5_SUPPORTED_DRAWINGFUNCTIONS;
          nvdev->supported_blittingflags     = NV5_SUPPORTED_BLITTINGFLAGS;
          nvdev->supported_blittingfunctions = NV5_SUPPORTED_BLITTINGFUNCTIONS;
     } else {
          nvdev->supported_drawingflags      = NV4_SUPPORTED_DRAWINGFLAGS;
          nvdev->supported_drawingfunctions  = NV4_SUPPORTED_DRAWINGFUNCTIONS;
          nvdev->supported_blittingflags     = NV4_SUPPORTED_BLITTINGFLAGS;
          nvdev->supported_blittingfunctions = NV4_SUPPORTED_BLITTINGFUNCTIONS;
     }

     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = nvdev->supported_drawingfunctions |
                                  nvdev->supported_blittingfunctions;
     device_info->caps.drawing  = nvdev->supported_drawingflags;
     device_info->caps.blitting = nvdev->supported_blittingflags;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     dfb_config->pollvsync_after = 1;
 
     /* set video memory start and limit */
     nvdrv->PVIDEO[0x920/4] = 0;
     nvdrv->PVIDEO[0x924/4] = 0;
     nvdrv->PVIDEO[0x908/4] = dfb_system_videoram_length() - 1;
     nvdrv->PVIDEO[0x90C/4] = dfb_system_videoram_length() - 1;
     //nvdrv->PGRAPH[0x684/4] = dfb_system_videoram_length() - 1;
     //nvdrv->PGRAPH[0x688/4] = dfb_system_videoram_length() - 1;

     /* set default Rop3 */
     nvdrv->PGRAPH[0x604/4] = 0x000000CC; /* copy */

     /* set default Beta (alpha) */
     nvdrv->PGRAPH[0x608/4] = 0x7F800000;
     
     /* set default pattern */
     nvdrv->PGRAPH[0x800/4] = 0xFFFFFFFF; /* Color0 */
     nvdrv->PGRAPH[0x804/4] = 0xFFFFFFFF; /* Color1 */
     nvdrv->PGRAPH[0x810/4] = 0x00000000; /* Shape */
 
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     (void) nvdev;

     D_DEBUG( "DirectFB/NVidia: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/NVidia:  %9d nv_waitfifo calls\n",
               nvdev->waitfifo_calls );
     D_DEBUG( "DirectFB/NVidia:  %9d register writes (nv_waitfifo sum)\n",
               nvdev->waitfifo_sum );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nvdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nvdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/NVidia: Conclusion:\n" );
     D_DEBUG( "DirectFB/NVidia:  Average register writes/nvidia_waitfifo"
               "call:%.2f\n",
               nvdev->waitfifo_sum/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/NVidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nvdev->fifo_waitcycles/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/NVidia:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * nvdev->fifo_cache_hits/
               (float)(nvdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, nvdrv->mmio_base, -1 );
}


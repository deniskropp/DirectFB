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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( tdfx )

#include "tdfx.h"

static void tdfxFillRectangle2D( void *drv, void *dev, DFBRectangle *rect );
static void tdfxFillRectangle3D( void *drv, void *dev, DFBRectangle *rect );
static void tdfxFillTriangle2D ( void *drv, void *dev, DFBTriangle  *tri );
static void tdfxFillTriangle3D ( void *drv, void *dev, DFBTriangle  *tri );
static void tdfxDrawLine2D     ( void *drv, void *dev, DFBRegion    *line );
//static void tdfxDrawLine3D     ( void *drv, void *dev, DFBRegion    *line );

typedef struct {
     /* for fifo/performance monitoring */
     unsigned int fifo_space;

     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;

     /* state validation */
     int v_destination2D;
     int v_destination3D;
     int v_color1;
     int v_colorFore;
     int v_alphaMode;
     int v_source2D;
     int v_srcColorkey;
     int v_commandExtra;
} TDFXDeviceData;

typedef struct {
     volatile __u8 *mmio_base;
     Voodoo2D      *voodoo2D;
     Voodoo3D      *voodoo3D;
} TDFXDriverData;



static inline void tdfx_waitfifo( TDFXDriverData *tdrv,
                                  TDFXDeviceData *tdev, int space )
{
     int timeout = 1000000;

     tdev->waitfifo_calls++;
     tdev->waitfifo_sum += space;

     if (tdev->fifo_space < space) {
          while (timeout--) {
               tdev->fifo_waitcycles++;

               tdev->fifo_space = (tdrv->voodoo2D->status & 0x3f);
               if (tdev->fifo_space >= space)
                    break;

          }
     }
     else {
          tdev->fifo_cache_hits++;
     }

     tdev->fifo_space -= space;

     if (!timeout)
          CAUTION( "timeout during waitfifo!" );
}

static inline void tdfx_waitidle( TDFXDriverData *tdrv,
                                  TDFXDeviceData *tdev )
{
     int i = 0;
     int timeout = 1000000;

//     tdfx_waitfifo( tdrv, tdev, 1 );

//     voodoo3D->nopCMD = 0;

     while (timeout--) {
          tdev->idle_waitcycles++;

          i = (tdrv->voodoo2D->status & (0xF << 7))  ?  0 : i + 1;
          if(i == 3)
               return;

     }

     BUG( "timeout during waitidle!\n");
}


static int blitFormat[] = {
     2, /* DSPF_RGB15 */
     3, /* DSPF_RGB16 */
     4, /* DSPF_RGB24 */
     5, /* DSPF_RGB32 */
     5, /* DSPF_ARGB  */
     0  /* DSPF_A8    */
};

static inline void tdfx_validate_source2D( TDFXDriverData *tdrv,
                                           TDFXDeviceData *tdev,
                                           CardState      *state )
{
     CoreSurface   *source   = state->source;
     SurfaceBuffer *buffer   = source->front_buffer;
     Voodoo2D      *voodoo2D = tdrv->voodoo2D;

     if (tdev->v_source2D)
          return;

     tdfx_waitfifo( tdrv, tdev, 2 );

     voodoo2D->srcBaseAddr = buffer->video.offset & 0xFFFFFF;
     voodoo2D->srcFormat   = (buffer->video.pitch & 0x3FFF) |
                             (blitFormat[DFB_PIXELFORMAT_INDEX(source->format)] << 16);

     tdev->v_source2D = 1;
}

static inline void tdfx_validate_destination2D( TDFXDriverData *tdrv,
                                                TDFXDeviceData *tdev,
                                                CardState      *state )
{
     CoreSurface   *destination = state->destination;
     SurfaceBuffer *buffer      = destination->back_buffer;
     Voodoo2D      *voodoo2D    = tdrv->voodoo2D;

     if (tdev->v_destination2D)
          return;

     tdfx_waitfifo( tdrv, tdev, 2 );

     voodoo2D->dstBaseAddr = buffer->video.offset;
     voodoo2D->dstFormat   = (buffer->video.pitch & 0x3FFF) |
                             (blitFormat[DFB_PIXELFORMAT_INDEX(destination->format)] << 16);

     tdev->v_destination2D = 1;
}

static inline void tdfx_validate_destination3D( TDFXDriverData *tdrv,
                                                TDFXDeviceData *tdev,
                                                CardState      *state )
{
     CoreSurface   *destination = state->destination;
     SurfaceBuffer *buffer      = destination->back_buffer;
     Voodoo3D      *voodoo3D    = tdrv->voodoo3D;

     __u32 lfbmode = TDFX_LFBMODE_PIXEL_PIPELINE_ENABLE;
     __u32 fbzMode = (1 << 9) | 1;

     if (tdev->v_destination3D)
          return;

     switch (destination->format) {
          case DSPF_RGB15:
               lfbmode |= TDFX_LFBMODE_RGB555;
               break;
          case DSPF_RGB16:
               lfbmode |= TDFX_LFBMODE_RGB565;
               break;
          case DSPF_RGB32:
               lfbmode |= TDFX_LFBMODE_RGB0888;
               break;
          case DSPF_ARGB:
               fbzMode |= (1 << 10);
               lfbmode |= TDFX_LFBMODE_ARGB8888;
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }

     tdfx_waitfifo( tdrv, tdev, 4 );

     voodoo3D->lfbMode = lfbmode;
     voodoo3D->fbzMode = fbzMode;
     voodoo3D->colBufferAddr = buffer->video.offset;
     voodoo3D->colBufferStride = buffer->video.pitch;

     tdev->v_destination3D = 1;
}

static inline void tdfx_validate_color1( TDFXDriverData *tdrv,
                                         TDFXDeviceData *tdev,
                                         CardState      *state )
{
     if (tdev->v_color1)
          return;

     tdfx_waitfifo( tdrv, tdev, 1 );

     tdrv->voodoo3D->color1 = PIXEL_ARGB( state->color.a,
                                          state->color.r,
                                          state->color.g,
                                          state->color.b );

     tdev->v_color1 = 1;
}

static inline void tdfx_validate_colorFore( TDFXDriverData *tdrv,
                                            TDFXDeviceData *tdev,
                                            CardState      *state )
{
     if (tdev->v_colorFore)
          return;

     tdfx_waitfifo( tdrv, tdev, 1 );

     switch (state->destination->format) {
          case DSPF_A8:
               tdrv->voodoo2D->colorFore = state->color.a;
               break;
          case DSPF_RGB15:
               tdrv->voodoo2D->colorFore = PIXEL_RGB15( state->color.r,
                                                        state->color.g,
                                                        state->color.b );
               break;
          case DSPF_RGB16:
               tdrv->voodoo2D->colorFore = PIXEL_RGB16( state->color.r,
                                                        state->color.g,
                                                        state->color.b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               tdrv->voodoo2D->colorFore = PIXEL_RGB32( state->color.r,
                                                        state->color.g,
                                                        state->color.b );
               break;
          case DSPF_ARGB:
               tdrv->voodoo2D->colorFore = PIXEL_ARGB(  state->color.a,
                                                        state->color.r,
                                                        state->color.g,
                                                        state->color.b );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }

     tdev->v_colorFore = 1;
}

static inline void tdfx_validate_alphaMode( TDFXDriverData *tdrv,
                                            TDFXDeviceData *tdev,
                                            CardState      *state )
{
     static int tdfxBlendFactor[] = {
          0,
          0x0, /* DSBF_ZERO         */
          0x4, /* DSBF_ONE          */
          0x2, /* DSBF_SRCCOLOR     */
          0x6, /* DSBF_INVSRCCOLOR  */
          0x1, /* DSBF_SRCALPHA     */
          0x5, /* DSBF_INVSRCALPHA  */
          0x3, /* DSBF_DESTALPHA    */
          0x7, /* DSBF_INVDESTALPHA */
          0x2, /* DSBF_DESTCOLOR    */
          0x6, /* DSBF_INVDESTCOLOR */
          0xF  /* DSBF_SRCALPHASAT  */
     };

     if (tdev->v_alphaMode)
          return;

     tdfx_waitfifo( tdrv, tdev, 1 );

     tdrv->voodoo3D->alphaMode = TDFX_ALPHAMODE_BLEND_ENABLE |
                                (tdfxBlendFactor[state->src_blend] <<  8) |
                                (tdfxBlendFactor[state->src_blend] << 16) |
                                (tdfxBlendFactor[state->dst_blend] << 12) |
                                (tdfxBlendFactor[state->dst_blend] << 20);

     tdev->v_alphaMode = 1;
}

static inline void tdfx_validate_srcColorkey( TDFXDriverData *tdrv,
                                              TDFXDeviceData *tdev,
                                              CardState      *state )
{
     Voodoo2D *voodoo2D = tdrv->voodoo2D;

     if (tdev->v_srcColorkey)
          return;

     tdfx_waitfifo( tdrv, tdev, 2 );

     voodoo2D->srcColorkeyMin =
     voodoo2D->srcColorkeyMax = state->src_colorkey;

     tdev->v_srcColorkey = 1;
}

static inline void tdfx_validate_commandExtra( TDFXDriverData *tdrv,
                                               TDFXDeviceData *tdev,
                                               CardState      *state )
{
     if (tdev->v_commandExtra)
          return;

     tdfx_waitfifo( tdrv, tdev, 1 );

     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          tdrv->voodoo2D->commandExtra = 1;
     else
          tdrv->voodoo2D->commandExtra = 0;

     tdev->v_commandExtra = 1;
}



static inline void tdfx_set_clip( TDFXDriverData *tdrv,
                                  TDFXDeviceData *tdev,
                                  DFBRegion      *clip )
{
     Voodoo2D *voodoo2D = tdrv->voodoo2D;
     Voodoo3D *voodoo3D = tdrv->voodoo3D;

     tdfx_waitfifo( tdrv, tdev, 4 );

     voodoo2D->clip0Min = ((clip->y1 & 0xFFF) << 16) |
                          (clip->x1 & 0xFFF);

     voodoo2D->clip0Max = (((clip->y2+1) & 0xFFF) << 16) |
                          ((clip->x2+1) & 0xFFF);

     voodoo3D->clipLeftRight = ((clip->x1 & 0xFFF) << 16) |
                               ((clip->x2+1) & 0xFFF);

     voodoo3D->clipTopBottom = ((clip->y1 & 0xFFF) << 16) |
                               ((clip->y2+1) & 0xFFF);
}



/* required implementations */

static void tdfxEngineSync( void *drv, void *dev )
{
     TDFXDriverData *tdrv = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev = (TDFXDeviceData*) dev;

     tdfx_waitidle( tdrv, tdev );
}

#define TDFX_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define TDFX_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE)

#define TDFX_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY)

#define TDFX_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


static void tdfxCheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     /* check for the special drawing function that does not support
        the usually supported drawingflags */
     if (accel == DFXL_DRAWLINE  &&  state->drawingflags != DSDRAW_NOFX)
          return;

     /* if there are no other drawing flags than the supported */
     if (!(state->drawingflags & ~TDFX_SUPPORTED_DRAWINGFLAGS))
          state->accel |= TDFX_SUPPORTED_DRAWINGFUNCTIONS;

     /* if there are no other blitting flags than the supported
        and the source and destination formats are the same */
     if (!(state->blittingflags & ~TDFX_SUPPORTED_BLITTINGFLAGS)  &&
         state->source  &&  state->source->format != DSPF_RGB24)
          state->accel |= TDFX_SUPPORTED_BLITTINGFUNCTIONS;
}

static void tdfxSetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     TDFXDriverData *tdrv = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev = (TDFXDeviceData*) dev;

     if (state->modified & SMF_DESTINATION)
          tdev->v_destination2D = tdev->v_destination3D = tdev->v_colorFore = 0;

     if (state->modified & SMF_SOURCE)
          tdev->v_source2D = 0;

     if (state->modified & (SMF_DST_BLEND | SMF_SRC_BLEND))
          tdev->v_alphaMode = 0;

     if (state->modified & SMF_COLOR)
          tdev->v_color1 = tdev->v_colorFore = 0;

     if (state->modified & SMF_SRC_COLORKEY)
          tdev->v_srcColorkey = 0;

     if (state->modified & SMF_BLITTING_FLAGS)
          tdev->v_commandExtra = 0;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    tdfx_validate_color1( tdrv, tdev, state );
                    tdfx_validate_alphaMode( tdrv, tdev, state );
                    tdfx_validate_destination3D( tdrv, tdev, state );

                    funcs->FillRectangle = tdfxFillRectangle3D;
                    funcs->FillTriangle = tdfxFillTriangle3D;
               }
               else {
                    tdfx_validate_colorFore( tdrv, tdev, state );
                    tdfx_validate_destination2D( tdrv, tdev, state );

                    funcs->FillRectangle = tdfxFillRectangle2D;
                    funcs->FillTriangle = tdfxFillTriangle2D;
               }

               state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    tdfx_validate_srcColorkey( tdrv, tdev, state );

               tdfx_validate_commandExtra( tdrv, tdev, state );
               tdfx_validate_source2D( tdrv, tdev, state );
               tdfx_validate_destination2D( tdrv, tdev, state );

               state->set |= DFXL_BLIT | DFXL_STRETCHBLIT;
               break;

          default:
               BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (state->modified & SMF_CLIP)
          tdfx_set_clip( tdrv, tdev, &state->clip );

     state->modified = 0;
}

static void tdfxFillRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;

     tdfx_waitfifo( tdrv, tdev, 3 );

     voodoo2D->dstXY   = ((rect->y & 0x1FFF) << 16) | (rect->x & 0x1FFF);
     voodoo2D->dstSize = ((rect->h & 0x1FFF) << 16) | (rect->w & 0x1FFF);

     voodoo2D->command = 5 | (1 << 8) | (0xCC << 24);
}

static void tdfxFillRectangle3D( void *drv, void *dev, DFBRectangle *rect )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo3D       *voodoo3D = tdrv->voodoo3D;

     tdfx_waitfifo( tdrv, tdev, 10 );

     voodoo3D->vertexAx = S12_4(rect->x);
     voodoo3D->vertexAy = S12_4(rect->y);

     voodoo3D->vertexBx = S12_4(rect->x);
     voodoo3D->vertexBy = S12_4(rect->y + rect->h);

     voodoo3D->vertexCx = S12_4(rect->x + rect->w);
     voodoo3D->vertexCy = S12_4(rect->y + rect->h);

     voodoo3D->triangleCMD = (1 << 31);


     voodoo3D->vertexBx = S12_4(rect->x + rect->w);
     voodoo3D->vertexBy = S12_4(rect->y);

     voodoo3D->triangleCMD = 0;
}

static void tdfxDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
}

static void tdfxDrawLine2D( void *drv, void *dev, DFBRegion *line )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;

     tdfx_waitfifo( tdrv, tdev, 5 );

     voodoo2D->srcXY   = ((line->y1 & 0x1FFF) << 16) | (line->x1 & 0x1FFF);
     voodoo2D->dstXY   = ((line->y2 & 0x1FFF) << 16) | (line->x2 & 0x1FFF);
     voodoo2D->command = 6 | (1 << 8) | (0xCC << 24);
}

/*static void tdfxDrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     int xl, xr, yb, yt;

     if (line->x1 < line->x2) {
          xl = -8;
          xr =  8;
     }
     else {
          xl =  8;
          xr = -8;
     }

     if (line->y1 < line->y2) {
          yt = -8;
          yb =  8;
     }
     else {
          yt =  8;
          yb = -8;
     }

     tdfx_waitfifo( 10 );

     voodoo3D->vertexAx = S12_4_( line->x1, xl );
     voodoo3D->vertexAy = S12_4_( line->y1, yt );

     voodoo3D->vertexBx = S12_4_( line->x2, xl );
     voodoo3D->vertexBy = S12_4_( line->y2, yb );

     voodoo3D->vertexCx = S12_4_( line->x2, xr );
     voodoo3D->vertexCy = S12_4_( line->y2, yb );

     voodoo3D->triangleCMD = (1 << 31);


     voodoo3D->vertexBx = S12_4_( line->x1, xr );
     voodoo3D->vertexBy = S12_4_( line->y1, yt );

     voodoo3D->triangleCMD = 0;
}*/

static void tdfxFillTriangle2D( void *drv, void *dev, DFBTriangle *tri )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;

     tdfx_waitfifo( tdrv, tdev, 7 );

     dfb_sort_triangle( tri );

     voodoo2D->srcXY = ((tri->y1 & 0x1FFF) << 16) | (tri->x1 & 0x1FFF);
     voodoo2D->command = 8 | (1 << 8) | (0xCC << 24);

     if (tri->x2 < tri->x3) {
        voodoo2D->launchArea[0] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
        voodoo2D->launchArea[1] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
        voodoo2D->launchArea[2] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);

     }
     else {
        voodoo2D->launchArea[0] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
        voodoo2D->launchArea[1] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
        voodoo2D->launchArea[2] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);

    }

}

static void tdfxFillTriangle3D( void *drv, void *dev, DFBTriangle *tri )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo3D       *voodoo3D = tdrv->voodoo3D;

     tdfx_waitfifo( tdrv, tdev, 7 );

     dfb_sort_triangle( tri );

     voodoo3D->vertexAx = S12_4(tri->x1);
     voodoo3D->vertexAy = S12_4(tri->y1);

     voodoo3D->vertexBx = S12_4(tri->x2);
     voodoo3D->vertexBy = S12_4(tri->y2);

     voodoo3D->vertexCx = S12_4(tri->x3);
     voodoo3D->vertexCy = S12_4(tri->y3);

     voodoo3D->triangleCMD = (1 << 31);

     voodoo3D->triangleCMD = 0;
}

static void tdfxBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;

     __u32 cmd = 1 | (1 <<8) | (0xCC << 24);//SST_2D_GO | SST_2D_SCRNTOSCRNBLIT | (ROP_COPY << 24);

     if (rect->x <= dx) {
          cmd |= (1 << 14);//SST_2D_X_RIGHT_TO_LEFT;
          rect->x += rect->w-1;
          dx += rect->w-1;
     }
     if (rect->y <= dy) {
          cmd |= (1 << 15);//SST_2D_Y_BOTTOM_TO_TOP;
          rect->y += rect->h-1;
          dy += rect->h-1;
     }


     tdfx_waitfifo( tdrv, tdev, 4 );

     voodoo2D->srcXY   = ((rect->y & 0x1FFF) << 16) | (rect->x & 0x1FFF);
     voodoo2D->dstXY   = ((dy      & 0x1FFF) << 16) | (dx      & 0x1FFF);
     voodoo2D->dstSize = ((rect->h & 0x1FFF) << 16) | (rect->w & 0x1FFF);

     voodoo2D->command = cmd;
}

static void tdfxStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) drv;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) dev;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;

     tdfx_waitfifo( tdrv, tdev, 5 );

     voodoo2D->srcXY   = ((sr->y & 0x1FFF) << 16) | (sr->x & 0x1FFF);
     voodoo2D->srcSize = ((sr->h & 0x1FFF) << 16) | (sr->w & 0x1FFF);

     voodoo2D->dstXY   = ((dr->y & 0x1FFF) << 16) | (dr->x & 0x1FFF);
     voodoo2D->dstSize = ((dr->h & 0x1FFF) << 16) | (dr->w & 0x1FFF);

     voodoo2D->command = 2 | (1 << 8) | (0xCC << 24);
}

/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_GRAPHICS_DRIVER_ABI_VERSION;
}

static int
driver_probe( GraphicsDevice *device )
{
#ifdef FB_ACCEL_3DFX_BANSHEE
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_3DFX_BANSHEE:          /* Banshee/Voodoo3 */
               return 1;
     }
#endif
     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "3Dfx Voodoo 3/4/5/Banshee Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof (TDFXDriverData);
     info->device_data_size = sizeof (TDFXDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data )
{
     TDFXDriverData *tdrv = (TDFXDriverData*) driver_data;

     tdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!tdrv->mmio_base)
          return DFB_IO;

     tdrv->voodoo2D = (Voodoo2D*)(tdrv->mmio_base + 0x100000);
     tdrv->voodoo3D = (Voodoo3D*)(tdrv->mmio_base + 0x200000);

     funcs->CheckState    = tdfxCheckState;
     funcs->SetState      = tdfxSetState;
     funcs->EngineSync    = tdfxEngineSync;

     funcs->DrawRectangle = tdfxDrawRectangle;
     funcs->DrawLine      = tdfxDrawLine2D;
     funcs->Blit          = tdfxBlit;
     funcs->StretchBlit   = tdfxStretchBlit;

     return DFB_OK;
}


static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     TDFXDriverData *tdrv     = (TDFXDriverData*) driver_data;
     TDFXDeviceData *tdev     = (TDFXDeviceData*) device_data;
     Voodoo2D       *voodoo2D = tdrv->voodoo2D;
     Voodoo3D       *voodoo3D = tdrv->voodoo3D;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Voodoo 3/4/5/Banshee" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "3Dfx" );


     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = TDFX_SUPPORTED_DRAWINGFUNCTIONS |
                                  TDFX_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = TDFX_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = TDFX_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;


     /* initialize card */
     voodoo2D->status = 0;
     voodoo3D->nopCMD = 3;

     tdfx_waitfifo( tdrv, tdev, 6 );

     voodoo3D->clipLeftRight1 = 0;
     voodoo3D->clipTopBottom1 = 0;

     voodoo3D->fbzColorPath = TDFX_FBZCOLORPATH_RGBSELECT_COLOR1 |
                              TDFX_FBZCOLORPATH_ASELECT_COLOR1;

     voodoo3D->textureMode = 0;

     voodoo2D->commandExtra = 0;
     voodoo2D->rop = 0xAAAAAA;

     tdfx_waitfifo( tdrv, tdev, 1 );           /* VOODOO !!!  */
     *((__u32*)((__u8*) tdrv->mmio_base + 0x10c)) = 1 << 4 | 1 << 8 | 5 << 12 |
                                                    1 << 18 | 5 << 24;

     dfb_config->pollvsync_after = 1;

     return DFB_OK;
}

static DFBResult
driver_init_layers( void *driver_data,
                    void *device_data )
{
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     TDFXDeviceData *tdev = (TDFXDeviceData*) device_data;
     TDFXDriverData *tdrv = (TDFXDriverData*) driver_data;

     (void) tdev;
     (void) tdrv;

     DEBUGMSG( "DirectFB/TDFX: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/TDFX:  %9d tdfx_waitfifo calls\n",
               tdev->waitfifo_calls );
     DEBUGMSG( "DirectFB/TDFX:  %9d register writes (tdfx_waitfifo sum)\n",
               tdev->waitfifo_sum );
     DEBUGMSG( "DirectFB/TDFX:  %9d FIFO wait cycles (depends on CPU)\n",
               tdev->fifo_waitcycles );
     DEBUGMSG( "DirectFB/TDFX:  %9d IDLE wait cycles (depends on CPU)\n",
               tdev->idle_waitcycles );
     DEBUGMSG( "DirectFB/TDFX:  %9d FIFO space cache hits(depends on CPU)\n",
               tdev->fifo_cache_hits );
     DEBUGMSG( "DirectFB/TDFX: Conclusion:\n" );
     DEBUGMSG( "DirectFB/TDFX:  Average register writes/tdfx_waitfifo"
               "call:%.2f\n",
               tdev->waitfifo_sum/(float)(tdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/TDFX:  Average wait cycles/tdfx_waitfifo call:"
               " %.2f\n",
               tdev->fifo_waitcycles/(float)(tdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/TDFX:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * tdev->fifo_cache_hits/
               (float)(tdev->waitfifo_calls)) );

     DEBUGMSG( "DirectFB/TDFX:  Pixels Out: %d\n", tdrv->voodoo3D->fbiPixelsOut );
     DEBUGMSG( "DirectFB/TDFX:  Triangles Out: %d\n", tdrv->voodoo3D->fbiTrianglesOut );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     TDFXDriverData *tdrv = (TDFXDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, tdrv->mmio_base, -1 );
}


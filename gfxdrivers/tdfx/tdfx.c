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
#include <core/gfxcard.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include "tdfx.h"

static void tdfxFillRectangle2D( DFBRectangle *rect );
static void tdfxFillRectangle3D( DFBRectangle *rect );
static void tdfxFillTriangle2D( DFBTriangle *tri );
static void tdfxFillTriangle3D( DFBTriangle *tri );
static void tdfxDrawLine2D( DFBRegion *line );
//static void tdfxDrawLine3D( DFBRegion *line );

/* for fifo/performance monitoring */
static unsigned int tdfx_fifo_space = 0;

static unsigned int tdfx_waitfifo_sum    = 0;
static unsigned int tdfx_waitfifo_calls  = 0;
static unsigned int tdfx_fifo_waitcycles = 0;
static unsigned int tdfx_idle_waitcycles = 0;
static unsigned int tdfx_fifo_cache_hits = 0;


static volatile __u8 *mmio_base = NULL;
static Voodoo2D      *voodoo2D = NULL;
static Voodoo3D      *voodoo3D = NULL;
static GfxCard       *tdfx = NULL;



static inline void tdfx_waitfifo( int requested_fifo_space )
{
     int timeout = 1000000;
     
     tdfx_waitfifo_calls++;
     tdfx_waitfifo_sum += requested_fifo_space;
     
     if (tdfx_fifo_space < requested_fifo_space) {
          while (timeout--) {
               tdfx_fifo_waitcycles++;

               tdfx_fifo_space = (voodoo2D->status & 0x3f);
               if (tdfx_fifo_space >= requested_fifo_space)
                    break;
               
          }
     }
     else {
          tdfx_fifo_cache_hits++;
     }

     tdfx_fifo_space -= requested_fifo_space;
     
     if (!timeout) 
          BUG( "timeout during waitfifo!\n");
}

static inline void tdfx_waitidle()
{           
     int i = 0;
     int timeout = 1000000;
     
//     tdfx_waitfifo( 1 );
     
//     voodoo3D->nopCMD = 0;
          
     while (timeout--) {
          tdfx_idle_waitcycles++;
          
          i = (voodoo2D->status & (0xF << 7))  ?  0 : i + 1;
          if(i == 3)
               return;
          
     }

     BUG( "timeout during waitidle!\n");              
}


/* state validation */

static int v_destination2D = 0;
static int v_destination3D = 0;
static int v_color1 = 0;
static int v_colorFore = 0;
static int v_alphaMode = 0;
static int v_source2D = 0;
static int v_srcColorkey = 0;
static int v_commandExtra = 0;

     
static int blitFormat[] = {
     2, /* DSPF_RGB15 */
     3, /* DSPF_RGB16 */
     4, /* DSPF_RGB24 */
     5, /* DSPF_RGB32 */     
     5, /* DSPF_ARGB  */     
     0  /* DSPF_A8    */
};

static inline void tdfx_validate_source2D()
{

     CoreSurface *source = tdfx->state->source;
     SurfaceBuffer *buffer = source->front_buffer;
     
     if (v_source2D)
          return;

     tdfx_waitfifo( 2 );

     voodoo2D->srcBaseAddr = buffer->video.offset & 0xFFFFFF;
     voodoo2D->srcFormat   = (buffer->video.pitch & 0x3FFF) | 
                             (blitFormat[PIXELFORMAT_INDEX(source->format)] << 16);

     v_source2D = 1;
}

static inline void tdfx_validate_destination2D()
{
     CoreSurface *destination = tdfx->state->destination;
     SurfaceBuffer *buffer = destination->back_buffer;

     if (v_destination2D)
          return;
     
     tdfx_waitfifo( 2 );

     voodoo2D->dstBaseAddr = buffer->video.offset;
     voodoo2D->dstFormat   = (buffer->video.pitch & 0x3FFF) | 
                             (blitFormat[PIXELFORMAT_INDEX(destination->format)] << 16);
     
     v_destination2D = 1;
}

static inline void tdfx_validate_destination3D()
{
     CoreSurface *destination = tdfx->state->destination;
     SurfaceBuffer *buffer = destination->back_buffer;

     __u32 lfbmode = TDFX_LFBMODE_PIXEL_PIPELINE_ENABLE;
     __u32 fbzMode = (1 << 9) | 1;

     if (v_destination3D)
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
     
     tdfx_waitfifo( 4 );
     
     voodoo3D->lfbMode = lfbmode;
     voodoo3D->fbzMode = fbzMode;
     voodoo3D->colBufferAddr = buffer->video.offset;
     voodoo3D->colBufferStride = buffer->video.pitch;
     
     v_destination3D = 1;
}

static inline void tdfx_validate_color1()
{
     if (v_color1)
          return;

     tdfx_waitfifo( 1 );

     voodoo3D->color1 = PIXEL_ARGB( tdfx->state->color.a,
                                    tdfx->state->color.r,
                                    tdfx->state->color.g,
                                    tdfx->state->color.b );

     v_color1 = 1;
}

static inline void tdfx_validate_colorFore()
{
     if (v_colorFore)
          return;

     tdfx_waitfifo( 1 );

     switch (tdfx->state->destination->format) {
          case DSPF_A8:
               voodoo2D->colorFore = tdfx->state->color.a;
               break;
          case DSPF_RGB15:
               voodoo2D->colorFore = PIXEL_RGB15( tdfx->state->color.r,
                                                  tdfx->state->color.g,
                                                  tdfx->state->color.b );
               break;
          case DSPF_RGB16:
               voodoo2D->colorFore = PIXEL_RGB16( tdfx->state->color.r,
                                                  tdfx->state->color.g,
                                                  tdfx->state->color.b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:                              
               voodoo2D->colorFore = PIXEL_RGB32( tdfx->state->color.r,
                                                  tdfx->state->color.g,
                                                  tdfx->state->color.b );
               break;
          case DSPF_ARGB:
               voodoo2D->colorFore = PIXEL_ARGB(  tdfx->state->color.a,
                                                  tdfx->state->color.r,
                                                  tdfx->state->color.g,
                                                  tdfx->state->color.b );
               break;
          default:
               BUG( "unexpected pixelformat!" );
               break;
     }

     v_colorFore = 1;
}

static inline void tdfx_validate_alphaMode()
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

     if (v_alphaMode)
          return;

     tdfx_waitfifo( 1 );

     voodoo3D->alphaMode = TDFX_ALPHAMODE_BLEND_ENABLE |
                         (tdfxBlendFactor[tdfx->state->src_blend] <<  8) |
                         (tdfxBlendFactor[tdfx->state->src_blend] << 16) |
                         (tdfxBlendFactor[tdfx->state->dst_blend] << 12) |
                         (tdfxBlendFactor[tdfx->state->dst_blend] << 20);
     
     v_alphaMode = 1;
}

static inline void tdfx_validate_srcColorkey()
{
     if (v_srcColorkey)
          return;

     tdfx_waitfifo( 2 );

     voodoo2D->srcColorkeyMin =
     voodoo2D->srcColorkeyMax = tdfx->state->src_colorkey;

     v_srcColorkey = 1;
}

static inline void tdfx_validate_commandExtra()
{
     if (v_commandExtra)
          return;

     tdfx_waitfifo( 1 );

     if (tdfx->state->blittingflags & DSBLIT_SRC_COLORKEY)
          voodoo2D->commandExtra = 1;
     else
          voodoo2D->commandExtra = 0;

     v_commandExtra = 1;
}



static inline void tdfx_set_clip()
{
     DFBRegion *clip = &tdfx->state->clip;

     tdfx_waitfifo( 4 );

     voodoo2D->clip0Min = ((clip->y1 & 0xFFF) << 16) |
                          (clip->x1 & 0xFFF);

     voodoo2D->clip0Max = (((clip->y2+1) & 0xFFF) << 16) |
                          ((clip->x2+1) & 0xFFF);
     
     voodoo3D->clipLeftRight = ((clip->x1 & 0xFFF) << 16) |
                               ((clip->x2+1) & 0xFFF);

     voodoo3D->clipTopBottom = ((clip->y1 & 0xFFF) << 16) |
                               ((clip->y2+1) & 0xFFF);

     tdfx->state->modified &= ~SMF_CLIP;
}



/* required implementations */

static void tdfxEngineSync()
{
     tdfx_waitidle( mmio_base );
}

#define TDFX_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define TDFX_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE)

#define TDFX_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY)

#define TDFX_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


static void tdfxCheckState( CardState *state, DFBAccelerationMask accel )
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

static void tdfxSetState( CardState *state, DFBAccelerationMask accel )
{
     if (state != tdfx->state) {
          state->modified |= SMF_ALL;
          state->set = 0;
          tdfx->state = state;
     
          v_color1 = v_colorFore = v_srcColorkey = v_alphaMode = v_source2D =
               v_destination2D = v_destination3D = v_commandExtra = 0;
     }
     else {
          if (state->modified & SMF_DESTINATION)
               v_destination2D = v_destination3D = v_colorFore = 0;
          
          if (state->modified & SMF_SOURCE)
               v_source2D = 0;

          if (state->modified & (SMF_DST_BLEND | SMF_SRC_BLEND))
               v_alphaMode = 0;

          if (state->modified & SMF_COLOR)
               v_color1 = v_colorFore = 0;

          if (state->modified & SMF_SRC_COLORKEY)
               v_srcColorkey = 0;
     
          if (state->modified & SMF_BLITTING_FLAGS)
               v_commandExtra = 0;
     }
          
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    tdfx_validate_color1();
                    tdfx_validate_alphaMode();
                    tdfx_validate_destination3D();
                    
                    tdfx->FillRectangle = tdfxFillRectangle3D;
                    tdfx->FillTriangle = tdfxFillTriangle3D;
               }
               else {
                    tdfx_validate_colorFore();
                    tdfx_validate_destination2D();

                    tdfx->FillRectangle = tdfxFillRectangle2D;
                    tdfx->FillTriangle = tdfxFillTriangle2D;
               }

               state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    tdfx_validate_srcColorkey();
               
               tdfx_validate_commandExtra();
               tdfx_validate_source2D();
               tdfx_validate_destination2D();

               state->set |= DFXL_BLIT | DFXL_STRETCHBLIT;
               break;

          case DFXL_DRAWSTRING:
          case DFXL_ALL:
               /* these are phony acceleration functions */
          default:
               BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (state->modified & SMF_CLIP)
          tdfx_set_clip();

     state->modified = 0;
}

static void tdfxFillRectangle2D( DFBRectangle *rect )
{
     tdfx_waitfifo( 3 );

     voodoo2D->dstXY   = ((rect->y & 0x1FFF) << 16) | (rect->x & 0x1FFF);
     voodoo2D->dstSize = ((rect->h & 0x1FFF) << 16) | (rect->w & 0x1FFF);

     voodoo2D->command = 5 | (1 << 8) | (0xCC << 24);
}

static void tdfxFillRectangle3D( DFBRectangle *rect )
{
     tdfx_waitfifo( 10 );

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

static void tdfxDrawRectangle( DFBRectangle *rect )
{
}

static void tdfxDrawLine2D( DFBRegion *line )
{
     tdfx_waitfifo( 5 );

     voodoo2D->srcXY   = ((line->y1 & 0x1FFF) << 16) | (line->x1 & 0x1FFF);
     voodoo2D->dstXY   = ((line->y2 & 0x1FFF) << 16) | (line->x2 & 0x1FFF);
     voodoo2D->command = 6 | (1 << 8) | (0xCC << 24);
}

/*static void tdfxDrawLine3D( DFBRegion *line )
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

static void tdfxFillTriangle2D( DFBTriangle *tri )
{
     tdfx_waitfifo( 7 );

     sort_triangle( tri );

     voodoo2D->srcXY = ((tri->y1 & 0x1FFF) << 16) | (tri->x1 & 0x1FFF);
     voodoo2D->command = 8 | (1 << 8) | (0xCC << 24);

     if (tri->y2 == tri->y3) {
          if (tri->x2 < tri->x3) {
               voodoo2D->launchArea[0] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
               voodoo2D->launchArea[1] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
               voodoo2D->launchArea[2] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
          }
          else {
               voodoo2D->launchArea[0] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
               voodoo2D->launchArea[1] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
               voodoo2D->launchArea[2] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
          }
     }
     else {
          voodoo2D->launchArea[0] = ((tri->y2 & 0x1FFF) << 16) | (tri->x2 & 0x1FFF);
          voodoo2D->launchArea[1] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
          voodoo2D->launchArea[2] = ((tri->y3 & 0x1FFF) << 16) | (tri->x3 & 0x1FFF);
     }
}

static void tdfxFillTriangle3D( DFBTriangle *tri )
{
     tdfx_waitfifo( 7 );

     sort_triangle( tri );

     voodoo3D->vertexAx = S12_4(tri->x1);
     voodoo3D->vertexAy = S12_4(tri->y1);

     voodoo3D->vertexBx = S12_4(tri->x2);
     voodoo3D->vertexBy = S12_4(tri->y2);

     voodoo3D->vertexCx = S12_4(tri->x3);
     voodoo3D->vertexCy = S12_4(tri->y3);

     voodoo3D->triangleCMD = (1 << 31);
}

static void tdfxBlit( DFBRectangle *rect, int dx, int dy )
{
     __u32 cmd = 1 | (1 <<8) | (0xCC << 24);//SST_2D_GO | SST_2D_SCRNTOSCRNBLIT | (ROP_COPY << 24);      
     
     if (tdfx->state->source == tdfx->state->destination) {        
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
     }
   
     
     tdfx_waitfifo( 4 );

     voodoo2D->srcXY   = ((rect->y & 0x1FFF) << 16) | (rect->x & 0x1FFF);
     voodoo2D->dstXY   = ((dy      & 0x1FFF) << 16) | (dx      & 0x1FFF);
     voodoo2D->dstSize = ((rect->h & 0x1FFF) << 16) | (rect->w & 0x1FFF);

     voodoo2D->command = cmd;
}

static void tdfxStretchBlit( DFBRectangle *sr, DFBRectangle *dr )
{
     tdfx_waitfifo( 5 );

     voodoo2D->srcXY   = ((sr->y & 0x1FFF) << 16) | (sr->x & 0x1FFF);
     voodoo2D->srcSize = ((sr->h & 0x1FFF) << 16) | (sr->w & 0x1FFF);
     
     voodoo2D->dstXY   = ((dr->y & 0x1FFF) << 16) | (dr->x & 0x1FFF);
     voodoo2D->dstSize = ((dr->h & 0x1FFF) << 16) | (dr->w & 0x1FFF);

     voodoo2D->command = 2 | (1 << 8) | (0xCC << 24);
}

/* exported symbols */

int driver_probe( int fd, GfxCard *card )
{
#ifdef FB_ACCEL_3DFX_BANSHEE
     switch (card->fix.accel) {
          case FB_ACCEL_3DFX_BANSHEE:          /* Banshee/Voodoo3 */
               return 1;
     }
#endif
     return 0;
}

int driver_init( int fd, GfxCard *card )
{
     mmio_base = (__u8*)mmap(NULL, card->fix.mmio_len, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, card->fix.smem_len);

     if (mmio_base == MAP_FAILED) {
          PERRORMSG("DirectFB/TDFX: Unable to map mmio region!\n");
          return DFB_IO;
     }

     voodoo2D = (Voodoo2D*)(mmio_base + 0x100000);
     voodoo3D = (Voodoo3D*)(mmio_base + 0x200000);

     sprintf( card->info.driver_name, "3Dfx V3/4/5/Banshee" );
     sprintf( card->info.driver_vendor, "convergence integrated media GmbH" );

     card->info.driver_version.major = 0;
     card->info.driver_version.minor = 0;

     card->caps.flags    = CCF_CLIPPING;
     card->caps.accel    = TDFX_SUPPORTED_DRAWINGFUNCTIONS |
                           TDFX_SUPPORTED_BLITTINGFUNCTIONS;
     card->caps.drawing  = TDFX_SUPPORTED_DRAWINGFLAGS;
     card->caps.blitting = TDFX_SUPPORTED_BLITTINGFLAGS;

     card->CheckState = tdfxCheckState;
     card->SetState = tdfxSetState;
     card->EngineSync = tdfxEngineSync;          

     card->DrawRectangle = tdfxDrawRectangle;
     card->DrawLine = tdfxDrawLine2D;
     card->Blit = tdfxBlit;
     card->StretchBlit = tdfxStretchBlit;

     card->byteoffset_align = 32 * 4;
     card->pixelpitch_align = 32;

     
     voodoo2D->status = 0;
     voodoo3D->nopCMD = 3;
     
     tdfx_waitfifo( 6 );

     voodoo3D->clipLeftRight1 = 0;
     voodoo3D->clipTopBottom1 = 0;

     voodoo3D->fbzColorPath = TDFX_FBZCOLORPATH_RGBSELECT_COLOR1 |
                              TDFX_FBZCOLORPATH_ASELECT_COLOR1;

     voodoo3D->textureMode = 0;

     voodoo2D->commandExtra = 0;
     voodoo2D->rop = 0xAAAAAA;

     tdfx_waitfifo( 1 );           /* VOODOO !!!  */
     *((__u32*)((__u8*) mmio_base + 0x10c)) = 1 << 4 | 1 << 8 | 5 << 12 | 
	                                      1 << 18 | 5 << 24;
     tdfx = card;

     dfb_config->pollvsync_after = 1;

     return DFB_OK;
}

void driver_init_layers()
{
}

void driver_deinit()
{
     DEBUGMSG( "DirectFB/TDFX: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/TDFX:  %9d tdfx_waitfifo calls\n",
               tdfx_waitfifo_calls );
     DEBUGMSG( "DirectFB/TDFX:  %9d register writes (tdfx_waitfifo sum)\n",
               tdfx_waitfifo_sum );
     DEBUGMSG( "DirectFB/TDFX:  %9d FIFO wait cycles (depends on CPU)\n",
               tdfx_fifo_waitcycles );
     DEBUGMSG( "DirectFB/TDFX:  %9d IDLE wait cycles (depends on CPU)\n",
               tdfx_idle_waitcycles );
     DEBUGMSG( "DirectFB/TDFX:  %9d FIFO space cache hits(depends on CPU)\n",
               tdfx_fifo_cache_hits );
     DEBUGMSG( "DirectFB/TDFX: Conclusion:\n" );
     DEBUGMSG( "DirectFB/TDFX:  Average register writes/tdfx_waitfifo"
               "call:%.2f\n",
               tdfx_waitfifo_sum/(float)(tdfx_waitfifo_calls) );
     DEBUGMSG( "DirectFB/TDFX:  Average wait cycles/tdfx_waitfifo call:"
               " %.2f\n",
               tdfx_fifo_waitcycles/(float)(tdfx_waitfifo_calls) );
     DEBUGMSG( "DirectFB/TDFX:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * tdfx_fifo_cache_hits/
               (float)(tdfx_waitfifo_calls)) );

     DEBUGMSG( "DirectFB/TDFX:  Pixels Out: %d\n", voodoo3D->fbiPixelsOut );
     DEBUGMSG( "DirectFB/TDFX:  Triangles Out: %d\n", voodoo3D->fbiTrianglesOut );
     
     munmap( (void*)mmio_base, tdfx->fix.mmio_len);
}


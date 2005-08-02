/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R200 based chipsets written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <dfb_types.h>
#include <directfb.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( r200 )


#include "r200.h"
#include "r200_regs.h"
#include "r200_mmio.h"
#include "r200_state.h"


/* driver capability flags */

#define R200_SUPPORTED_DRAWINGFLAGS \
     ( DSDRAW_BLEND | DSDRAW_XOR )

#define R200_SUPPORTED_DRAWINGFUNCTIONS \
     ( DFXL_FILLRECTANGLE | DFXL_FILLTRIANGLE | \
       DFXL_DRAWRECTANGLE | DFXL_DRAWLINE )

#define R200_SUPPORTED_BLITTINGFLAGS \
     ( DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | \
       DSBLIT_COLORIZE           | DSBLIT_SRC_PREMULTCOLOR | \
       DSBLIT_SRC_COLORKEY       | DSBLIT_DEINTERLACE )

#define R200_SUPPORTED_BLITTINGFUNCTIONS \
     ( DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES )


#define DSBLIT_MODULATE_ALPHA ( DSBLIT_BLEND_ALPHACHANNEL | \
                                DSBLIT_BLEND_COLORALPHA )
#define DSBLIT_MODULATE_COLOR ( DSBLIT_BLEND_COLORALPHA   | \
                                DSBLIT_COLORIZE           | \
                                DSBLIT_SRC_PREMULTCOLOR )
#define DSBLIT_MODULATE       ( DSBLIT_MODULATE_ALPHA     | \
                                DSBLIT_MODULATE_COLOR )



static bool r200FillRectangle      ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r200FillRectangle420   ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r200FillTriangle       ( void *drv, void *dev,
                                     DFBTriangle *tri );
static bool r200DrawRectangle      ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r200DrawLine           ( void *drv, void *dev,
                                     DFBRegion *line );
static bool r200Blit               ( void *drv, void *dev,
                                     DFBRectangle *sr, int dx, int dy );
static bool r200Blit420            ( void *drv, void *dev,
                                     DFBRectangle *sr, int dx, int dy );
static bool r200StretchBlit        ( void *drv, void *dev,
                                     DFBRectangle *sr, DFBRectangle *dr );
static bool r200StretchBlit420     ( void *drv, void *dev,
                                     DFBRectangle *sr, DFBRectangle *dr );
static bool r200TextureTriangles   ( void *drv, void *dev,
                                     DFBVertex *ve, int num,
                                     DFBTriangleFormation formation );
static bool r200TextureTriangles420( void *drv, void *dev,
                                     DFBVertex *ve, int num,
                                     DFBTriangleFormation formation );


static void 
r200_reset( R200DriverData *rdrv, R200DeviceData *rdev )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     __u32          clock_cntl_index;
     __u32          mclk_cntl;
     __u32          rbbm_soft_reset;
     __u32          host_path_cntl;
     __u32          dp_datatype;
     __u32          pitch64;
     __u32          bpp;
     
     r200_flush( rdrv, rdev );
     r200_waitidle( rdrv, rdev );
      
     clock_cntl_index = r200_in32( mmio, CLOCK_CNTL_INDEX );
     mclk_cntl        = r200_inpll( mmio, MCLK_CNTL );
     rbbm_soft_reset  = r200_in32( mmio, RBBM_SOFT_RESET );
     dp_datatype      = (r200_in32( mmio, CRTC_GEN_CNTL ) >> 8) & 0xf;
     
     switch (dp_datatype) {
          case DST_8BPP:
          case DST_8BPP_RGB332:
               bpp = 8;
               break;
          case DST_15BPP:
               bpp = 15;
               break;
          case DST_16BPP:
               bpp = 16;
               break;
          case DST_24BPP:
               bpp = 24;
               break;
          default:
               bpp = 32;
               break;
     }

     pitch64 = r200_in32( mmio, CRTC_H_TOTAL_DISP );
     pitch64 = ((((pitch64 >> 16) + 1) << 3) * bpp / 8 + 0x3f) >> 6;
    
     r200_outpll( mmio, MCLK_CNTL, mclk_cntl     |
			                    FORCEON_MCLKA |
			                    FORCEON_MCLKB |
			                    FORCEON_YCLKA |
			                    FORCEON_YCLKB |
			                    FORCEON_MC    |
			                    FORCEON_AIC );

     host_path_cntl = r200_in32( mmio, HOST_PATH_CNTL );
     
     r200_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset |
                                        SOFT_RESET_CP | SOFT_RESET_SE |
                                        SOFT_RESET_RE | SOFT_RESET_PP |
                                        SOFT_RESET_E2 | SOFT_RESET_RB );
     r200_in32( mmio, RBBM_SOFT_RESET );
     
     r200_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset & 
                                       ~(SOFT_RESET_CP | SOFT_RESET_SE |
                                         SOFT_RESET_RE | SOFT_RESET_PP |
                                         SOFT_RESET_E2 | SOFT_RESET_RB) );
     r200_in32( mmio, RBBM_SOFT_RESET );
     
     r200_out32( mmio, HOST_PATH_CNTL, host_path_cntl | HDP_SOFT_RESET );
     r200_in32( mmio, HOST_PATH_CNTL );
     r200_out32( mmio, HOST_PATH_CNTL, host_path_cntl );
     
     r200_out32( mmio, CLOCK_CNTL_INDEX, clock_cntl_index );
     r200_outpll( mmio, MCLK_CNTL, mclk_cntl );
   
     /* set framebuffer offset */
     r200_waitfifo( rdrv, rdev, 3 );
     r200_out32( mmio, DEFAULT_OFFSET, (rdev->fb_offset >> 10) |
                                       (pitch64 << 22) );
     r200_out32( mmio, DISPLAY_BASE_ADDR, rdev->fb_offset );
     r200_out32( mmio, OV0_BASE_ADDR, rdev->fb_offset );
       
     r200_waitfifo( rdrv, rdev, 1 );
#ifdef WORDS_BIGENDIAN
     r200_out32( mmio, DP_DATATYPE, dp_datatype | HOST_BIG_ENDIAN_EN );
#else
     r200_out32( mmio, DP_DATATYPE, dp_datatype );
#endif

     /* Disable byte swapping */
     r200_waitfifo( rdrv, rdev, 1 );
     r200_out32( mmio, SURFACE_CNTL, SURF_TRANSLATION_DIS );
     
     /* restore 2d engine */
     r200_waitfifo( rdrv, rdev, 4 );
     r200_out32( mmio, SC_TOP_LEFT, 0 );
     r200_out32( mmio, SC_BOTTOM_RIGHT, 0x07ff07ff );
     r200_out32( mmio, DP_GUI_MASTER_CNTL, GMC_BRUSH_SOLID_COLOR    |
                                           GMC_SRC_DATATYPE_COLOR   |
                                           GMC_ROP3_PATCOPY         |
                                           GMC_DP_SRC_SOURCE_MEMORY |
                                           GMC_CLR_CMP_CNTL_DIS     |
                                           GMC_WR_MSK_DIS );
     r200_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
   
     /* restore 3d engine */                                      
     r200_waitfifo( rdrv, rdev, 13 );
     r200_out32( mmio, SE_LINE_WIDTH, 0x10 );
     r200_out32( mmio, PP_MISC, ALPHA_TEST_PASS ); 
     r200_out32( mmio, R200_PP_CNTL_X, 0 );
     r200_out32( mmio, R200_PP_TXMULTI_CTL_0, 0 ); 
     r200_out32( mmio, R200_RE_CNTL, R200_SCISSOR_ENABLE | R200_PERSPECTIVE_ENABLE );
     r200_out32( mmio, R200_SE_VTX_STATE_CNTL, 0 );
     r200_out32( mmio, R200_SE_VTE_CNTL, R200_VTX_ST_DENORMALIZED );
     r200_out32( mmio, R200_SE_VAP_CNTL, R200_VAP_FORCE_W_TO_ONE |
	                                    R200_VAP_VF_MAX_VTX_NUM );
     if (rdev->chipset == CHIP_RS300)
          r200_out32( mmio, R200_SE_VAP_CNTL_STATUS, TCL_BYPASS );
     else
          r200_out32( mmio, R200_SE_VAP_CNTL_STATUS, 0 );
     r200_out32( mmio, RB3D_DSTCACHE_MODE, RB3D_DC_2D_CACHE_AUTOFLUSH |
                                           RB3D_DC_3D_CACHE_AUTOFLUSH |
                                           R200_RB3D_DC_2D_CACHE_AUTOFREE |
                                           R200_RB3D_DC_3D_CACHE_AUTOFREE );
     r200_out32( mmio, RE_TOP_LEFT, 0 );
     r200_out32( mmio, RE_BOTTOM_RIGHT, 0x07ff07ff );
     r200_out32( mmio, RB3D_ROPCNTL, ROP_XOR );
     
     rdev->set = 0;
     rdev->dst_format = DSPF_UNKNOWN;
     rdev->write_2d = false;
     rdev->write_3d = false;
}


static void r200AfterSetVar( void *drv, void *dev )
{
     r200_reset( (R200DriverData*)drv, (R200DeviceData*)dev );
}

static void r200EngineSync( void *drv, void *dev )
{
     r200_waitidle( (R200DriverData*)drv, (R200DeviceData*)dev );
}

static void r200FlushTextureCache( void *drv, void *dev )
{
     r200_flush( (R200DriverData*)drv, (R200DeviceData*)dev );
}

static void r200CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     CoreSurface *destination = state->destination;
     CoreSurface *source      = state->source;
     
     switch (destination->format) {
          case DSPF_A8:
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (DFB_DRAWING_FUNCTION( accel ) &&
                  (accel & DFXL_FILLTRIANGLE || state->drawingflags))
                    return;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if ((source->format != DSPF_I420  &&
                         source->format != DSPF_YV12) ||
                        state->blittingflags & ~DSBLIT_BLEND_COLORALPHA)
                         return;
               } else {
                    if (accel & ~DFXL_FILLRECTANGLE ||
                        state->drawingflags & ~DSDRAW_XOR)
                         return;
               }
               break;
               
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & ~R200_SUPPORTED_BLITTINGFLAGS)
               return;
          
          if (source->width < 8 || source->height < 8)
               return;

          if (state->blittingflags &  DSBLIT_SRC_COLORKEY &&
             (state->blittingflags & ~DSBLIT_SRC_COLORKEY ||
              destination->format != source->format))
               return;

          if (state->blittingflags & DSBLIT_MODULATE &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          switch (source->format) {
               case DSPF_A8:
               case DSPF_RGB332:
               case DSPF_ARGB4444:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    if (destination->format == DSPF_UYVY ||
                        destination->format == DSPF_YUY2)
                         return;
               case DSPF_YUY2:
               case DSPF_UYVY:
                    state->accel |= (state->blittingflags & DSBLIT_SRC_COLORKEY)
                                    ? DFXL_BLIT : R200_SUPPORTED_BLITTINGFUNCTIONS;
                    break;

               case DSPF_I420:
               case DSPF_YV12:
                    if (destination->format != DSPF_I420 &&
                        destination->format != DSPF_YV12)
                         return;
                    state->accel |= R200_SUPPORTED_BLITTINGFUNCTIONS;
                    break;
               
               default:
                    break;
          }
     } 
     else {
          if (state->drawingflags & ~R200_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          state->accel |= DFB_PLANAR_PIXELFORMAT( destination->format )
                          ? DFXL_FILLRECTANGLE
                          : R200_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void r200SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
 
     rdev->set &= ~state->modified;
     if (DFB_BLITTING_FUNCTION( accel )) {
          switch (rdev->accel) {
               case DFXL_BLIT:
               case DFXL_STRETCHBLIT:
                    if (accel == DFXL_TEXTRIANGLES)
                         rdev->set &= ~SMF_BLITTING_FLAGS;
                    break;
               case DFXL_TEXTRIANGLES:
                    if (accel != DFXL_TEXTRIANGLES)
                         rdev->set &= ~SMF_BLITTING_FLAGS;
                    break;
               default:
                    break;
          }
     }
     rdev->accel = accel;
     
     r200_set_destination( rdrv, rdev, state );
     r200_set_clip( rdrv, rdev, state );
    
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               r200_set_drawing_color( rdrv, rdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND)
                    r200_set_blend_function( rdrv, rdev, state );
               
               r200_set_drawingflags( rdrv, rdev, state );

               if (DFB_PLANAR_PIXELFORMAT( rdev->dst_format )) {
                    funcs->FillRectangle = r200FillRectangle420;
                    state->set = DFXL_FILLRECTANGLE;
               }
               else {
                    funcs->FillRectangle = r200FillRectangle;
                    state->set = DFXL_FILLRECTANGLE |
                                 DFXL_FILLTRIANGLE  |
                                 DFXL_DRAWLINE      |
                                 DFXL_DRAWRECTANGLE;
               }
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:     
               r200_set_source( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE)
                    r200_set_blend_function( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_COLOR)
                    r200_set_blitting_color( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    r200_set_src_colorkey( rdrv, rdev, state );
               
               r200_set_blittingflags( rdrv, rdev, state );

               if (DFB_PLANAR_PIXELFORMAT( rdev->dst_format )) {
                    funcs->Blit             = r200Blit420;
                    funcs->StretchBlit      = r200StretchBlit420;
                    funcs->TextureTriangles = r200TextureTriangles420;
               } else {
                    funcs->Blit             = r200Blit;
                    funcs->StretchBlit      = r200StretchBlit;
                    funcs->TextureTriangles = r200TextureTriangles;
               }
               
               state->set = (accel == DFXL_TEXTRIANGLES)
                            ?  DFXL_TEXTRIANGLES
                            : (DFXL_BLIT | DFXL_STRETCHBLIT);
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}


/* acceleration functions */

#define r200_enter2d( rdrv, rdev ) {                                      \
     if ((rdev)->write_3d) {                                              \
          r200_waitfifo( rdrv, rdev, 1 );                                 \
          r200_out32( (rdrv)->mmio_base, WAIT_UNTIL, WAIT_3D_IDLECLEAN ); \
          rdev->write_3d = false;                                         \
     }                                                                    \
     (rdev)->write_2d = true;                                             \
}

#define r200_enter3d( rdrv, rdev ) {                                      \
     if ((rdev)->write_2d) {                                              \
          r200_waitfifo( rdrv, rdev, 1 );                                 \
          r200_out32( (rdrv)->mmio_base, WAIT_UNTIL, WAIT_2D_IDLECLEAN ); \
          rdev->write_2d = false;                                         \
     }                                                                    \
     (rdev)->write_3d = true;                                             \
}

static inline void
out_vertex2d( volatile __u8 *mmio,
              float x, float y, float s, float t )
{
     union {
          float f[4];
          __u32 d[4];
     } tmp = {
          .f = { x, y, s, t }
     };
     
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[0] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[1] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[2] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[3] );
}

static inline void
out_vertex3d( volatile __u8 *mmio,
              float x, float y, float z, float w, float s, float t )
{
     union {
          float f[6];
          __u32 d[6];
     } tmp = {
          .f = { x, y, z, w, s, t }
     };
     
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[0] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[1] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[2] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[3] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[4] );
     r200_out32( mmio, SE_PORT_DATA0, tmp.d[5] );
}

/* drawing functions */

static void
r200DoFillRectangle2D( R200DriverData *rdrv,
                       R200DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, DST_Y_X, (rect->y << 16) |
                                (rect->x & 0x3fff) );
     r200_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) |
                                         (rect->w & 0x3fff) );
}

static void
r200DoFillRectangle3D( R200DriverData *rdrv,
                       R200DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;

     r200_waitfifo( rdrv, rdev, 17 );
     r200_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                   VF_PRIM_WALK_DATA      |
                                   (4 << VF_NUM_VERTICES_SHIFT) );

     out_vertex2d( mmio, rect->x        , rect->y        , 0, 0 );
     out_vertex2d( mmio, rect->x+rect->w, rect->y        , 0, 0 );
     out_vertex2d( mmio, rect->x+rect->w, rect->y+rect->h, 0, 0 );
     out_vertex2d( mmio, rect->x        , rect->y+rect->h, 0, 0 );
}

static bool
r200FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;

     if (rdev->drawingflags & ~DSDRAW_XOR ||
         rdev->dst_format == DSPF_ARGB4444)
     {      
          r200_enter3d( rdrv, rdev );
          r200DoFillRectangle3D( rdrv, rdev, rect );
     }
     else {
          if (rdev->dst_422) {
               rect->x /= 2;
               rect->w = (rect->w+1) >> 1;
          }
     
          r200_enter2d( rdrv, rdev );
          r200DoFillRectangle2D( rdrv, rdev, rect );
     }

     return true;
}

static bool
r200FillRectangle420( void *drv, void *dev, DFBRectangle *rect )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     r200_enter2d( rdrv, rdev );

     /* Fill Luma plane */
     r200DoFillRectangle2D( rdrv, rdev, rect );

     /* Scale coordinates */
     rect->x /= 2;
     rect->y /= 2;
     rect->w = (rect->w+1) >> 1;
     rect->h = (rect->h+1) >> 1;

     /* Prepare Cb plane */
     r200_waitfifo( rdrv, rdev, 5 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     r200_out32( mmio, DST_PITCH, rdev->dst_pitch >> 1 );
     r200_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r200_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                        ((clip->x2+1)/2 & 0xffff) );
     r200_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

     /* Fill Cb plane */
     r200DoFillRectangle2D( rdrv, rdev, rect );
     
     /* Prepare Cr plane */
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     r200_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

     /* Fill Cr plane */
     r200DoFillRectangle2D( rdrv, rdev, rect );

     /* Reset */
     r200_waitfifo( rdrv, rdev, 5 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset );
     r200_out32( mmio, DST_PITCH, rdev->dst_pitch );
     r200_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r200_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                        ((clip->x2+1) & 0xffff) );
     r200_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );

     return true;
}         

static bool
r200FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     R200DriverData *rdrv = ( R200DriverData* ) drv;
     R200DeviceData *rdev = ( R200DeviceData* ) dev;
     volatile __u8  *mmio = rdrv->mmio_base;

     r200_enter3d( rdrv, rdev );
     r200_waitfifo( rdrv, rdev, 13 );
     
     r200_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_TRIANGLE_LIST |
                                   VF_PRIM_WALK_DATA          |
                                   (3 << VF_NUM_VERTICES_SHIFT) );
                                   
     out_vertex2d( mmio, tri->x1, tri->y1, 0, 0 );
     out_vertex2d( mmio, tri->x2, tri->y2, 0, 0 );
     out_vertex2d( mmio, tri->x3, tri->y3, 0, 0 );
     
     return true;
}
     
static bool
r200DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     R200DriverData *rdrv = ( R200DriverData* ) drv;
     R200DeviceData *rdev = ( R200DeviceData* ) dev;
     volatile __u8  *mmio = rdrv->mmio_base;

     if (rdev->drawingflags & ~DSDRAW_XOR ||
         rdev->dst_format == DSPF_ARGB4444)
     {
          r200_enter3d( rdrv, rdev );
          r200_waitfifo( rdrv, rdev, 33 );
     
          r200_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LIST |
                                        VF_PRIM_WALK_DATA      |
                                        (8 << VF_NUM_VERTICES_SHIFT) );
     
          /* top line */
          out_vertex2d( mmio, rect->x        , rect->y+1        , 0, 0 );
          out_vertex2d( mmio, rect->x+rect->w, rect->y+1        , 0, 0 );
          /* bottom line */
          out_vertex2d( mmio, rect->x        , rect->y+rect->h  , 0, 0 );
          out_vertex2d( mmio, rect->x+rect->w, rect->y+rect->h  , 0, 0 );
          /* left line */
          out_vertex2d( mmio, rect->x+1      , rect->y+1        , 0, 0 );
          out_vertex2d( mmio, rect->x+1      , rect->y+rect->h-1, 0, 0 );
          /* right line */
          out_vertex2d( mmio, rect->x+rect->w, rect->y+1        , 0, 0 );
          out_vertex2d( mmio, rect->x+rect->w, rect->y+rect->h-1, 0, 0 );
     }
     else {
          if (rdev->dst_422) {
               rect->x /= 2;
               rect->w = (rect->w+1) >> 1;
          }
          
          r200_enter2d( rdrv, rdev );
          r200_waitfifo( rdrv, rdev, 7 );
     
          /* first line */
          r200_out32( mmio, DST_Y_X, (rect->y << 16) | (rect->x & 0x3fff) );
          r200_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
          /* second line */
          r200_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
          /* third line */
          r200_out32( mmio, DST_Y_X, ((rect->y+rect->h-1) << 16) | (rect->x & 0x3fff) );
          r200_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
          /* fourth line */
          r200_out32( mmio, DST_Y_X, (rect->y << 16) | ((rect->x+rect->w-1) & 0xffff) );
          r200_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
     }

     return true;
}

static bool
r200DrawLine( void *drv, void *dev, DFBRegion *line )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     volatile __u8  *mmio = rdrv->mmio_base;

     if (rdev->drawingflags & ~DSDRAW_XOR ||
         rdev->dst_format == DSPF_ARGB4444)
     {
          r200_enter3d( rdrv, rdev );
          r200_waitfifo( rdrv, rdev, 17 );
          
          r200_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LIST |
                                        VF_PRIM_WALK_DATA      |
                                        (2 << VF_NUM_VERTICES_SHIFT) );
          
          out_vertex2d( mmio, line->x1, line->y1, 0, 0 );
          out_vertex2d( mmio, line->x2, line->y2, 0, 0 );
     }
     else {
          if (rdev->dst_422) {
               line->x1 /= 2;
               line->x2 = (line->x2+1) / 2;
          }
          
          r200_enter2d( rdrv, rdev );
          r200_waitfifo( rdrv, rdev, 2 );
     
          r200_out32( mmio, DST_LINE_START, (line->y1 << 16) | 
                                            (line->x1 & 0xffff) );
          r200_out32( mmio, DST_LINE_END, (line->y2 << 16) |
                                          (line->x2 & 0xffff) );
     }

     return true;
}

/* blitting functions */

static void 
r200DoBlit2D( R200DriverData *rdrv, R200DeviceData *rdev,
              int sx, int sy, int dx, int dy, int w, int h )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     __u32          dir  = 0;
     
     /* check which blitting direction should be used */
     if (sx <= dx) {
          sx += w-1;
          dx += w-1;
     } else
          dir |= DST_X_LEFT_TO_RIGHT;

     if (sy <= dy) {
          sy += h-1;
          dy += h-1;
     } else
          dir |= DST_Y_TOP_TO_BOTTOM;

     r200_waitfifo( rdrv, rdev, 4 ); 
     r200_out32( mmio, DP_CNTL, dir ); 
     r200_out32( mmio, SRC_Y_X,          (sy << 16) | (sx & 0x3fff) );
     r200_out32( mmio, DST_Y_X,          (dy << 16) | (dx & 0x3fff) );
     r200_out32( mmio, DST_HEIGHT_WIDTH, (h  << 16) | (w  & 0x3fff) );
}

static void
r200DoBlit3D( R200DriverData *rdrv, R200DeviceData *rdev,
              DFBRectangle   *sr,   DFBRectangle   *dr )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r200_waitfifo( rdrv, rdev, 17 );

     r200_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_QUAD_LIST |
                                   VF_PRIM_WALK_DATA      |
                                   (4 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d( mmio, dr->x      , dr->y      , sr->x      , sr->y       );
     out_vertex2d( mmio, dr->x+dr->w, dr->y      , sr->x+sr->w, sr->y       );
     out_vertex2d( mmio, dr->x+dr->w, dr->y+dr->h, sr->x+sr->w, sr->y+sr->h );
     out_vertex2d( mmio, dr->x      , dr->y+dr->h, sr->x      , sr->y+sr->h );
}

static bool 
r200Blit( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     
     if (rdev->src_format != rdev->dst_format || 
         rdev->blittingflags & ~DSBLIT_SRC_COLORKEY) 
     {
          DFBRectangle dr = { dx, dy, sr->w, sr->h };
          
          if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
               sr->y /= 2;
               sr->h /= 2;
          }
          
          r200_enter3d( rdrv, rdev );
          r200DoBlit3D( rdrv, rdev, sr, &dr );
     }
     else {
          if (rdev->dst_422) {
               sr->x /= 2;
               sr->w = (sr->w+1) >> 1;
               dx    /= 2;
          }
     
          r200_enter2d( rdrv, rdev );
          r200DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     }

     return true;
}

static bool
r200Blit420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;  
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     if (rdev->blittingflags) {
          DFBRectangle dr = { dx, dy, sr->w, sr->h };
          return r200StretchBlit420( drv, dev, sr, &dr );
     }

     r200_enter2d( rdrv, rdev );

     /* Blit Luma plane */
     r200DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );

     /* Scale coordinates */
     sr->x /= 2;
     sr->y /= 2;
     sr->w = (sr->w+1) >> 1;
     sr->h = (sr->h+1) >> 1;
     dx    /= 2;
     dy    /= 2;
     
     /* Prepare Cb plane */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     r200_out32( mmio, DST_PITCH, rdev->dst_pitch >> 1 );
     r200_out32( mmio, SRC_OFFSET, rdev->src_offset_cb );
     r200_out32( mmio, SRC_PITCH, rdev->src_pitch >> 1 );
     r200_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r200_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1/2) << 16) |
                                        ((clip->x2+1/2) & 0xffff) );

     /* Blit Cb plane */
     r200DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Prepare Cr plane */
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     r200_out32( mmio, SRC_OFFSET, rdev->src_offset_cr );

     /* Blit Cr plane */
     r200DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Reset */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, DST_OFFSET, rdev->dst_offset );
     r200_out32( mmio, DST_PITCH, rdev->dst_pitch );
     r200_out32( mmio, SRC_OFFSET, rdev->src_offset );
     r200_out32( mmio, SRC_PITCH, rdev->src_pitch );
     r200_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r200_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                        ((clip->x2+1) & 0xffff) );

     return true;
}

static bool 
r200StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r200_enter3d( rdrv, rdev );
     r200DoBlit3D( rdrv, rdev, sr, dr );
     
     return true;
}

static bool 
r200StretchBlit420( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev; 
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r200_enter3d( rdrv, rdev );

     /* Blit Luma plane */
     r200DoBlit3D( rdrv, rdev, sr, dr );

     /* Scale coordinates */
     sr->x /= 2;
     sr->y /= 2;
     sr->w = (sr->w+1) >> 1;
     sr->h = (sr->h+1) >> 1;
     dr->x /= 2;
     dr->y /= 2;
     dr->w = (dr->w+1) >> 1;
     dr->h = (dr->h+1) >> 1;

     /* Prepare Cb plane */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     r200_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch >> 1 );
     r200_out32( mmio, R200_PP_TXSIZE_0, (rdev->src_width/2-1) |
                                        ((rdev->src_height/2-1) << 16) );
     r200_out32( mmio, R200_PP_TXPITCH_0, (rdev->src_pitch >> 1) - 32 );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset_cb );
     r200_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r200_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                        (clip->x2/2 & 0xffff) );

     /* Blit Cb plane */
     r200DoBlit3D( rdrv, rdev, sr, dr );

     /* Prepare Cr plane */
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset_cr );

     /* Blit Cr plane */
     r200DoBlit3D( rdrv, rdev, sr, dr );
          
     /* Reset */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     r200_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     r200_out32( mmio, R200_PP_TXSIZE_0, (rdev->src_width-1) |
                                        ((rdev->src_height-1) << 16) );
     r200_out32( mmio, R200_PP_TXPITCH_0, rdev->src_pitch - 32 );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset );
     r200_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r200_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                        (clip->x2 & 0xffff) );
     
     return true;
}

static void
r200DoTextureTriangles( R200DriverData *rdrv, R200DeviceData *rdev,
                        DFBVertex *ve, int num, __u32 primitive )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     int            i;
 
     r200_waitfifo( rdrv, rdev, 1 ); 
     r200_out32( mmio, SE_VF_CNTL, primitive | VF_PRIM_WALK_DATA |
                                   (num << VF_NUM_VERTICES_SHIFT) );

     for (; num >= 10; num -= 10) {
          r200_waitfifo( rdrv, rdev, 60 );
          for (i = 0; i < 10; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
          ve += 10;
     }

     if (num > 0) {
          r200_waitfifo( rdrv, rdev, num*6 );
          for (i = 0; i < num; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
     }
}

static bool 
r200TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                      int num, DFBTriangleFormation formation )
{ 
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     __u32           prim = 0;
     int             i;

     if (num > 65535) {
          D_WARN( "R200 supports maximum 65535 vertices" );
          return false;
     }

     switch (formation) {
          case DTTF_LIST:
               prim = VF_PRIM_TYPE_TRIANGLE_LIST;
               break;
          case DTTF_STRIP:
               prim = VF_PRIM_TYPE_TRIANGLE_STRIP;
               break;
          case DTTF_FAN:
               prim = VF_PRIM_TYPE_TRIANGLE_FAN;
               break;
          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }

     for (i = 0; i < num; i++) {
          ve[i].s *= (float)rdev->src_width;
          ve[i].t *= (float)rdev->src_height;
     }

     r200DoTextureTriangles( rdrv, rdev, ve, num, prim );

     return true;
}

static bool 
r200TextureTriangles420( void *drv, void *dev, DFBVertex *ve,
                         int num, DFBTriangleFormation formation )
{ 
     R200DriverData *rdrv = (R200DriverData*) drv;
     R200DeviceData *rdev = (R200DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     __u32           prim = 0;
     int             i;

     if (num > 65535) {
          D_WARN( "R200 supports maximum 65535 vertices" );
          return false;
     }

     switch (formation) {
          case DTTF_LIST:
               prim = VF_PRIM_TYPE_TRIANGLE_LIST;
               break;
          case DTTF_STRIP:
               prim = VF_PRIM_TYPE_TRIANGLE_STRIP;
               break;
          case DTTF_FAN:
               prim = VF_PRIM_TYPE_TRIANGLE_FAN;
               break;
          default:
               D_BUG( "unexpected triangle formation" );
               return false;
     }

     for (i = 0; i < num; i++) {
          ve[i].s *= (float)rdev->src_width;
          ve[i].t *= (float)rdev->src_height;
     }

     /* Map Luma plane */
     r200DoTextureTriangles( rdrv, rdev, ve, num, prim );

     /* Scale coordinates */
     for (i = 0; i < num; i++) {
          ve[i].x *= 0.5;
          ve[i].y *= 0.5;
          ve[i].s *= 0.5;
          ve[i].t *= 0.5;
     }

     /* Prepare Cb plane */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     r200_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch >> 1 );
     r200_out32( mmio, R200_PP_TXSIZE_0, (rdev->src_width/2-1) |
                                        ((rdev->src_height/2-1) << 16) );
     r200_out32( mmio, R200_PP_TXPITCH_0, (rdev->src_pitch >> 1) - 32 );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset_cb );
     r200_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r200_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                        (clip->x2/2 & 0xffff) );

     /* Map Cb plane */
     r200DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Prepare Cr plane */
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset_cr );

     /* Map Cr plane */
     r200DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Reset */
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     r200_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     r200_out32( mmio, R200_PP_TXSIZE_0, (rdev->src_width-1) |
                                        ((rdev->src_height-1) << 16) );
     r200_out32( mmio, R200_PP_TXPITCH_0, rdev->src_pitch - 32 );
     r200_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset );
     r200_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r200_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                        (clip->x2 & 0xffff) );

     return true;
}

/* probe functions */

static struct {
     __u16       id;
     __u32       chip;
     const char *name;
} dev_table[] = {
     { 0x514c, CHIP_R200 , "Radeon 8500 QL" },
     { 0x4242, CHIP_R200 , "Radeon 8500 AIW BB" },
     { 0x4243, CHIP_R200 , "Radeon 8500 AIW BC" },
     { 0x514d, CHIP_R200 , "Radeon 9100 QM" },
     { 0x5148, CHIP_R200 , "FireGL 8700/8800 QH" },
     { 0x4966, CHIP_RV250, "Radeon 9000/PRO If" },
     { 0x4967, CHIP_RV250, "Radeon 9000 Ig" },
     { 0x4c66, CHIP_RV250, "Radeon Mobility 9000 (M9) Lf" },
     { 0x4c67, CHIP_RV250, "Radeon Mobility 9000 (M9) Lg" },
     { 0x4c64, CHIP_RV250, "FireGL Mobility 9000 (M9) Ld" },
     { 0x5960, CHIP_RV280, "Radeon 9200PRO" },
     { 0x5961, CHIP_RV280, "Radeon 9200" },
     { 0x5962, CHIP_RV280, "Radeon 9200" },
     { 0x5964, CHIP_RV280, "Radeon 9200SE" },
     { 0x5c61, CHIP_RV280, "Radeon Mobility 9200 (M9+)" },
     { 0x5c63, CHIP_RV280, "Radeon Mobility 9200 (M9+)" },
     { 0x5834, CHIP_RS300, "Radeon 9100 IGP (A5)" },
     { 0x5835, CHIP_RS300, "Radeon Mobility 9100 IGP (U3)" },
     { 0x7834, CHIP_RS350, "Radeon 9100 PRO IGP" },
     { 0x7835, CHIP_RS350, "Radeon Mobility 9200 IGP" }
};

static int 
r200_probe_chipset( int *ret_index )
{
     unsigned int vendor_id;
     unsigned int device_id;
     int          i;

     dfb_system_get_deviceid( &vendor_id, &device_id );
     
     if (vendor_id == 0x1002) {
          for (i = 0; i < sizeof(dev_table)/sizeof(dev_table[0]); i++) {
               if ((unsigned int)dev_table[i].id == device_id) {
                    if (ret_index)
                         *ret_index = i;
                    return 1;
               }
          }
     }
     
     return 0;
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     return r200_probe_chipset( NULL );
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "ATI Radeon R200/RV250/RV280/RS300/RS350 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Claudio Ciccani" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof(R200DriverData);
     info->device_data_size = sizeof(R200DeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     
     /* gain access to memory mapped registers */
     rdrv->mmio_base = ( volatile __u8* ) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!rdrv->mmio_base)
          return DFB_IO;
          
     rdrv->device_data = (R200DeviceData*) device_data;

     /* fill function table */
     funcs->AfterSetVar       = r200AfterSetVar;
     funcs->EngineSync        = r200EngineSync;
     funcs->FlushTextureCache = r200FlushTextureCache;
     funcs->CheckState        = r200CheckState;
     funcs->SetState          = r200SetState;
     funcs->FillRectangle     = r200FillRectangle;
     funcs->FillTriangle      = r200FillTriangle;
     funcs->DrawRectangle     = r200DrawRectangle;
     funcs->DrawLine          = r200DrawLine;
     funcs->Blit              = r200Blit;
     funcs->StretchBlit       = r200StretchBlit;
     funcs->TextureTriangles  = r200TextureTriangles;
     
     /* primary screen */
     dfb_screens_hook_primary( device, driver_data, 
                               &R200PrimaryScreenFuncs,
                               &OldPrimaryScreenFuncs,
                               &OldPrimaryScreenDriverData ); 
     /* overlay support */
     dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                          driver_data, &R200OverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     R200DeviceData *rdev = (R200DeviceData*) device_data;
     int             id   = 0;
     
     if (!r200_probe_chipset( &id )) {
          D_ERROR( "DirectFB/R200: "
                   "unexpected error while probing device id!\n" );
          return DFB_FAILURE;
     }
     
     rdev->chipset = dev_table[id].chip;

     rdev->fb_offset = r200_in32( rdrv->mmio_base, NB_TOM );
     rdev->fb_offset = (rdev->fb_offset & 0xffff) << 16;
     
     D_DEBUG( "DirectFB/R200: "
              "Framebuffer starts at offset 0x%08x.\n",
               rdev->fb_offset );

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, dev_table[id].name );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = R200_SUPPORTED_DRAWINGFUNCTIONS |
                                  R200_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = R200_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = R200_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 16;
     device_info->limits.surface_pixelpitch_alignment = 64;

     dfb_config->pollvsync_after = 1;
 
     r200_reset( rdrv, rdev );
    
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     R200DeviceData *rdev = (R200DeviceData*) device_data;
    
     D_DEBUG( "DirectFB/R200: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/R200:  %9d r200_waitfifo calls\n",
              rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R200:  %9d register writes (r200_waitfifo sum)\n",
              rdev->waitfifo_sum );
     D_DEBUG( "DirectFB/R200:  %9d FIFO wait cycles (depends on CPU)\n",
              rdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/R200:  %9d IDLE wait cycles (depends on CPU)\n",
              rdev->idle_waitcycles );
     D_DEBUG( "DirectFB/R200:  %9d FIFO space cache hits(depends on CPU)\n",
              rdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/R200: Conclusion:\n" );
     D_DEBUG( "DirectFB/R200:  Average register writes/r200_waitfifo call:%.2f\n",
              rdev->waitfifo_sum / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R200:  Average wait cycles/r200_waitfifo call: %.2f\n",
              rdev->fifo_waitcycles / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R200:  Average fifo space cache hits: %02d%%\n",
              (int)(100 * rdev->fifo_cache_hits / (float)rdev->waitfifo_calls) );

     r200_reset( rdrv, rdev );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
    R200DriverData *rdrv = (R200DriverData*) driver_data;

    dfb_gfxcard_unmap_mmio( device, rdrv->mmio_base, -1 );
}


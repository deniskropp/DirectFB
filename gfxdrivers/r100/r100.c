/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R100 based chipsets written by
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

DFB_GRAPHICS_DRIVER( r100 )


#include "r100.h"
#include "r100_regs.h"
#include "r100_mmio.h"
#include "r100_state.h"


/* driver capability flags */

#define R100_SUPPORTED_DRAWINGFLAGS \
     ( DSDRAW_BLEND | DSDRAW_XOR )

#define R100_SUPPORTED_DRAWINGFUNCTIONS \
     ( DFXL_FILLRECTANGLE | DFXL_FILLTRIANGLE | \
       /*DFXL_DRAWRECTANGLE |*/ DFXL_DRAWLINE )

#define R100_SUPPORTED_BLITTINGFLAGS \
     ( DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | \
       DSBLIT_COLORIZE           | DSBLIT_SRC_PREMULTCOLOR | \
       DSBLIT_SRC_COLORKEY       | DSBLIT_DEINTERLACE      | \
       DSBLIT_XOR )

#define R100_SUPPORTED_BLITTINGFUNCTIONS \
     ( DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES )


#define DSBLIT_MODULATE_ALPHA ( DSBLIT_BLEND_ALPHACHANNEL | \
                                DSBLIT_BLEND_COLORALPHA )
#define DSBLIT_MODULATE_COLOR ( DSBLIT_BLEND_COLORALPHA   | \
                                DSBLIT_COLORIZE           | \
                                DSBLIT_SRC_PREMULTCOLOR )
#define DSBLIT_MODULATE       ( DSBLIT_MODULATE_ALPHA     | \
                                DSBLIT_MODULATE_COLOR )



static bool r100FillRectangle      ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r100FillRectangle420   ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r100FillTriangle       ( void *drv, void *dev,
                                     DFBTriangle *tri );
static bool r100FillTriangle420    ( void *drv, void *dev,
                                     DFBTriangle *tri );
static bool r100DrawRectangle      ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r100DrawRectangle420   ( void *drv, void *dev,
                                     DFBRectangle *rect );
static bool r100DrawLine           ( void *drv, void *dev,
                                     DFBRegion *line );
static bool r100DrawLine420        ( void *drv, void *dev,
                                     DFBRegion *line );
static bool r100Blit               ( void *drv, void *dev,
                                     DFBRectangle *sr, int dx, int dy );
static bool r100Blit420            ( void *drv, void *dev,
                                     DFBRectangle *sr, int dx, int dy );
static bool r100StretchBlit        ( void *drv, void *dev,
                                     DFBRectangle *sr, DFBRectangle *dr );
static bool r100StretchBlit420     ( void *drv, void *dev,
                                     DFBRectangle *sr, DFBRectangle *dr );
static bool r100TextureTriangles   ( void *drv, void *dev,
                                     DFBVertex *ve, int num,
                                     DFBTriangleFormation formation );
static bool r100TextureTriangles420( void *drv, void *dev,
                                     DFBVertex *ve, int num,
                                     DFBTriangleFormation formation );


void 
r100_reset( R100DriverData *rdrv, R100DeviceData *rdev )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     __u32          clock_cntl_index;
     __u32          mclk_cntl;
     __u32          rbbm_soft_reset;
     __u32          host_path_cntl;
     __u32          dp_datatype;
     __u32          pitch64;
     __u32          bpp;

     dp_datatype = (r100_in32( mmio, CRTC_GEN_CNTL ) >> 8) & 0xf;
     
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

     pitch64 = r100_in32( mmio, CRTC_H_TOTAL_DISP );
     pitch64 = ((((pitch64 >> 16) + 1) << 3) * bpp / 8 + 0x3f) >> 6;

     clock_cntl_index = r100_in32( mmio, CLOCK_CNTL_INDEX );
#if 0 
     mclk_cntl  = r100_inpll( mmio, MCLK_CNTL );
     r100_outpll( mmio, MCLK_CNTL, mclk_cntl     |
			                    FORCEON_MCLKA |
			                    FORCEON_MCLKB |
			                    FORCEON_YCLKA |
			                    FORCEON_YCLKB |
			                    FORCEON_MC    |
			                    FORCEON_AIC );

     host_path_cntl  = r100_in32( mmio, HOST_PATH_CNTL );
     rbbm_soft_reset = r100_in32( mmio, RBBM_SOFT_RESET );
     
     r100_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset |                       
                                        SOFT_RESET_CP | SOFT_RESET_HI |
                                        SOFT_RESET_SE | SOFT_RESET_RE |
                                        SOFT_RESET_PP | SOFT_RESET_E2 |
                                        SOFT_RESET_RB );
     r100_in32( mmio, RBBM_SOFT_RESET );
     
     r100_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset & 
                                       ~(SOFT_RESET_CP | SOFT_RESET_HI |
                                         SOFT_RESET_SE | SOFT_RESET_RE |
                                         SOFT_RESET_PP | SOFT_RESET_E2 |
                                         SOFT_RESET_RB) );
     r100_in32( mmio, RBBM_SOFT_RESET );
     
     r100_out32( mmio, HOST_PATH_CNTL, host_path_cntl | HDP_SOFT_RESET );
     r100_in32( mmio, HOST_PATH_CNTL );
     r100_out32( mmio, HOST_PATH_CNTL, host_path_cntl );
     
     r100_out32( mmio, CLOCK_CNTL_INDEX, clock_cntl_index );
     r100_outpll( mmio, MCLK_CNTL, mclk_cntl );
#else 
     (void) mclk_cntl;
     (void) rbbm_soft_reset;
     (void) host_path_cntl;
#endif

     /* reset byteswapper */
     r100_out32( mmio, SURFACE_CNTL, rdev->surface_cntl );
    
     /* set framebuffer base location */
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, DISPLAY_BASE_ADDR, rdev->fb_offset );
     r100_out32( mmio, DISPLAY2_BASE_ADDR, rdev->fb_offset );

     /* set default offset/pitch */
     r100_waitfifo( rdrv, rdev, 3 );
     r100_out32( mmio, DEFAULT_OFFSET, 
                       (rdev->fb_offset >> 10) | (pitch64 << 22) );
     r100_out32( mmio, DST_PITCH_OFFSET, 
                       (rdev->fb_offset >> 10) | (pitch64 << 22) );
     r100_out32( mmio, SRC_PITCH_OFFSET,
                       (rdev->fb_offset >> 10) | (pitch64 << 22) );

     r100_waitfifo( rdrv, rdev, 1 );
#ifdef WORDS_BIGENDIAN
     r100_out32( mmio, DP_DATATYPE, dp_datatype | HOST_BIG_ENDIAN_EN );
#else
     r100_out32( mmio, DP_DATATYPE, dp_datatype );
#endif
     
     /* restore 2d engine */
     r100_waitfifo( rdrv, rdev, 5 );
     r100_out32( mmio, SC_TOP_LEFT, 0 );
     r100_out32( mmio, SC_BOTTOM_RIGHT, 0x07ff07ff );
     r100_out32( mmio, DP_GUI_MASTER_CNTL, GMC_BRUSH_SOLID_COLOR    |
                                           GMC_SRC_DATATYPE_COLOR   |
                                           GMC_ROP3_PATCOPY         |
                                           GMC_DP_SRC_SOURCE_MEMORY |
                                           GMC_CLR_CMP_CNTL_DIS     |
                                           GMC_WR_MSK_DIS );
     r100_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     r100_out32( mmio, DP_WRITE_MASK, 0xffffffff );
   
     /* restore 3d engine */                                      
     r100_waitfifo( rdrv, rdev, 7 ); 
     r100_out32( mmio, SE_COORD_FMT, VTX_XY_PRE_MULT_1_OVER_W0 |
                                     TEX1_W_ROUTING_USE_W0 );
     r100_out32( mmio, SE_LINE_WIDTH, 0x10 );
     r100_out32( mmio, SE_CNTL_STATUS, TCL_BYPASS );
     r100_out32( mmio, PP_MISC, ALPHA_TEST_PASS );
     r100_out32( mmio, RB3D_DSTCACHE_MODE, RB3D_DC_2D_CACHE_AUTOFLUSH |
                                           RB3D_DC_3D_CACHE_AUTOFLUSH );
     r100_out32( mmio, RB3D_ROPCNTL, ROP_XOR );
     r100_out32( mmio, RB3D_PLANEMASK, 0xffffffff );

     /* set YUV422 color buffer */
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, PP_TXFILTER_1, 0 );
     r100_out32( mmio, PP_TXFORMAT_1, TXFORMAT_VYUY422 );
     
     rdev->set = 0;
     rdev->src_format = DSPF_UNKNOWN;
     rdev->dst_format = DSPF_UNKNOWN;
     rdev->write_2d = false;
     rdev->write_3d = false;
}


static void r100AfterSetVar( void *drv, void *dev )
{
     r100_waitidle( (R100DriverData*)drv, (R100DeviceData*)dev );
     r100_reset( (R100DriverData*)drv, (R100DeviceData*)dev );
}

static void r100EngineSync( void *drv, void *dev )
{
     r100_waitidle( (R100DriverData*)drv, (R100DeviceData*)dev );
}

static void r100FlushTextureCache( void *drv, void *dev )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     volatile __u8  *mmio = rdrv->mmio_base;

     r100_waitfifo( rdrv, rdev, 1 );
     r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
}

static void r100CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     R100DeviceData *rdev                    = (R100DeviceData*) dev;
     CoreSurface    *destination             = state->destination;
     CoreSurface    *source                  = state->source;
     int             supported_drawingfuncs  = R100_SUPPORTED_DRAWINGFUNCTIONS; 
     int             supported_drawingflags  = R100_SUPPORTED_DRAWINGFLAGS;
     int             supported_blittingfuncs = R100_SUPPORTED_BLITTINGFUNCTIONS;
     int             supported_blittingflags = R100_SUPPORTED_BLITTINGFLAGS;
     
     switch (destination->format) {               
          case DSPF_A8:
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          case DSPF_LUT8:
          case DSPF_ALUT44:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingflags   =  DSDRAW_NOFX;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;

          case DSPF_ARGB2554:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingfuncs  &= ~DFXL_FILLTRIANGLE;
               supported_drawingflags   =  DSDRAW_XOR;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;
               
          case DSPF_AiRGB:
               supported_drawingflags  &= ~DSDRAW_BLEND;
               supported_blittingflags &= ~DSBLIT_MODULATE_ALPHA;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != DSPF_A8      &&
                   source->format != DSPF_I420    &&
                   source->format != DSPF_YV12)
                    return;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (source && source->format != DSPF_A8)
                    supported_blittingflags &= ~(DSBLIT_COLORIZE | DSBLIT_SRC_COLORKEY);
               break;
               
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               if (destination->format != source->format)
                    return;
               supported_blittingfuncs  = DFXL_BLIT;
               supported_blittingflags &= DSBLIT_SRC_COLORKEY | DSBLIT_XOR;
          }
               
          if (accel & ~supported_blittingfuncs ||
              state->blittingflags & ~supported_blittingflags)
               return;
          
          if (source->width  < 8 || source->width  > 2048 ||
              source->height < 8 || source->height > 2048)
               return;

          if (state->blittingflags & DSBLIT_MODULATE_ALPHA &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          switch (source->format) {                    
               case DSPF_RGB332:
               case DSPF_ARGB4444:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    if (destination->format == DSPF_UYVY ||
                        destination->format == DSPF_YUY2)
                         return;
               case DSPF_A8:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;
               
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_ARGB2554:
                    if (destination->format != source->format)
                         return;
                    break;

               case DSPF_I420:
               case DSPF_YV12:
                    if (destination->format != DSPF_I420 &&
                        destination->format != DSPF_YV12)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= supported_blittingfuncs;
          rdev->blitting_mask = supported_blittingfuncs;
     } 
     else {
          if (accel & ~supported_drawingfuncs ||
              state->drawingflags & ~supported_drawingflags)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          state->accel |= supported_drawingfuncs;
          rdev->drawing_mask = supported_drawingfuncs;
     }
}

static void r100SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
 
     rdev->set &= ~state->modified;
     if (DFB_BLITTING_FUNCTION( accel )) {
          if ((rdev->accel ^ accel) & DFXL_TEXTRIANGLES)
               rdev->set &= ~SMF_BLITTING_FLAGS;
     }
     
     rdev->accel = accel;
     
     r100_set_destination( rdrv, rdev, state );
     r100_set_clip( rdrv, rdev, state );
    
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               r100_set_drawing_color( rdrv, rdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND)
                    r100_set_blend_function( rdrv, rdev, state );
               
               r100_set_drawingflags( rdrv, rdev, state );

               if (DFB_PLANAR_PIXELFORMAT( rdev->dst_format )) {
                    funcs->FillRectangle = r100FillRectangle420;
                    funcs->FillTriangle  = r100FillTriangle420;
                    funcs->DrawRectangle = r100DrawRectangle420;
                    funcs->DrawLine      = r100DrawLine420;
               } else {
                    funcs->FillRectangle = r100FillRectangle;
                    funcs->FillTriangle  = r100FillTriangle;
                    funcs->DrawRectangle = r100DrawRectangle;
                    funcs->DrawLine      = r100DrawLine;
               }

               state->set = rdev->drawing_mask;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:     
               r100_set_source( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA)
                    r100_set_blend_function( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_COLOR)
                    r100_set_blitting_color( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    r100_set_src_colorkey( rdrv, rdev, state );
               
               r100_set_blittingflags( rdrv, rdev, state );

               if (DFB_PLANAR_PIXELFORMAT( rdev->dst_format )) {
                    funcs->Blit             = r100Blit420;
                    funcs->StretchBlit      = r100StretchBlit420;
                    funcs->TextureTriangles = r100TextureTriangles420;
               } else {
                    funcs->Blit             = r100Blit;
                    funcs->StretchBlit      = r100StretchBlit;
                    funcs->TextureTriangles = r100TextureTriangles;
               }
               
               state->set = (accel & DFXL_TEXTRIANGLES) 
                            ? : (rdev->blitting_mask & ~DFXL_TEXTRIANGLES);
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}


/* acceleration functions */

#define r100_enter2d( rdrv, rdev ) {                                       \
     if ((rdev)->write_3d) {                                               \
          r100_waitfifo( rdrv, rdev, 1 );                                  \
          r100_out32( (rdrv)->mmio_base, WAIT_UNTIL, WAIT_3D_IDLECLEAN  |  \
                                                     WAIT_HOST_IDLECLEAN );\
          (rdev)->write_3d = false;                                        \
     }                                                                     \
     (rdev)->write_2d = true;                                              \
}

#define r100_enter3d( rdrv, rdev ) {                                       \
     if ((rdev)->write_2d) {                                               \
          r100_waitfifo( rdrv, rdev, 1 );                                  \
          r100_out32( (rdrv)->mmio_base, WAIT_UNTIL, WAIT_2D_IDLECLEAN  |  \
                                                     WAIT_HOST_IDLECLEAN );\
          (rdev)->write_2d = false;                                        \
     }                                                                     \
     (rdev)->write_3d = true;                                              \
}

static __inline__ __u32 
f2d( float f )
{
     union {
          float f;
          __u32 d;
     } tmp;
     tmp.f = f;
     return tmp.d;
}     

static __inline__ void
out_vertex2d0( volatile __u8 *mmio, float x, float y )
{
     r100_out32( mmio, SE_PORT_DATA0, f2d(x) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(y) );
}

static __inline__ void
out_vertex2d2( volatile __u8 *mmio, float x, float y, float s, float t )
{
     r100_out32( mmio, SE_PORT_DATA0, f2d(x) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(y) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(s) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(t) );
}

static __inline__ void
out_vertex3d( volatile __u8 *mmio,
              float x, float y, float z, float w, float s, float t )
{ 
     r100_out32( mmio, SE_PORT_DATA0, f2d(x) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(y) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(z) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(w) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(s) );
     r100_out32( mmio, SE_PORT_DATA0, f2d(t) );
}

/* drawing functions */

static void
r100DoFillRectangle2D( R100DriverData *rdrv,
                       R100DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r100_waitfifo( rdrv, rdev, 2 );
     
     r100_out32( mmio, DST_Y_X, (rect->y << 16) |
                                (rect->x & 0x3fff) );
     r100_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) |
                                         (rect->w & 0x3fff) );
}

static void
r100DoFillRectangle3D( R100DriverData *rdrv,
                       R100DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;

     if (rect->w == 1 && rect->h == 1) {
          r100_waitfifo( rdrv, rdev, 3 );
          
          r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_POINT_LIST |
                                        VF_PRIM_WALK_DATA       |
                                        VF_RADEON_MODE          |
                                        (1 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x, rect->y );
     }
     else {
          r100_waitfifo( rdrv, rdev, 7 );
     
          r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_RECTANGLE_LIST |
                                        VF_PRIM_WALK_DATA           |
                                        VF_RADEON_MODE              |
                                        (3 << VF_NUM_VERTICES_SHIFT) );

          out_vertex2d0( mmio, rect->x        , rect->y         );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y         );
          out_vertex2d0( mmio, rect->x+rect->w, rect->y+rect->h );
     }
}

static bool
r100FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;

     if (rdev->drawingflags & ~DSDRAW_XOR) {      
          r100_enter3d( rdrv, rdev );
          r100DoFillRectangle3D( rdrv, rdev, rect );
     }
     else {
          if (rdev->dst_422) {
               rect->x /= 2;
               rect->w  = (rect->w+1) >> 1;
          }
     
          r100_enter2d( rdrv, rdev );
          r100DoFillRectangle2D( rdrv, rdev, rect );
     }

     return true;
}

static bool
r100FillRectangle420( void *drv, void *dev, DFBRectangle *rect )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     if (rdev->drawingflags & ~DSDRAW_XOR) {
          r100_enter3d( rdrv, rdev );

          /* Fill Luma plane */
          r100DoFillRectangle3D( rdrv, rdev, rect );
          
          /* Scale coordinates */
          rect->x /= 2;
          rect->y /= 2;
          rect->w  = (rect->w+1) >> 1;
          rect->h  = (rect->h+1) >> 1;

          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                             (clip->x2/2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoFillRectangle3D( rdrv, rdev, rect );

          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoFillRectangle3D( rdrv, rdev, rect );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                             (clip->x2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
     }
     else {
          r100_enter2d( rdrv, rdev );

          /* Fill Luma plane */
          r100DoFillRectangle2D( rdrv, rdev, rect );

          /* Scale coordinates */
          rect->x /= 2;
          rect->y /= 2;
          rect->w  = (rect->w+1) >> 1;
          rect->h  = (rect->h+1) >> 1;

          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                             ((clip->x2+1)/2 & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoFillRectangle2D( rdrv, rdev, rect );
     
          /* Prepare Cr plane */
          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoFillRectangle2D( rdrv, rdev, rect );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                             ((clip->x2+1) & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
     }
          
     return true;
}

static void
r100DoFillTriangle( R100DriverData *rdrv,
                    R100DeviceData *rdev,
                    DFBTriangle    *tri )
{
     volatile __u8 *mmio = rdrv->mmio_base;

     r100_waitfifo( rdrv, rdev, 7 );
     
     r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_TRIANGLE_LIST |
                                   VF_PRIM_WALK_DATA          |
                                   VF_RADEON_MODE             |
                                   (3 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d0( mmio, tri->x1, tri->y1 );
     out_vertex2d0( mmio, tri->x2, tri->y2 );
     out_vertex2d0( mmio, tri->x3, tri->y3 );
}

static bool
r100FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;

     r100_enter3d( rdrv, rdev );

     r100DoFillTriangle( rdrv, rdev, tri );
     
     return true;
}

static bool
r100FillTriangle420( void *drv, void *dev, DFBTriangle *tri )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;

     r100_enter3d( rdrv, rdev );

     /* Fill Luma plane */
     r100DoFillTriangle( rdrv, rdev, tri );
          
     /* Scale coordinates */
     tri->x1 /= 2;
     tri->y1 /= 2;
     tri->x2 /= 2;
     tri->y2 /= 2;
     tri->x3 /= 2;
     tri->y3 /= 2;

     /* Prepare Cb plane */
     r100_waitfifo( rdrv, rdev, 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                        (clip->x2/2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

     /* Fill Cb plane */
     r100DoFillTriangle( rdrv, rdev, tri );

     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     r100_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

     /* Fill Cr plane */
     r100DoFillTriangle( rdrv, rdev, tri );

     /* Reset */
     r100_waitfifo( rdrv, rdev, 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                        (clip->x2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
     
     return true;
}

static void
r100DoDrawRectangle2D( R100DriverData *rdrv,
                       R100DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r100_waitfifo( rdrv, rdev, 7 );
     
     /* left line */
     r100_out32( mmio, DST_Y_X, (rect->y << 16) | (rect->x & 0x3fff) );
     r100_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
     /* top line */
     r100_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
     /* bottom line */
     r100_out32( mmio, DST_Y_X, ((rect->y+rect->h-1) << 16) | (rect->x & 0x3fff) );
     r100_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | (rect->w & 0xffff) );
     /* right line */
     r100_out32( mmio, DST_Y_X, (rect->y << 16) | ((rect->x+rect->w-1) & 0x3fff) );
     r100_out32( mmio, DST_HEIGHT_WIDTH, (rect->h << 16) | 1 );
}

static void
r100DoDrawRectangle3D( R100DriverData *rdrv,
                       R100DeviceData *rdev,
                       DFBRectangle   *rect )
{
     volatile __u8 *mmio = rdrv->mmio_base;

     r100_waitfifo( rdrv, rdev, 9 );
          
     r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LOOP |
                                   VF_PRIM_WALK_DATA      |
                                   VF_RADEON_MODE         |
                                   (4 << VF_NUM_VERTICES_SHIFT) );
     /* top/left */
     out_vertex2d0( mmio, rect->x        , rect->y         );
     /* top/right */
     out_vertex2d0( mmio, rect->x+rect->w, rect->y         );
     /* bottom/right */
     out_vertex2d0( mmio, rect->x+rect->w, rect->y+rect->h );
     /* bottom/left */
     out_vertex2d0( mmio, rect->x        , rect->y+rect->h );
}
     
static bool
r100DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;

     if (rdev->drawingflags & ~DSDRAW_XOR) {
          r100_enter3d( rdrv, rdev );
          r100DoDrawRectangle3D( rdrv, rdev, rect );
     }
     else {
          if (rdev->dst_422) {
               rect->x /= 2;
               rect->w  = (rect->w+1) >> 1;
          }
          
          r100_enter2d( rdrv, rdev );
          r100DoDrawRectangle2D( rdrv, rdev, rect );
     }

     return true;
}

static bool
r100DrawRectangle420( void *drv, void *dev, DFBRectangle *rect )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     if (rdev->drawingflags & ~DSDRAW_XOR) {
          r100_enter3d( rdrv, rdev );

          /* Fill Luma plane */
          r100DoDrawRectangle3D( rdrv, rdev, rect );
          
          /* Scale coordinates */
          rect->x  /= 2;
          rect->y  /= 2;
          rect->w >>= 1;
          rect->h >>= 1;

          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                             (clip->x2/2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoDrawRectangle3D( rdrv, rdev, rect );

          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoDrawRectangle3D( rdrv, rdev, rect );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                             (clip->x2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
     }
     else { 
          r100_enter2d( rdrv, rdev );

          /* Fill Luma plane */
          r100DoDrawRectangle2D( rdrv, rdev, rect );

          /* Scale coordinates */
          rect->x  /= 2;
          rect->y  /= 2;
          rect->w >>= 1;
          rect->h >>= 1;

          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                             ((clip->x2+1)/2 & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoDrawRectangle2D( rdrv, rdev, rect );
     
          /* Prepare Cr plane */
          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoDrawRectangle2D( rdrv, rdev, rect );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                             ((clip->x2+1) & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
     }
     
     return true;
}

static void
r100DoDrawLine2D( R100DriverData *rdrv,
                  R100DeviceData *rdev,
                  DFBRegion      *line )
{
     volatile __u8 *mmio = rdrv->mmio_base;

     r100_waitfifo( rdrv, rdev, 2 );
     
     r100_out32( mmio, DST_LINE_START, (line->y1 << 16) | 
                                       (line->x1 & 0xffff) );
     r100_out32( mmio, DST_LINE_END, (line->y2 << 16) |
                                     (line->x2 & 0xffff) );
}

static void
r100DoDrawLine3D( R100DriverData *rdrv,
                  R100DeviceData *rdev,
                  DFBRegion      *line )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r100_waitfifo( rdrv, rdev, 5 );
     
     r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_LINE_LIST |
                                   VF_PRIM_WALK_DATA      |
                                   VF_RADEON_MODE         |
                                   (2 << VF_NUM_VERTICES_SHIFT) );

     out_vertex2d0( mmio, line->x1, line->y1 );
     out_vertex2d0( mmio, line->x2, line->y2 );
}

static bool
r100DrawLine( void *drv, void *dev, DFBRegion *line )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;

     if (rdev->drawingflags & ~DSDRAW_XOR) {
          r100_enter3d( rdrv, rdev ); 
          r100DoDrawLine3D( rdrv, rdev, line );
     }
     else {
          if (rdev->dst_422) {
               line->x1 /= 2;
               line->x2  = (line->x2+1) / 2;
          }
          
          r100_enter2d( rdrv, rdev );
          r100DoDrawLine2D( rdrv, rdev, line );
     }

     return true;
}

static bool
r100DrawLine420( void *drv, void *dev, DFBRegion *line )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     line->x1 &= ~1;
     line->y1 &= ~1;
     line->x2 &= ~1;
     line->y2 &= ~1;
     
     if (rdev->drawingflags & ~DSDRAW_XOR) {
          r100_enter3d( rdrv, rdev );
          
          /* Fill Luma plane */
          r100DoDrawLine3D( rdrv, rdev, line );
          
          /* Scale coordinates */
          line->x1 /= 2;
          line->y1 /= 2;
          line->x2 /= 2;
          line->y2 /= 2;
          
          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) |
                                             (clip->x2/2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoDrawLine3D( rdrv, rdev, line );

          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, PP_TFACTOR_1, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoDrawLine3D( rdrv, rdev, line );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
          r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
          r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                             (clip->x2 & 0xffff) );
          r100_out32( mmio, PP_TFACTOR_1, rdev->y_cop );
     }
     else {
          r100_enter2d( rdrv, rdev );

          /* Fill Luma plane */
          r100DoDrawLine2D( rdrv, rdev, line );

          /* Scale coordinates */
          line->x1 /= 2;
          line->y1 /= 2;
          line->x2 /= 2;
          line->y2 /= 2;
          
          /* Prepare Cb plane */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                         (clip->x1/2 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1)/2 << 16) |
                                             ((clip->x2+1)/2 & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cb_cop );

          /* Fill Cb plane */
          r100DoDrawLine2D( rdrv, rdev, line );
     
          /* Prepare Cr plane */
          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->cr_cop );

          /* Fill Cr plane */
          r100DoDrawLine2D( rdrv, rdev, line );

          /* Reset */
          r100_waitfifo( rdrv, rdev, 5 );
          r100_out32( mmio, DST_OFFSET, rdev->dst_offset );
          r100_out32( mmio, DST_PITCH, rdev->dst_pitch );
          r100_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                         (clip->x1 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                             ((clip->x2+1) & 0xffff) );
          r100_out32( mmio, DP_BRUSH_FRGD_CLR, rdev->y_cop );
     }
          
     return true;
}

/* blitting functions */

static void 
r100DoBlit2D( R100DriverData *rdrv, R100DeviceData *rdev,
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

     r100_waitfifo( rdrv, rdev, 4 ); 
     
     r100_out32( mmio, DP_CNTL, dir ); 
     r100_out32( mmio, SRC_Y_X,          (sy << 16) | (sx & 0x3fff) );
     r100_out32( mmio, DST_Y_X,          (dy << 16) | (dx & 0x3fff) );
     r100_out32( mmio, DST_HEIGHT_WIDTH, (h  << 16) | (w  & 0x3fff) );
}

static void
r100DoBlit3D( R100DriverData *rdrv, R100DeviceData *rdev,
              DFBRectangle   *sr,   DFBRectangle   *dr )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     r100_waitfifo( rdrv, rdev, 13 );

     r100_out32( mmio, SE_VF_CNTL, VF_PRIM_TYPE_RECTANGLE_LIST |
                                   VF_PRIM_WALK_DATA           |
                                   VF_RADEON_MODE              |
                                   (3 << VF_NUM_VERTICES_SHIFT) );
     
     out_vertex2d2( mmio, dr->x      , dr->y      , sr->x      , sr->y       );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y      , sr->x+sr->w, sr->y       );
     out_vertex2d2( mmio, dr->x+dr->w, dr->y+dr->h, sr->x+sr->w, sr->y+sr->h );
}

static bool 
r100Blit( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     
     if (rdev->src_format != rdev->dst_format || 
         rdev->blittingflags & ~(DSBLIT_SRC_COLORKEY | DSBLIT_XOR)) 
     {
          DFBRectangle dr = { dx, dy, sr->w, sr->h };
          
          if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
               sr->y /= 2;
               sr->h /= 2;
          }
          
          r100_enter3d( rdrv, rdev );
          r100DoBlit3D( rdrv, rdev, sr, &dr );
     }
     else {
          if (rdev->dst_422) {
               sr->x /= 2;
               sr->w  = (sr->w+1) >> 1;
               dx    /= 2;
          }
     
          r100_enter2d( rdrv, rdev );
          r100DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     }

     return true;
}

static bool
r100Blit420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;  
     DFBRegion      *clip = &rdev->clip;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     if (!DFB_PLANAR_PIXELFORMAT( rdev->src_format ) ||
         rdev->blittingflags & ~DSBLIT_XOR) 
     {
          DFBRectangle dr = { dx, dy, sr->w, sr->h };
          return r100StretchBlit420( drv, dev, sr, &dr );
     }

     r100_enter2d( rdrv, rdev );

     /* Blit Luma plane */
     r100DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );

     /* Scale coordinates */
     sr->x /= 2;
     sr->y /= 2;
     sr->w  = (sr->w+1) >> 1;
     sr->h  = (sr->h+1) >> 1;
     dx    /= 2;
     dy    /= 2;
     
     /* Prepare Cb plane */
     r100_waitfifo( rdrv, rdev, 6 );
     r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cb );
     r100_out32( mmio, DST_PITCH, rdev->dst_pitch/2 );
     r100_out32( mmio, SRC_OFFSET, rdev->src_offset_cb );
     r100_out32( mmio, SRC_PITCH, rdev->src_pitch/2 );
     r100_out32( mmio, SC_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1/2) << 16) |
                                        ((clip->x2+1/2) & 0xffff) );

     /* Blit Cb plane */
     r100DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Prepare Cr plane */
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, DST_OFFSET, rdev->dst_offset_cr );
     r100_out32( mmio, SRC_OFFSET, rdev->src_offset_cr );

     /* Blit Cr plane */
     r100DoBlit2D( rdrv, rdev, sr->x, sr->y, dx, dy, sr->w, sr->h );
     
     /* Reset */
     r100_waitfifo( rdrv, rdev, 6 );
     r100_out32( mmio, DST_OFFSET, rdev->dst_offset );
     r100_out32( mmio, DST_PITCH, rdev->dst_pitch );
     r100_out32( mmio, SRC_OFFSET, rdev->src_offset );
     r100_out32( mmio, SRC_PITCH, rdev->src_pitch );
     r100_out32( mmio, SC_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r100_out32( mmio, SC_BOTTOM_RIGHT, ((clip->y2+1) << 16) |
                                        ((clip->x2+1) & 0xffff) );

     return true;
}

static bool 
r100StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r100_enter3d( rdrv, rdev );
     r100DoBlit3D( rdrv, rdev, sr, dr );
     
     return true;
}

static bool 
r100StretchBlit420( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     R100DriverData *rdrv    = (R100DriverData*) drv;
     R100DeviceData *rdev    = (R100DeviceData*) dev; 
     DFBRegion      *clip    = &rdev->clip;
     volatile __u8  *mmio    = rdrv->mmio_base;
     bool            src_420 = DFB_PLANAR_PIXELFORMAT( rdev->src_format );
     
     if (rdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     r100_enter3d( rdrv, rdev );

     /* Blit Luma plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );

     /* Scale coordinates */
     if (src_420) {
          sr->x /= 2;
          sr->y /= 2;
          sr->w  = (sr->w+1) >> 1;
          sr->h  = (sr->h+1) >> 1;
     }
     dr->x /= 2;
     dr->y /= 2;
     dr->w  = (dr->w+1) >> 1;
     dr->h  = (dr->h+1) >> 1;

     /* Prepare Cb plane */
     r100_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     if (src_420) {
          r100_out32( mmio, PP_TEX_SIZE_0, (rdev->src_height/2 << 16) |
                                           (rdev->src_width/2 & 0xffff) );
          r100_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch/2 - 32 );
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cb );
     }
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                        (clip->x2/2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_0, rdev->cb_cop );

     /* Blit Cb plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );

     /* Prepare Cr plane */
     r100_waitfifo( rdrv, rdev, src_420 ? 3 : 2 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     if (src_420)
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
     r100_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );

     /* Blit Cr plane */
     r100DoBlit3D( rdrv, rdev, sr, dr );
          
     /* Reset */
     r100_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     if (src_420) {
          r100_out32( mmio, PP_TEX_SIZE_0, (rdev->src_height << 16) |
                                           (rdev->src_width & 0xffff) );
          r100_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     }
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                        (clip->x2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_0, rdev->y_cop );
     
     return true;
}

static void
r100DoTextureTriangles( R100DriverData *rdrv, R100DeviceData *rdev,
                        DFBVertex *ve, int num, __u32 primitive )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     int            i;
 
     r100_waitfifo( rdrv, rdev, 1 ); 
     
     r100_out32( mmio, SE_VF_CNTL, primitive         | 
                                   VF_PRIM_WALK_DATA |
                                   VF_RADEON_MODE    |
                                   (num << VF_NUM_VERTICES_SHIFT) );

     for (; num >= 10; num -= 10) {
          r100_waitfifo( rdrv, rdev, 60 );
          for (i = 0; i < 10; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
          ve += 10;
     }

     if (num > 0) {
          r100_waitfifo( rdrv, rdev, num*6 );
          for (i = 0; i < num; i++)
               out_vertex3d( mmio, ve[i].x, ve[i].y, ve[i].z,
                                   ve[i].w, ve[i].s, ve[i].t );
     }
}

static bool 
r100TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                      int num, DFBTriangleFormation formation )
{ 
     R100DriverData *rdrv = (R100DriverData*) drv;
     R100DeviceData *rdev = (R100DeviceData*) dev;
     __u32           prim = 0;

     if (num > 65535) {
          D_WARN( "R100 supports maximum 65535 vertices" );
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

     r100_enter3d( rdrv, rdev );
     
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );

     return true;
}

static bool 
r100TextureTriangles420( void *drv, void *dev, DFBVertex *ve,
                         int num, DFBTriangleFormation formation )
{ 
     R100DriverData *rdrv    = (R100DriverData*) drv;
     R100DeviceData *rdev    = (R100DeviceData*) dev;
     DFBRegion      *clip    = &rdev->clip;
     volatile __u8  *mmio    = rdrv->mmio_base;
     bool            src_420 = DFB_PLANAR_PIXELFORMAT( rdev->src_format );
     __u32           prim    = 0;
     int             i;

     if (num > 65535) {
          D_WARN( "R100 supports maximum 65535 vertices" );
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

     r100_enter3d( rdrv, rdev );

     /* Map Luma plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );

     /* Scale coordinates */
     for (i = 0; i < num; i++) {
          ve[i].x *= 0.5;
          ve[i].y *= 0.5;
          if (src_420) {
               ve[i].s *= 0.5;
               ve[i].t *= 0.5;
          }
     }

     /* Prepare Cb plane */
     r100_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cb );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch/2 );
     if (src_420) {
          r100_out32( mmio, PP_TEX_SIZE_0, (rdev->src_height << 16) |
                                           (rdev->src_width & 0xffff) );
          r100_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch/2 - 32 );
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cb );
     }
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1/2 << 16) |
                                    (clip->x1/2 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2/2 << 16) | 
                                        (clip->x2/2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_0, rdev->cb_cop );
     
     /* Map Cb plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Prepare Cr plane */
     r100_waitfifo( rdrv, rdev, src_420 ? 3 : 2 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset_cr );
     if (src_420)
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset_cr );
     r100_out32( mmio, PP_TFACTOR_0, rdev->cr_cop );

     /* Map Cr plane */
     r100DoTextureTriangles( rdrv, rdev, ve, num, prim );
     
     /* Reset */
     r100_waitfifo( rdrv, rdev, src_420 ? 8 : 5 );
     r100_out32( mmio, RB3D_COLOROFFSET, rdev->dst_offset );
     r100_out32( mmio, RB3D_COLORPITCH, rdev->dst_pitch );
     if (src_420) {
          r100_out32( mmio, PP_TEX_SIZE_0, (rdev->src_height << 16) |
                                           (rdev->src_width & 0xffff) );
          r100_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
          r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     }
     r100_out32( mmio, RE_TOP_LEFT, (clip->y1 << 16) |
                                    (clip->x1 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT, (clip->y2 << 16) |
                                        (clip->x2 & 0xffff) );
     r100_out32( mmio, PP_TFACTOR_0, rdev->y_cop );
     
     return true;
}


/* primary screen hooks */

static DFBResult
crtc1WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     R100DriverData *rdrv = (R100DriverData*) driver_data;
     volatile __u8  *mmio = rdrv->mmio_base; 
     int             i;
     
     if (dfb_config->pollvsync_none)
          return DFB_OK;
          
     r100_out32( mmio, GEN_INT_STATUS, VSYNC_INT_AK );
     
     for (i = 0; i < 2000000; i++) {
          struct timespec t = { 0, 0 };     
          
          if (r100_in32( mmio, GEN_INT_STATUS ) & VSYNC_INT)
               break;
          nanosleep( &t, NULL );
     }

     return DFB_OK;
}

ScreenFuncs R100PrimaryScreenFuncs = {
     .WaitVSync = crtc1WaitVSync
};

ScreenFuncs  OldPrimaryScreenFuncs;
void        *OldPrimaryScreenDriverData;



/* probe functions */

static const struct {
     __u16       id;
     __u32       chip;
     bool        igp;
     const char *name;
} dev_table[] = {
     { 0x5144, CHIP_R100 , false, "Radeon QD" },
     { 0x5145, CHIP_R100 , false, "Radeon QE" },
     { 0x5146, CHIP_R100 , false, "Radeon QF" },
     { 0x5147, CHIP_R100 , false, "Radeon QG" },
     { 0x5159, CHIP_RV100, false, "Radeon VE/7000 QY" },
     { 0x515a, CHIP_RV100, false, "Radeon VE/7000 QZ" },
     { 0x4c59, CHIP_RV100, false, "Radeon Mobility M6 LY" },
     { 0x4c5a, CHIP_RV100, false, "Radeon Mobility M6 LZ" },
     { 0x4c57, CHIP_RV200, false, "Radeon Mobility M7 LW" },
     { 0x4c58, CHIP_RV200, false, "FireGL Mobility 7800 M7 LX" },
     { 0x5157, CHIP_RV200, false, "Radeon 7500 QW" },
     { 0x5158, CHIP_RV200, false, "Radeon 7500 QX" },
     { 0x4136, CHIP_RS100, true , "Radeon IGP320 (A3)" },
     { 0x4336, CHIP_RS100, true , "Radeon IGP320M (U1)" },
     { 0x4137, CHIP_RS200, true , "Radeon IGP330/340/350 (A4)" },
     { 0x4337, CHIP_RS200, true , "Radeon IGP330M/340M/350M (U2)" },
     { 0x4237, CHIP_RS250, true , "Radeon 7000 IGP (A4+)" },
     { 0x4437, CHIP_RS250, true , "Radeon Mobility 7000 IGP" }
};

static int 
r100_probe_chipset( int *ret_index )
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
     return r100_probe_chipset( NULL );
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "ATI Radeon R100/RV100/RV200/RS100/RS200/RS250 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Claudio Ciccani" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof(R100DriverData);
     info->device_data_size = sizeof(R100DeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     R100DriverData *rdrv = (R100DriverData*) driver_data;
     
     /* gain access to memory mapped registers */
     rdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!rdrv->mmio_base)
          return DFB_IO;

     rdrv->device_data = (R100DeviceData*) device_data;
     rdrv->fb_base     = dfb_gfxcard_memory_virtual( device, 0 );

     /* fill function table */
     funcs->AfterSetVar       = r100AfterSetVar;
     funcs->EngineSync        = r100EngineSync;
     funcs->FlushTextureCache = r100FlushTextureCache;
     funcs->CheckState        = r100CheckState;
     funcs->SetState          = r100SetState;
     funcs->FillRectangle     = r100FillRectangle;
     funcs->FillTriangle      = r100FillTriangle;
     funcs->DrawRectangle     = r100DrawRectangle;
     funcs->DrawLine          = r100DrawLine;
     funcs->Blit              = r100Blit;
     funcs->StretchBlit       = r100StretchBlit;
     funcs->TextureTriangles  = r100TextureTriangles;
     
     /* primary screen */
     dfb_screens_hook_primary( device, driver_data, 
                               &R100PrimaryScreenFuncs,
                               &OldPrimaryScreenFuncs,
                               &OldPrimaryScreenDriverData ); 
     /* overlay support */
     dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                          driver_data, &R100OverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     R100DriverData *rdrv = (R100DriverData*) driver_data;
     R100DeviceData *rdev = (R100DeviceData*) device_data;
     int             id   = 0;
     
     if (!r100_probe_chipset( &id )) {
          D_ERROR( "DirectFB/R100: "
                   "unexpected error while probing device id!\n" );
          return DFB_FAILURE;
     }
     
     rdev->chipset = dev_table[id].chip;
     rdev->igp     = dev_table[id].igp;
     
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, dev_table[id].name );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

     device_info->caps.flags    = CCF_CLIPPING | CCF_AUXMEMORY;
     device_info->caps.accel    = R100_SUPPORTED_DRAWINGFUNCTIONS |
                                  R100_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = R100_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = R100_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32;
     device_info->limits.surface_pixelpitch_alignment = 64;

     dfb_config->pollvsync_after = 1;

     /* reserve memory for YUV422 color buffer */
     rdev->yuv422_buffer = dfb_gfxcard_reserve_memory( device, 128 );
     if (rdev->yuv422_buffer == (__u32)-1) {
          D_ERROR( "DirectFB/R100: "
                   "couldn't reserve 128 bytes of video memory!\n" );
          return DFB_NOVIDEOMEMORY;
     }

     r100_waitidle( rdrv, rdev );

     if (rdev->igp) {
          __u32 tom;
          /* force MC_FB_LOCATION to NB_TOM */
          tom = r100_in32( rdrv->mmio_base, NB_TOM );
          r100_out32( rdrv->mmio_base, MC_FB_LOCATION, tom );
          rdev->fb_offset = tom << 16;
     } 
     else {
          rdev->fb_offset = r100_in32( rdrv->mmio_base, MC_FB_LOCATION ) << 16;
     }
     
     D_DEBUG( "DirectFB/R100: "
              "Framebuffer location at 0x%08x.\n", rdev->fb_offset );

     if (dfb_system_auxram_length()) {
          r100_out32( rdrv->mmio_base, AGP_BASE,
                      dfb_system_aux_memory_physical( 0 ) );
          r100_out32( rdrv->mmio_base, AGP_CNTL,
                      r100_in32( rdrv->mmio_base, AGP_CNTL ) | 0x000e0000 );
          r100_out32( rdrv->mmio_base, AGP_COMMAND, 0 );

          rdev->agp_offset = r100_in32( rdrv->mmio_base, MC_AGP_LOCATION ) << 16;

          D_DEBUG( "DirectFB/R100: "
                   "AGP aperture location at 0x%08x.\n", rdev->agp_offset );
     }

     rdev->surface_cntl  = r100_in32( rdrv->mmio_base, SURFACE_CNTL );
     rdev->surface_cntl &= ~(NONSURF_AP0_SWP_16BPP | NONSURF_AP0_SWP_32BPP);
 
     r100_reset( rdrv, rdev );
    
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     R100DriverData *rdrv = (R100DriverData*) driver_data;
     R100DeviceData *rdev = (R100DeviceData*) device_data;
    
     D_DEBUG( "DirectFB/R100: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/R100:  %9d r100_waitfifo calls\n",
              rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R100:  %9d register writes (r100_waitfifo sum)\n",
              rdev->waitfifo_sum );
     D_DEBUG( "DirectFB/R100:  %9d FIFO wait cycles (depends on CPU)\n",
              rdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/R100:  %9d IDLE wait cycles (depends on CPU)\n",
              rdev->idle_waitcycles );
     D_DEBUG( "DirectFB/R100:  %9d FIFO space cache hits(depends on CPU)\n",
              rdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/R100: Conclusion:\n" );
     D_DEBUG( "DirectFB/R100:  Average register writes/r100_waitfifo call:%.2f\n",
              rdev->waitfifo_sum / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R100:  Average wait cycles/r100_waitfifo call: %.2f\n",
              rdev->fifo_waitcycles / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/R100:  Average fifo space cache hits: %02d%%\n",
              (int)(100 * rdev->fifo_cache_hits / (float)rdev->waitfifo_calls) );

     r100_waitidle( rdrv, rdev );
     r100_reset( rdrv, rdev );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
    R100DriverData *rdrv = (R100DriverData*) driver_data;

    dfb_gfxcard_unmap_mmio( device, rdrv->mmio_base, -1 );
}


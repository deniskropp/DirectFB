#ifndef __GP2D_BLT_H__
#define __GP2D_BLT_H__

#include <sys/ioctl.h>

#include "gp2d_types.h"



#define GP2D_SUPPORTED_DRAWINGFLAGS      (DSDRAW_BLEND)

#define GP2D_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE | \
                                          DFXL_FILLTRIANGLE  | \
                                          DFXL_DRAWLINE      | \
                                          DFXL_DRAWRECTANGLE)

#define GP2D_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_COLORALPHA | \
                                          DSBLIT_SRC_COLORKEY)

#define GP2D_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT | DFXL_STRETCHBLIT)


DFBResult gp2dEngineSync  ( void *drv, void *dev );

void gp2dEngineReset      ( void *drv, void *dev );
void gp2dFlushTextureCache( void *drv, void *dev );

void gp2dEmitCommands     ( void *drv, void *dev );

void gp2dCheckState       ( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel );

void gp2dSetState         ( void *drv, void *dev,
                              GraphicsDeviceFuncs *funcs,
                              CardState *state, DFBAccelerationMask accel );

bool gp2dFillRectangle    ( void *drv, void *dev, DFBRectangle *rect );
bool gp2dFillTriangle     ( void *drv, void *dev, DFBTriangle *triangle );
bool gp2dDrawRectangle    ( void *drv, void *dev, DFBRectangle *rect );
bool gp2dDrawLine         ( void *drv, void *dev, DFBRegion *line );
bool gp2dBlit             ( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
bool gp2dStretchBlit      ( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );



DFBResult
gp2d_blt_gen_free( GP2DDriverData *gdrv,
                   unsigned int    num );

GP2DBuffer *
gp2d_get_buffer( GP2DDriverData *gdrv );

DFBResult
gp2d_create_buffer( GP2DDriverData  *gdrv,
                    unsigned int     size,
                    GP2DBuffer     **ret_buffer );

void
gp2d_put_buffer( GP2DDriverData *gdrv,
                 GP2DBuffer     *buffer );

DFBResult
gp2d_exec_buffer( GP2DDriverData *gdrv,
                  GP2DBuffer     *buffer );


#define GP2D_S16S16(h,l)         ((u32)((((u16)(h)) << 16) | ((u16)(l))))

#define GP2D_XY(x,y)             GP2D_S16S16(x,y)


#define GP2D_OPCODE_TRAP        0x00000000
#define GP2D_OPCODE_WPR         0x18000000
#define GP2D_OPCODE_SYNC        0x12000000
#define GP2D_OPCODE_LCOFS       0x40000000
#define GP2D_OPCODE_MOVE        0x48000000
#define GP2D_OPCODE_NOP         0x08000000
#define GP2D_OPCODE_INTERRUPT   0x08008000
#define GP2D_SYNC_TCLR          0x00000010

#define GP2D_OPCODE_POLYGON_4A  0x82000000
#define GP2D_OPCODE_POLYGON_4C  0x80000000
#define GP2D_OPCODE_LINE_C      0xB0000000
#define GP2D_OPCODE_BITBLTA     0xA2000100
#define GP2D_OPCODE_BITBLTC     0xA0000000

#define GP2D_DRAWMODE_MTRE      0x00008000
#define GP2D_DRAWMODE_CLIP      0x00002000
#define GP2D_DRAWMODE_STRANS    0x00000800
#define GP2D_DRAWMODE_SS        0x00000100
#define GP2D_DRAWMODE_ANTIALIAS 0x00000002
#define GP2D_DRAWMODE_ALPHA     0x00000002

#define GP2D_DRAWMODE_SRCDIR_X  0x00000040
#define GP2D_DRAWMODE_SRCDIR_Y  0x00000020
#define GP2D_DRAWMODE_DSTDIR_X  0x00000010
#define GP2D_DRAWMODE_DSTDIR_Y  0x00000008



/*
 * Registers
 */
#define GP2D_REG_SSAR         0x04c
#define GP2D_REG_RSAR         0x050
#define GP2D_REG_SSTRR        0x058
#define GP2D_REG_DSTRR        0x05c
#define GP2D_REG_STCR         0x080
#define GP2D_REG_ALPHR        0x088
#define GP2D_REG_RCLR         0x0c0
#define GP2D_REG_SCLMAR       0x0d0
#define GP2D_REG_UCLMIR       0x0d4
#define GP2D_REG_UCLMAR       0x0d8
#define GP2D_REG_GTRCR        0x100
#define GP2D_REG_MTRAR        0x104
#define GP2D_REG_MD0R         0x1fc


#define GP2D_GTRCR_AFE        0x00000001
#define GP2D_GTRCR_GTE        0x80000000


#endif


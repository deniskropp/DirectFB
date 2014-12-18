#ifndef __VSP1_BLT_H__
#define __VSP1_BLT_H__

#include <sys/ioctl.h>

#include "vsp1_types.h"


// define 0 or 1
#define VSP1_USE_THREAD	1


#define VSP1_SUPPORTED_DRAWINGFLAGS      (DSDRAW_BLEND)

#define VSP1_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_NONE)//FILLRECTANGLE)

#define VSP1_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_ALPHACHANNEL | \
                                          DSBLIT_BLEND_COLORALPHA   | \
                                          DSBLIT_SRC_PREMULTIPLY)

#define VSP1_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT | DFXL_STRETCHBLIT)


DFBResult vsp1EngineSync   ( void *drv, void *dev );
void      vsp1EmitCommands ( void *drv, void *dev );

void      vsp1CheckState   ( void *drv, void *dev,
                             CardState *state, DFBAccelerationMask accel );

void      vsp1SetState     ( void *drv, void *dev,
                             GraphicsDeviceFuncs *funcs,
                             CardState *state, DFBAccelerationMask accel );

bool      vsp1FillRectangle( void *drv, void *dev, DFBRectangle *rect );
bool      vsp1Blit         ( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
bool      vsp1StretchBlit  ( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );



#define VSP1_S16S16(h,l)         ((u32)((((u16)(h)) << 16) | ((u16)(l))))

#define VSP1_XY(x,y)             VSP1_S16S16(x,y)




#endif


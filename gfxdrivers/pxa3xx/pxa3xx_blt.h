#ifndef __PXA3XX_BLT_H__
#define __PXA3XX_BLT_H__

#include "pxa3xx_types.h"



#define PXA3XX_SUPPORTED_DRAWINGFLAGS      (DSDRAW_NOFX | \
                                            DSDRAW_BLEND)

#define PXA3XX_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE)

#define PXA3XX_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_ALPHACHANNEL | \
                                            DSBLIT_COLORIZE | \
                                            DSBLIT_ROTATE90 | \
                                            DSBLIT_ROTATE180 | \
                                            DSBLIT_ROTATE270)

#define PXA3XX_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT)


DFBResult pxa3xxEngineSync  ( void *drv, void *dev );

void pxa3xxEngineReset      ( void *drv, void *dev );

void pxa3xxEmitCommands     ( void *drv, void *dev );

void pxa3xxCheckState       ( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel );

void pxa3xxSetState         ( void *drv, void *dev,
                              GraphicsDeviceFuncs *funcs,
                              CardState *state, DFBAccelerationMask accel );


#define PXA3XX_S16S16(h,l)         ((u32)((((u16)(h)) << 16) | ((u16)(l))))

#define PXA3XX_WH(w,h)             PXA3XX_S16S16(h,w)


#endif

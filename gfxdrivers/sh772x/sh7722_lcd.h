#ifndef __SH7722__LCD_H__
#define __SH7722__LCD_H__

#include "sh7722_types.h"


void sh7722_lcd_setup( void                  *drv,
                       int                    width,
                       int                    height,
                       ulong                  phys,
                       int                    pitch,
                       DFBSurfacePixelFormat  format,
                       bool                   swap );


#endif


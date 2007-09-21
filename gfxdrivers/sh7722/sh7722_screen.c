#ifdef SH7722_DEBUG
#define DIRECT_FORCE_DEBUG
#endif


#include <config.h>

#include <stdio.h>

#include <sys/mman.h>

#include <asm/types.h>

#include <directfb.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>


#include "sh7722.h"
#include "sh7722_screen.h"


D_DEBUG_DOMAIN( SH7722_Screen, "SH7722/Screen", "Renesas SH7722 Screen" );

/**********************************************************************************************************************/

static DFBResult
sh7722InitScreen( CoreScreen           *screen,
               CoreGraphicsDevice       *device,
               void                 *driver_data,
               void                 *screen_data,
               DFBScreenDescription *description )
{
     D_DEBUG_AT( SH7722_Screen, "%s()\n", __FUNCTION__ );

     /* Set the screen capabilities. */
     description->caps = DSCCAPS_NONE;

     /* Set the screen name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "SH7722 Screen" );

     return DFB_OK;
}

static DFBResult
sh7722GetScreenSize( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data,
                  int        *ret_width,
                  int        *ret_height )
{
     D_DEBUG_AT( SH7722_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_width != NULL );
     D_ASSERT( ret_height != NULL );

     *ret_width  = SH7722_LCD_WIDTH;
     *ret_height = SH7722_LCD_HEIGHT;

     return DFB_OK;
}

ScreenFuncs sh7722ScreenFuncs = {
     InitScreen:    sh7722InitScreen,
     GetScreenSize: sh7722GetScreenSize
};


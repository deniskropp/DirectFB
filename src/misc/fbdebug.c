/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   Balanced binary tree ported from glib-2.0.

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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <pthread.h>

#include <core/fusion/lock.h>
#include <core/coredefs.h>

#include <directfb.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/mem.h>

#ifdef DFB_DEBUG

typedef struct {
     unsigned int x;
     unsigned int y;
     unsigned int width;
     unsigned int height;

     unsigned int x2;
     unsigned int y2;
} FBDebugArea;

static int   fbdebug_compatible_format( int al, int rl, int gl, int bl,
                                        int ao, int ro, int go, int bo );

static void *fbdebug_area_data        ( FBDebugArea  *area,
                                        unsigned int  x,
                                        unsigned int  y,
                                        unsigned int  width,
                                        unsigned int  height );

static int fd = -1;

static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;

static __u8 *data;
static int   bpp;
static int   pitch;


DFBResult
fbdebug_init()
{
     if (fd > -1)
          return DFB_OK;

     if (!dfb_config->fbdebug_device)
          return DFB_OK;

     fd = open( dfb_config->fbdebug_device, O_RDWR );
     if (fd < 0) {
          PERRORMSG( "DirectFB/FBDebug: Could not open '%s'!\n",
                     dfb_config->fbdebug_device );
          return DFB_INIT;
     }

     if (ioctl( fd, FBIOGET_VSCREENINFO, &var )) {
          PERRORMSG( "DirectFB/FBDebug: FBIOGET_VSCREENINFO on '%s' failed!\n",
                     dfb_config->fbdebug_device );
          goto error_close;
     }

     if (!fbdebug_compatible_format( 0, 5, 6, 5,  0, 11, 5, 0 )) {
          var.bits_per_pixel = 16;
          ioctl( fd, FBIOPUT_VSCREENINFO, &var );
     }

     if (!fbdebug_compatible_format( 0, 5, 6, 5,  0, 11, 5, 0 )) {
          ERRORMSG( "DirectFB/FBDebug: Only RGB16 supported yet!\n" );
          goto error_close;
     }

     if (ioctl( fd, FBIOGET_FSCREENINFO, &fix )) {
          PERRORMSG( "DirectFB/FBDebug: FBIOGET_FSCREENINFO on '%s' failed!\n",
                     dfb_config->fbdebug_device );
          goto error_close;
     }

     data = mmap( NULL, fix.smem_len,
                  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
     if ((int)data == -1) {
          PERRORMSG( "DirectFB/FBDebug: Could not mmap '%s'!\n",
                     dfb_config->fbdebug_device );
          goto error_close;
     }

     memset( data, 0, fix.smem_len );

     bpp   = (var.bits_per_pixel+7) / 8;
     pitch = fix.line_length;

     INITMSG( "DirectFB/FBDebug: Initialized, resolution %dx%d, pitch %d.\n",
              var.xres, var.yres, pitch );

     return DFB_OK;


error_close:
     close( fd );
     fd = -1;

     return DFB_INIT;
}

void
fbdebug_exit()
{
     if (fd < 0)
          return;

     munmap( data, fix.smem_len );

     close( fd );
     fd = -1;
}

void
fbdebug_get_size( unsigned int *width,
                  unsigned int *height )
{
     if (!width || !height)
          return;

     if (fd < 0) {
          *width  = 640;
          *height = 480;
     }
     else {
          *width  = var.xres;
          *height = var.yres;
     }
}

DFBResult
fbdebug_get_area( unsigned int   x,
                  unsigned int   y,
                  unsigned int   width,
                  unsigned int   height,
                  FBDebugArea  **area )
{
     int          x2, y2;
     FBDebugArea *a;

     if (!area)
          return DFB_INVARG;

     if (fd < 0) {
          *area = NULL;
          return DFB_OK;
     }

     x2 = x + width  - 1;
     y2 = y + height - 1;

     if (x >= var.xres || y >= var.yres || x2 >= var.xres || y2 >= var.yres)
          return DFB_INVAREA;

     a = DFBCALLOC( 1, sizeof(FBDebugArea) );

     a->x      = x;
     a->y      = y;
     a->width  = width;
     a->height = height;
     a->x2     = x2;
     a->y2     = y2;

     *area = a;

     return DFB_OK;
}

void
fbdebug_free_area( FBDebugArea *area )
{
     if (!area)
          return;

     DFBFREE( area );
}

DFBResult
fbdebug_fill( FBDebugArea  *area,
              unsigned int  x,
              unsigned int  y,
              unsigned int  width,
              unsigned int  height,
              __u8          r,
              __u8          g,
              __u8          b )
{
     __u16 *dst;
     __u16  col;

     if (fd < 0)
          return DFB_OK;

     if (!area)
          return DFB_INVARG;

     if (!width || !height) {
          x = y  = 0;
          width  = area->width;
          height = area->height;
     }

     dst = fbdebug_area_data( area, x, y, width, height );
     if (!dst)
          return DFB_INVARG;

     col = PIXEL_RGB16( r, g, b );

     while (height--) {
          unsigned int i;

          for (i=0; i<width; i++)
               dst[i] = col;

          dst += var.xres_virtual;
     }

     return DFB_OK;
}




/* file internal */

static int fbdebug_compatible_format( int al, int rl, int gl, int bl,
                                      int ao, int ro, int go, int bo )
{
     int ah, rh, gh, bh;
     int vah, vrh, vgh, vbh;

     ah = al + ao - 1;
     rh = rl + ro - 1;
     gh = gl + go - 1;
     bh = bl + bo - 1;

     vah = var.transp.length + var.transp.offset - 1;
     vrh = var.red.length + var.red.offset - 1;
     vgh = var.green.length + var.green.offset - 1;
     vbh = var.blue.length + var.blue.offset - 1;

     if (ah == vah && al >= var.transp.length &&
         rh == vrh && rl >= var.red.length &&
         gh == vgh && gl >= var.green.length &&
         bh == vbh && bl >= var.blue.length)
          return 1;

     return 0;
}

static void *
fbdebug_area_data( FBDebugArea  *area,
                   unsigned int  x,
                   unsigned int  y,
                   unsigned int  width,
                   unsigned int  height )
{
     int x1 = area->x + x;
     int y1 = area->y + y;
     int x2 = x1 + width  - 1;
     int y2 = y1 + height - 1;

     if (x1 > area->x2 || y1 > area->y2 || x2 > area->x2 || y2 > area->y2)
          return NULL;

     return (void*)(data + y1*pitch + x1*bpp);
}

#endif


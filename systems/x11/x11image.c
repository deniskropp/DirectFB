/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "x11.h"
#include "x11image.h"

/**********************************************************************************************************************/

DFBResult x11ImageInit( DFBX11                *x11,
                        x11Image              *image,
                        int                    width,
                        int                    height,
                        DFBSurfacePixelFormat  format )
{
     int           ret;
     Visual       *visual;
     DFBX11Shared *shared = x11->shared;

     if (!x11->use_shm)
          return DFB_UNSUPPORTED;

     /* Lookup visual. */
     visual = x11->visuals[DFB_PIXELFORMAT_INDEX(format)];
     if (!visual)
          return DFB_UNSUPPORTED;

     /* For probing. */
     if (!image)
          return DFB_OK;

     image->width  = width;
     image->height = height;
     image->format = format;
     image->depth  = DFB_COLOR_BITS_PER_PIXEL( format );

     D_MAGIC_SET( image, x11Image );

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_IMAGE_INIT, image, &ret )) {
          D_MAGIC_CLEAR( image );
          return DFB_FUSION;
     }

     if (ret) {
          D_DERROR( ret, "X11/Image: X11_IMAGE_INIT call failed!\n" );
          D_MAGIC_CLEAR( image );
          return ret;
     }

     return DFB_OK;
}

DFBResult
x11ImageDestroy( DFBX11   *x11,
                 x11Image *image )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     D_MAGIC_ASSERT( image, x11Image );

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_IMAGE_DESTROY, image, &ret ))
          return DFB_FUSION;

     if (ret) {
          D_DERROR( ret, "X11/Image: X11_IMAGE_DESTROY call failed!\n" );
          return ret;
     }

     D_MAGIC_CLEAR( image );

     return DFB_OK;
}

DFBResult
x11ImageAttach( x11Image  *image,
                void     **ret_addr )
{
     void *addr;

     D_MAGIC_ASSERT( image, x11Image );
     D_ASSERT( ret_addr != NULL );

     /* FIXME: We also need to DETACH! */

     addr = shmat( image->seginfo.shmid, NULL, 0 );
     if (!addr) {
          int erno = errno;

          D_PERROR( "X11/Image: shmat( %d ) failed!\n", image->seginfo.shmid );

          return errno2result( erno );
     }

     *ret_addr = addr;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_x11_image_init_handler( DFBX11 *x11, x11Image *image )
{
     Visual *visual;
     XImage *ximage;

     D_MAGIC_ASSERT( image, x11Image );

     if (!x11->use_shm)
          return DFB_UNSUPPORTED;

     /* Lookup visual. */
     visual = x11->visuals[DFB_PIXELFORMAT_INDEX(image->format)];
     if (!visual)
          return DFB_UNSUPPORTED;

     image->visual = visual;

     XLockDisplay( x11->display );

     ximage = XShmCreateImage( x11->display, image->visual, image->depth,
                               ZPixmap, NULL, &image->seginfo, image->width, image->height );
     if (!ximage) {
          D_ERROR( "X11/ShmImage: Error creating shared image (XShmCreateImage)!\n");
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     /* we firstly create our shared memory segment with the size we need, and
      correct permissions for the owner, the group and the world --> 0777 */
     image->seginfo.shmid = shmget( IPC_PRIVATE, 
                                    ximage->bytes_per_line * ximage->height,
                                    IPC_CREAT | 0777 );
     if (image->seginfo.shmid < 0)
          goto error;

     /* Then, we have to attach the segment to our process, and we let the
        function search the correct memory place --> NULL. It's safest ! */
     image->seginfo.shmaddr = shmat( image->seginfo.shmid, NULL, 0 );
     if (!image->seginfo.shmaddr)
          goto error_shmat;

     ximage->data = image->seginfo.shmaddr;

     /* We set the buffer in Read and Write mode */
     image->seginfo.readOnly = False;

     if (!XShmAttach( x11->display, &image->seginfo ))
          goto error_xshmattach;

     image->ximage = ximage;
     image->pitch  = ximage->bytes_per_line;

     image->pixmap = XShmCreatePixmap( x11->display, DefaultRootWindow(x11->display), ximage->data,
                                       &image->seginfo, image->width, image->height, image->depth );

     image->gc = XCreateGC( x11->display, image->pixmap, 0, NULL );

     XUnlockDisplay( x11->display );

     return DFB_OK;


error_xshmattach:
     shmdt( image->seginfo.shmaddr );

error_shmat:
     shmctl( image->seginfo.shmid, IPC_RMID, NULL );

error:
     XDestroyImage( ximage );

     XUnlockDisplay( x11->display );

     return DFB_FAILURE;
}

DFBResult
dfb_x11_image_destroy_handler( DFBX11 *x11, x11Image *image )
{
     D_MAGIC_ASSERT( image, x11Image );

     XLockDisplay( x11->display );

     XFreeGC( x11->display, image->gc );
     XFreePixmap( x11->display, image->pixmap );

     XShmDetach( x11->display, &image->seginfo );

     XDestroyImage( image->ximage );

     XUnlockDisplay( x11->display );

     shmdt( image->seginfo.shmaddr );

     shmctl( image->seginfo.shmid, IPC_RMID, NULL );

     return DFB_OK;
}


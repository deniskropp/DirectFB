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

#include <direct/mem.h>

#include "x11.h"

extern DFBX11  *dfb_x11;
extern CoreDFB *dfb_x11_core;


static int
error_handler( Display *display, XErrorEvent *event )
{
     if (dfb_x11->use_shm) {
          D_INFO( "X11/Display: Error! Disabling XShm.\n" );

          dfb_x11->use_shm = false;
     }

     return 0;
}

Bool
dfb_x11_open_window(XWindow** ppXW, int iXPos, int iYPos, int iWidth, int iHeight)
{
     XWindow* xw = (XWindow *)calloc(1, sizeof(XWindow));

     /* We set the structure as needed for our window */
     xw->width   = iWidth;
     xw->height  = iHeight;
     xw->display = dfb_x11->display;

     xw->screenptr = DefaultScreenOfDisplay(xw->display);
     xw->screennum = DefaultScreen(xw->display);
     xw->visual    = DefaultVisualOfScreen(xw->screenptr);
     xw->depth     = DefaultDepth( xw->display, xw->screennum );

     XLockDisplay( dfb_x11->display );

     xw->window = XCreateWindow( xw->display,
                                 RootWindowOfScreen(xw->screenptr),
                                 iXPos, iYPos, iWidth, iHeight, 0, xw->depth, InputOutput,
                                 xw->visual, 0, NULL );

     if (!xw->window) {
          free( xw );
          XUnlockDisplay( dfb_x11->display );
          return False;
     }


     XSizeHints Hints;

     /*
      * Here we inform the function of what we are going to change for the
      * window (there's also PPosition but it's obsolete)
      */
     Hints.flags    =    PSize | PMinSize | PMaxSize;

     /*
      * Now we set the structure to the values we need for width & height.
      * For esthetic reasons we set Width=MinWidth=MaxWidth.
      * The same goes for Height. You can try whith differents values, or
      * let's use Hints.flags=Psize; and resize your window..
      */
     Hints.min_width          =    Hints.max_width          =    Hints.base_width    =    xw->width;
     Hints.min_height    =    Hints.max_height    =    Hints.base_height   =    xw->height;

     /* Now we can set the size hints for the specified window */
     XSetWMNormalHints(xw->display,xw->window,&Hints);

     /* We change the title of the window (default:Untitled) */
     XStoreName(xw->display,xw->window,"DFB X11 system window");



     XSelectInput( xw->display, xw->window,
                   ExposureMask|KeyPressMask|KeyReleaseMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask );


     xw->gc = XCreateGC(xw->display, xw->window, 0, NULL);



     // Create a null cursor
     XColor  fore;
     XColor  back;
     char    zero = 0;

     xw->pixmp1     = XCreateBitmapFromData( xw->display, xw->window, &zero, 1, 1 );
     xw->pixmp2     = XCreateBitmapFromData( xw->display, xw->window, &zero, 1, 1 );

     xw->NullCursor = XCreatePixmapCursor( xw->display, xw->pixmp1, xw->pixmp2, &fore, &back, 0, 0 );

     XDefineCursor( xw->display, xw->window, xw->NullCursor );


     /* maps the window and raises it to the top of the stack */
     XMapRaised( xw->display, xw->window );


     dfb_x11->use_shm = XShmQueryExtension(dfb_x11->display);

     if (dfb_x11->use_shm) {
          // Shared memory 	
          xw->shmseginfo=(XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));
          if (!xw->shmseginfo) {
               dfb_x11->use_shm = false;
               goto no_shm;
          }

          memset(xw->shmseginfo,0, sizeof(XShmSegmentInfo));

          xw->ximage=XShmCreateImage(xw->display, xw->visual, xw->depth, ZPixmap,
                                     NULL,xw->shmseginfo, xw->width, xw->height * 2);
          if (!xw->ximage) {
               D_ERROR("X11: Error creating shared image (XShmCreateImage) \n");
               dfb_x11->use_shm = false;
               free(xw->shmseginfo);
               goto no_shm;
          }

          xw->bpp = (xw->ximage->bits_per_pixel + 7) / 8;

          /* we firstly create our shared memory segment with the size we need, and
          correct permissions for the owner, the group and the world --> 0777 */
          xw->shmseginfo->shmid=shmget(IPC_PRIVATE, 
                                       xw->ximage->bytes_per_line * xw->ximage->height * 2,
                                       IPC_CREAT|0777);

          if (xw->shmseginfo->shmid<0) {
               dfb_x11->use_shm = false;
               XDestroyImage(xw->ximage);
               free(xw->shmseginfo);
               goto no_shm;
          }

          /* Then, we have to attach the segment to our process, and we let the
          function search the correct memory place --> NULL. It's safest ! */
          xw->shmseginfo->shmaddr = shmat( xw->shmseginfo->shmid, NULL, 0 );
          if (!xw->shmseginfo->shmaddr) {
               dfb_x11->use_shm = false;
               shmctl(xw->shmseginfo->shmid,IPC_RMID,NULL);
               XDestroyImage(xw->ximage);
               free(xw->shmseginfo);
               goto no_shm;
          }

          /* We set the buffer in Read and Write mode */
          xw->shmseginfo->readOnly=False;

          xw->virtualscreen= xw->ximage->data = xw->shmseginfo->shmaddr;


          XSetErrorHandler( error_handler );

          XShmAttach(dfb_x11->display,xw->shmseginfo);

          XShmPutImage(dfb_x11->display, xw->window, xw->gc, xw->ximage,
                       0, 0, 0, 0, 1, 1, False);

          XSync(dfb_x11->display, False);

          XSetErrorHandler( NULL );

          if (!dfb_x11->use_shm) {
               shmdt(xw->shmseginfo->shmaddr);
               shmctl(xw->shmseginfo->shmid,IPC_RMID,NULL);
               XDestroyImage(xw->ximage);
               free(xw->shmseginfo);
          }
     }

no_shm:
     if (!dfb_x11->use_shm) {
          int pitch;

          xw->bpp = xw->depth / 8;

          pitch = (xw->bpp * xw->width + 3) & ~3;

          xw->virtualscreen = D_MALLOC( 2 * xw->height * pitch );

          xw->ximage=XCreateImage(xw->display, xw->visual, xw->depth, ZPixmap, 0,
                                  xw->virtualscreen,
                                  xw->width, xw->height * 2, 32, pitch);
          if (!xw->ximage) {
               D_ERROR("X11: Error creating image (XCreateImage) \n");
               XFreeGC(xw->display,xw->gc);
               XDestroyWindow(xw->display,xw->window);
               XUnlockDisplay( dfb_x11->display );
               free( xw );
               return False;
          }
     }

     XUnlockDisplay( dfb_x11->display );

     D_INFO( "X11/Display: %ssing XShm.\n", dfb_x11->use_shm ? "U" : "Not u" );

     (*ppXW) = xw;

     return True;
}

void
dfb_x11_close_window( XWindow* xw )
{
     XLockDisplay( dfb_x11->display );

     if (dfb_x11->use_shm) {
          XShmDetach(xw->display, xw->shmseginfo);
          shmdt(xw->shmseginfo->shmaddr);
          shmctl(xw->shmseginfo->shmid,IPC_RMID,NULL);
          free(xw->shmseginfo);
     }
     else
          D_FREE( xw->virtualscreen );

     XDestroyImage(xw->ximage);

     XFreeGC(xw->display,xw->gc);
     XDestroyWindow(xw->display,xw->window);

     XUnlockDisplay( dfb_x11->display );

     free(xw);
}


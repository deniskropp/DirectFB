/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __SYSTEMS_XWINDOW_H__
#define __SYSTEMS_XWINDOW_H__

#include <X11/Xlib.h>    /* fundamentals X datas structures */
#include <X11/Xutil.h>   /* datas definitions for various functions */
#include <X11/keysym.h>  /* for a perfect use of keyboard events */

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
 


typedef struct 
{
	Display*			display;
	Window 				window;
	Screen*				screenptr;
	int 				screennum;
	Visual*				visual;
	GC 					gc;
	XImage*				ximage;
	Colormap 			colormap;

	Pixmap 				pixmap;
	XShmSegmentInfo*	shmseginfo;
	unsigned char*		videomemory;

	unsigned char*		virtualscreen;
	int 				videoaccesstype;

	int 				width;
	int 				height;
	int 				depth;
	int 				pixelsize;
	int 				screensize;
	/* (Null) cursor stuff*/
	Pixmap  			pixmp1;
	Pixmap  			pixmp2;
	Cursor 				NullCursor;
	
} XWindow;

void 	xw_reset(XWindow* xw);
Bool 	xw_setPixelSize(XWindow* xw);
void 	xw_clearScreen(XWindow* xw);
void 	xw_setPixel(XWindow* xw, int iXPos, int iYPos, int iColor);
void 	xw_closeWindow(XWindow** ppXW);
Bool 	xw_openWindow(XWindow** ppXW, int iXPos, int iYPos, int iWidth, int iHeight, int iDepth);



#endif /* __SYSTEMS_XWINDOW_H__ */


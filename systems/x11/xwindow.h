#ifndef __SYSTEMS_XWINDOW_H__
#define __SYSTEMS_XWINDOW_H__

#include <X11/Xlib.h>    /* fundamentals X datas structures */
#include <X11/Xutil.h>   /* datas definitions for various functions */
#include <X11/keysym.h>  /* for a perfect use of keyboard events */

#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
 
static const int g_iNeededDepth = 24;


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


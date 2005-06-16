#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "x11.h"


extern DFBX11 *dfb_x11;


void xw_reset(XWindow* xw)
{
	memset( xw, 0, sizeof(XWindow) );
}


Bool xw_setPixelSize(XWindow* xw)
{
	if(xw->depth!=DefaultDepth(xw->display,DefaultScreen(xw->display)))
	{
		fprintf(stderr,"Please, I need a %d bits display\n",xw->depth);
		exit(1);
	}

    /*
	* a very dirty way to know the pixelsize. It's better to use XVisualInfo.
	* I'll search informations for this critical and important part.
	*/
	switch(xw->depth)
	{
		case 8:
			xw->pixelsize=1;
			break;
		case 16:
			xw->pixelsize=2;
			break;
		case 24:
			xw->pixelsize=4;
			break;
		case 32:
			xw->pixelsize=4;
			break;
		default:
			xw->pixelsize=1;
	}
	return True;
}



void xw_clearScreen(XWindow* xw)
{
	memset(xw->virtualscreen, 127, xw->width*xw->height*xw->pixelsize);
}

void xw_setPixel(XWindow* xw, int iXPos, int iYPos, int iColor)
{
	//	xw->virtualscreen[(iYPos*xw->width+iXPos)<<1] = (int)iColor;   /* (y*width+x)*pixelsize */
	int* pRGBBuf = (int*)xw->virtualscreen;
	pRGBBuf[(iYPos*xw->width+iXPos)] = iColor;
	// xw->virtualscreen[(iYPos*xw->width*xw->pixelsize+iXPos)<<1] = iColor;   /* (y*width+x)*pixelsize */
}


/** Closes and deallocates an allready opened window. Use fx. &pXWindow where pXWindow is declared as 
	'XWindow* pXWindow'. Ie. call like: 'xw_closeWindow(&pXWindow) */
void xw_closeWindow(XWindow** ppXW)
{
	if ( *ppXW )
	{
		XWindow* xw	= (*ppXW);
		XShmDetach(xw->display, xw->shmseginfo);
		if ( xw->ximage ) XDestroyImage(xw->ximage);
		shmdt(xw->shmseginfo->shmaddr);
		shmctl(xw->shmseginfo->shmid,IPC_RMID,NULL);
		free(xw->shmseginfo);
		XFreeGC(xw->display,xw->gc);
		XDestroyWindow(xw->display,xw->window);
		XCloseDisplay(xw->display);
		free(xw);
	}
}




/** Creates and open a window. Use fx. &pXWindow where pXWindow is declared as 
	'XWindow* pXWindow'. Ie. call like: 'xw_closeWindow(&pXWindow). */
Bool xw_openWindow(XWindow** ppXW, int iXPos, int iYPos, int iWidth, int iHeight, int iDepth)
{
	
	(*ppXW) 	= (XWindow *)malloc(sizeof(XWindow));
	XWindow* xw	= (*ppXW);
	dfb_x11->xw = xw;
	xw_reset(xw);

	/* We set the structure as needed for our window */
	xw->width	= iWidth;
	xw->height	= iHeight;
	xw->depth	= iDepth;

	xw->display = XOpenDisplay(NULL);
	if( !xw->display ) {
		printf("X11: Error opening X_Display\n");
		return False;
	}
	
	xw_setPixelSize(xw);

	xw->screenptr	= DefaultScreenOfDisplay(xw->display);
	xw->screennum	= DefaultScreen(xw->display);
	xw->visual		= DefaultVisualOfScreen(xw->screenptr);

	
	xw->window=XCreateWindow(xw->display,
							 RootWindowOfScreen(xw->screenptr),
							 iXPos, iYPos, iWidth, iHeight, 0, iDepth, InputOutput,
							 xw->visual, 0, NULL);
	if(!xw->window) return False;


	XSizeHints Hints;

    /*
	* Here we inform the function of what we are going to change for the
	* window (there's also PPosition but it's obsolete)
	*/
	Hints.flags	=	PSize | PMinSize | PMaxSize;

    /*
	* Now we set the structure to the values we need for width & height.
	* For esthetic reasons we set Width=MinWidth=MaxWidth.
	* The same goes for Height. You can try whith differents values, or
	* let's use Hints.flags=Psize; and resize your window..
	*/
	Hints.min_width		=	Hints.max_width		=	Hints.base_width	=	xw->width;
	Hints.min_height	=	Hints.max_height	=	Hints.base_height	=	xw->height;

	/* Now we can set the size hints for the specified window */
	XSetWMNormalHints(xw->display,xw->window,&Hints);

	/* We change the title of the window (default:Untitled) */
	XStoreName(xw->display,xw->window,"DFB X11 system window");



	XSelectInput(xw->display, xw->window,
				 ExposureMask|KeyPressMask|KeyReleaseMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask);


	xw->gc=XCreateGC(xw->display, xw->window, 0, NULL);



	/* maps the window and raises it to the top of the stack */
	XMapRaised(xw->display, xw->window);

	
	// Shared memory 	
	xw->shmseginfo=(XShmSegmentInfo *)malloc(sizeof(XShmSegmentInfo));
	if(!xw->shmseginfo) return False;

	memset(xw->shmseginfo,0, sizeof(XShmSegmentInfo));

	xw->ximage=XShmCreateImage(xw->display, xw->visual, xw->depth, ZPixmap,
							   NULL,xw->shmseginfo, xw->width, xw->height);
	if(!xw->ximage) {
		printf("X11: Error creating shared image (XShmCreateImage) \n");
		return False;
	}
	
	
    /* we firstly create our shared memory segment with the size we need, and
	correct permissions for the owner, the group and the world --> 0777 */
	xw->shmseginfo->shmid=shmget(IPC_PRIVATE, 
								 xw->ximage->bytes_per_line * xw->ximage->height,
								 IPC_CREAT|0777);

	if(xw->shmseginfo->shmid<0) return False;

    /* Then, we have to attach the segment to our process, and we let the
	function search the correct memory place --> NULL. It's safest ! */
	xw->shmseginfo->shmaddr = shmat( xw->shmseginfo->shmid, NULL, 0 );
	if(!xw->shmseginfo->shmaddr)  return False;

	/* We set the buffer in Read and Write mode */
	xw->shmseginfo->readOnly=False;

	xw->virtualscreen= (unsigned char *) (xw->ximage->data = xw->shmseginfo->shmaddr );
	if(!XShmAttach(xw->display,xw->shmseginfo))  return False;
	
	return True;
}






/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>

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

#ifndef DFBPP_H
#define DFBPP_H

#ifdef __DIRECTFB_H__
#error Please include '++dfb.h' before 'directfb.h'.
#endif

#include <iostream>

#define IDirectFB              IDirectFB_C
#define IDirectFBScreen        IDirectFBScreen_C
#define IDirectFBDisplayLayer  IDirectFBDisplayLayer_C
#define IDirectFBSurface       IDirectFBSurface_C
#define IDirectFBPalette       IDirectFBPalette_C
#define IDirectFBWindow        IDirectFBWindow_C
#define IDirectFBInputDevice   IDirectFBInputDevice_C
#define IDirectFBEventBuffer   IDirectFBEventBuffer_C
#define IDirectFBFont          IDirectFBFont_C
#define IDirectFBImageProvider IDirectFBImageProvider_C
#define IDirectFBVideoProvider IDirectFBVideoProvider_C
#define IDirectFBDataBuffer    IDirectFBDataBuffer_C

#include <directfb.h>

#undef IDirectFB
#undef IDirectFBScreen
#undef IDirectFBDisplayLayer
#undef IDirectFBSurface
#undef IDirectFBPalette
#undef IDirectFBWindow
#undef IDirectFBInputDevice
#undef IDirectFBEventBuffer
#undef IDirectFBFont
#undef IDirectFBImageProvider
#undef IDirectFBVideoProvider
#undef IDirectFBDataBuffer

class DirectFB;
class IDirectFB;
class IDirectFBScreen;
class IDirectFBDisplayLayer;
class IDirectFBSurface;
class IDirectFBPalette;
class IDirectFBWindow;
class IDirectFBInputDevice;
class IDirectFBEventBuffer;
class IDirectFBFont;
class IDirectFBImageProvider;
class IDirectFBVideoProvider;
class IDirectFBDataBuffer;



template <class IMPLEMENTINGCLASS, class IPPAny_C>
class IPPAny
{
	protected:
		IPPAny(IPPAny_C *iface) {
			this->iface = iface;
		}
		inline IPPAny_C *get_iface() { return iface; }
		inline IPPAny_C *get_iface() const { return iface; }
		
	protected:
		IPPAny_C* iface;
	public:
		IPPAny(){
			iface = NULL;
		}
		
		IPPAny(const IMPLEMENTINGCLASS &other) {
			IPPAny_C *other_iface = other.iface;
			if (other_iface)
				other_iface->AddRef( other_iface );
			iface = other_iface;
		}
		
		virtual ~IPPAny() {
			if (iface)
				iface->Release( iface );
		}
		inline operator IMPLEMENTINGCLASS*() {
			return dynamic_cast<IMPLEMENTINGCLASS*>(this);
		}
		inline operator IMPLEMENTINGCLASS*() const{
 			return dynamic_cast<IMPLEMENTINGCLASS*> (this);
 		}
		inline operator bool() {
			return iface != NULL;
		}
		inline IMPLEMENTINGCLASS &operator = (const IMPLEMENTINGCLASS &other) {
			IPPAny_C *other_iface = other.iface;

			if (other_iface)
				other_iface->AddRef( other_iface );
			if (iface)
				iface->Release( iface );
			iface = other_iface;
			return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
		}
		inline IMPLEMENTINGCLASS &operator = (IPPAny_C *other_iface) {
			if (other_iface)
				other_iface->AddRef( other_iface );
			if (iface)
				iface->Release( iface );
			iface = other_iface;
			return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
		}
};

#include "idirectfb.h"
#include "idirectfbscreen.h"
#include "idirectfbdisplaylayer.h"
#include "idirectfbsurface.h"
#include "idirectfbpalette.h"
#include "idirectfbwindow.h"
#include "idirectfbinputdevice.h"
#include "idirectfbeventbuffer.h"
#include "idirectfbfont.h"
#include "idirectfbimageprovider.h"
#include "idirectfbvideoprovider.h"
#include "idirectfbdatabuffer.h"


#define DFB_ADD_SURFACE_DESC(d,f)   (d) = (DFBSurfaceDescriptionFlags)  ((d) | (f))
#define DFB_ADD_SURFACE_CAPS(c,f)   (c) = (DFBSurfaceCapabilities)      ((c) | (f))
#define DFB_ADD_DRAWING_FLAG(d,f)   (d) = (DFBSurfaceDrawingFlags)      ((d) | (f))
#define DFB_ADD_BLITTING_FLAG(b,f)  (b) = (DFBSurfaceBlittingFlags)     ((b) | (f))


class DirectFB {
public:
     static void      Init   (int *argc = NULL, char *(*argv[]) = NULL);
     static IDirectFB Create ();
};

class DFBException {
public:
     DFBException (const char *action, DFBResult result_code);

     const char *GetAction() const;
     const char *GetResult() const;
     DFBResult   GetResultCode() const;

     friend std::ostream &operator << (std::ostream &stream, DFBException *ex);

private:
     const char *action;
     DFBResult   result_code;
};



#endif

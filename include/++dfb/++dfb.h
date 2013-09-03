/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


#ifndef DFBPP_H
#define DFBPP_H

#ifdef __DIRECTFB_H__
#error Please include '++dfb.h' before 'directfb.h'.
#endif


#ifdef WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the PPDFB_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// PPDFB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef PPDFB_EXPORTS
#define PPDFB_API __declspec(dllexport)
#else
#define PPDFB_API __declspec(dllimport)
#endif
#else
#define PPDFB_API
#endif


#include <iostream>

#include "++dfb_mangle.h"
#include <directfb++.h>
#include "++dfb_unmangle.h"


class DFBException {
public:
     PPDFB_API DFBException (const char *action, DFBResult result_code);

     const char PPDFB_API *GetAction() const;
     const char PPDFB_API *GetResult() const;
     DFBResult  PPDFB_API  GetResultCode() const;

     friend std::ostream PPDFB_API &operator << (std::ostream &stream, DFBException *ex);

private:
     const char *action;
     DFBResult   result_code;
};


#define PPDFB_DFBCHECK(...)                                           \
     do {                                                             \
          DFBResult ret = (DFBResult)(__VA_ARGS__);                   \
                                                                      \
          if (ret)                                                    \
               throw new DFBException (__FUNCTION__, ret);            \
     } while (0);


template <class IMPLEMENTINGCLASS, class IPPAny_C>
class IPPAny
{
	protected:
		IPPAny(IPPAny_C *iface) {
			this->iface = iface;
		}
		inline IPPAny_C *get_iface() { return iface; }
		inline IPPAny_C *get_iface() const { return iface; }

	public:
		IPPAny_C* iface;
	public:
		IPPAny(){
			iface = NULL;
		}

		IPPAny(const IPPAny &other) {
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
			IPPAny_C *old_iface   = iface;
			IPPAny_C *other_iface = other.iface;
			if (other_iface)
				PPDFB_DFBCHECK( other_iface->AddRef( other_iface ) );
			iface = other_iface;
			if (old_iface)
				PPDFB_DFBCHECK( old_iface->Release( old_iface ) );
			//return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
			return reinterpret_cast<IMPLEMENTINGCLASS&>(*this);
		}
		inline IMPLEMENTINGCLASS &operator = (IPPAny_C *other_iface) {
               IPPAny_C *old_iface = iface;
			if (other_iface)
				PPDFB_DFBCHECK( other_iface->AddRef( other_iface ) );
			iface = other_iface;
			if (old_iface)
				PPDFB_DFBCHECK( old_iface->Release( old_iface ) );
			//return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
			return reinterpret_cast<IMPLEMENTINGCLASS&>(*this);
		}
};




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


namespace DirectFB {
     void      PPDFB_API Init   (int *argc = NULL, char *(*argv[]) = NULL);
     IDirectFB PPDFB_API Create ();
}


#endif

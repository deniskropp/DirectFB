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

#define DFBPoint     DFBPoint_C
#define DFBDimension DFBDimension_C
#define DFBRectangle DFBRectangle_C
#define DFBRegion    DFBRegion_C


#include <directfb.h>
#include <directfb_util.h>

extern "C" {
#include <directfb_strings.h>
}

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

#undef DFBPoint
#undef DFBDimension
#undef DFBRectangle
#undef DFBRegion



class DFBPoint : public DFBPoint_C {
public:
     DFBPoint() {
          x = 0;
          y = 0;
     }

     DFBPoint( const int &_x, const int &_y ) {
          x = _x;
          y = _y;
     }

     DFBPoint( const DFBPoint_C &point ) {
          x = point.x;
          y = point.y;
     }

     DFBPoint( const DFBRectangle_C &rectangle ) {
          x = rectangle.x;
          y = rectangle.y;
     }

     DFBPoint( const DFBRegion_C &region ) {
          x = region.x1;
          y = region.y1;
     }

     bool operator== ( const DFBPoint &ref ) const {
          return ref.x == x && ref.y == y;
     }

     DFBPoint operator +( const DFBPoint &offset ) const {
          DFBPoint p( *this );
          p.x += offset.x;
          p.y += offset.y;
          return p;
     }

     DFBPoint& operator +=( const DFBPoint& offset ) {
          x += offset.x;
          y += offset.y;
          return *this;
     }
};

class DFBDimension : public DFBDimension_C {
public:
     DFBDimension() {
          w = 0;
          h = 0;
     }

     DFBDimension( const int &_w, const int &_h ) {
          w = _w;
          h = _h;
     }

     DFBDimension( const DFBDimension_C &dimension ) {
          w = dimension.w;
          h = dimension.h;
     }

     DFBDimension( const DFBPoint_C &point ) {
          w = point.x;
          h = point.y;
     }

     DFBDimension( const DFBRectangle_C &rectangle ) {
          w = rectangle.w;
          h = rectangle.h;
     }

     DFBDimension( const DFBRegion_C &region ) {
          w = region.x2 - region.x1 + 1;
          h = region.y2 - region.y1 + 1;
     }

     bool operator== ( const DFBDimension &ref ) const {
          return ref.w == w && ref.h == h;
     }

	bool contains( const DFBRegion_C &region ) const {
		if (region.x1 < 0 || region.y1 < 0)
			return false;

		if (region.x2 >= w || region.y2 >= h)
			return false;

		return true;
	}
};



class DFBRectangle : public DFBRectangle_C {
public:
     DFBRectangle() {
          x = 0;
          y = 0;
          w = 0;
          h = 0;
     }

     DFBRectangle( const int &_x, const int &_y, const int &_w, const int &_h ) {
          x = _x;
          y = _y;
          w = _w;
          h = _h;
     }

     DFBRectangle( const DFBRectangle_C &rectangle ) {
          x = rectangle.x;
          y = rectangle.y;
          w = rectangle.w;
          h = rectangle.h;
     }

     DFBRectangle( const DFBRegion_C &region ) {
          x = region.x1;
          y = region.y1;
          w = region.x2 - region.x1 + 1;
          h = region.y2 - region.y1 + 1;
     }

     DFBRectangle( const DFBDimension_C &dimension ) {
          x = 0;
          y = 0;
          w = dimension.w;
          h = dimension.h;
     }

     DFBRectangle( const DFBPoint_C &point, const DFBDimension_C &dimension ) {
          x = point.x;
          y = point.y;
          w = dimension.w;
          h = dimension.h;
     }

     bool operator== ( const DFBRectangle &ref ) const {
          return ref.x == x && ref.y == y && ref.w == w && ref.h == h;
     }

     DFBRectangle& operator-= ( const DFBPoint &sub ) {
          x -= sub.x;
          y -= sub.y;

          return *this;
     }

     DFBRectangle operator -( const DFBPoint &sub ) const {
          return DFBRectangle( x - sub.x, y - sub.y, w, h );
     }

     DFBRectangle operator +( const DFBPoint &offset ) const {
          DFBRectangle r( *this );
          r.x += offset.x;
          r.y += offset.y;
          return r;
     }
};


class DFBRegion : public DFBRegion_C {
public:
     DFBRegion() {
          x1 = 0;
          y1 = 0;
          x2 = 0;
          y2 = 0;
     }

     DFBRegion( const int &_x1, const int &_y1, const int &_x2, const int &_y2 ) {
          x1 = _x1;
          y1 = _y1;
          x2 = _x2;
          y2 = _y2;
     }

     DFBRegion( const DFBRegion_C &region ) {
          x1 = region.x1;
          y1 = region.y1;
          x2 = region.x2;
          y2 = region.y2;
     }

     DFBRegion( const DFBRectangle_C &rectangle ) {
          x1 = rectangle.x;
          y1 = rectangle.y;
          x2 = x1 + rectangle.w - 1;
          y2 = y1 + rectangle.h - 1;
     }

     DFBRegion( const DFBDimension_C &dimension ) {
          x1 = 0;
          y1 = 0;
          x2 = dimension.w - 1;
          y2 = dimension.h - 1;
     }

     DFBRegion( const DFBPoint_C &point, const DFBDimension_C &dimension ) {
          x1 = point.x;
          y1 = point.y;
          x2 = x1 + dimension.w - 1;
          y2 = y1 + dimension.h - 1;
     }

     bool operator== ( const DFBRegion &ref ) const {
          return ref.x1 == x1 && ref.y1 == y1 && ref.x2 == x2 && ref.y2 == y2;
     }

     DFBRegion& operator-= ( const DFBPoint &sub ) {
          x1 -= sub.x;
          y1 -= sub.y;
          x2 -= sub.x;
          y2 -= sub.y;

          return *this;
     }

     DFBRegion operator- ( const DFBPoint &sub ) const {
          return DFBRegion( x1 - sub.x, y1 - sub.y, x2 - sub.x, y2 - sub.y );
     }

     DFBRegion& operator|= ( const DFBRegion &r ) {
          if (r.x1 < x1)
               x1 = r.x1;

          if (r.y1 < y1)
               y1 = r.y1;

          if (r.x2 > x2)
               x2 = r.x2;

          if (r.y2 > y2)
               y2 = r.y2;

          return *this;
     }

     void unionWith ( const DFBRegion &r ) {
          if (r.x1 < x1)
               x1 = r.x1;

          if (r.y1 < y1)
               y1 = r.y1;

          if (r.x2 > x2)
               x2 = r.x2;

          if (r.y2 > y2)
               y2 = r.y2;
     }
};


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
			return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
		}
		inline IMPLEMENTINGCLASS &operator = (IPPAny_C *other_iface) {
               IPPAny_C *old_iface = iface;
			if (other_iface)
				PPDFB_DFBCHECK( other_iface->AddRef( other_iface ) );
			iface = other_iface;
			if (old_iface)
				PPDFB_DFBCHECK( old_iface->Release( old_iface ) );
			return dynamic_cast<IMPLEMENTINGCLASS&>(*this);
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


#define DFB_ADD_SURFACE_DESC(d,f)   (d) = static_cast<DFBSurfaceDescriptionFlags>  ((d) | (f))
#define DFB_ADD_SURFACE_CAPS(c,f)   (c) = static_cast<DFBSurfaceCapabilities>      ((c) | (f))
#define DFB_ADD_DRAWING_FLAG(d,f)   (d) = static_cast<DFBSurfaceDrawingFlags>      ((d) | (f))
#define DFB_ADD_BLITTING_FLAG(b,f)  (b) = static_cast<DFBSurfaceBlittingFlags>     ((b) | (f))


namespace DirectFB {
     void      PPDFB_API Init   (int *argc = NULL, char *(*argv[]) = NULL);
     IDirectFB PPDFB_API Create ();
}


#endif

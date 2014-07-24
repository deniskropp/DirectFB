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


#ifndef DIRECTFBPP_H
#define DIRECTFBPP_H

#include <direct/Types++.h>

#define DFBPoint     DFBPoint_C
#define DFBDimension DFBDimension_C
#define DFBRectangle DFBRectangle_C
#define DFBRegion    DFBRegion_C
#define DFBUpdates   DFBUpdates_C


#include <directfb.h>
#include <directfb_util.h>

extern "C" {
#include <directfb_keynames.h>
#include <directfb_strings.h>
}


#undef DFBPoint
#undef DFBDimension
#undef DFBRectangle
#undef DFBRegion
#undef DFBUpdates



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

     bool operator!= ( const DFBDimension &ref ) const {
          return ref.w != w || ref.h != h;
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

     int width() const {
          return x2 - x1 + 1;
     }

     int height() const {
          return y2 - y1 + 1;
     }

     void translate( int x, int y ) {
          x1 += x;
          y1 += y;
          x2 += x;
          y2 += y;
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
          unionWith( r );

          return *this;
     }

     void unionWith( const DFBRegion &r ) {
          if (r.x1 < x1)
               x1 = r.x1;

          if (r.y1 < y1)
               y1 = r.y1;

          if (r.x2 > x2)
               x2 = r.x2;

          if (r.y2 > y2)
               y2 = r.y2;
     }

     DFBRegion& operator&= ( const DFBRegion &r ) {
          clipBy( r );

          return *this;
     }

     void clipBy( const DFBRegion &r ) {
          if (r.x1 > x1)
               x1 = r.x1;

          if (r.y1 > y1)
               y1 = r.y1;

          if (r.x2 < x2)
               x2 = r.x2;

          if (r.y2 < y2)
               y2 = r.y2;
     }
};


class DFBUpdates : public DFBUpdates_C {
private:
//     int        max_regions;
//     DFBRegion *regions;

public:
     DFBUpdates( int max_regions = 8 )
//          :
//          max_regions( max_regions )
     {
          regions = new DFBRegion[max_regions];

          if (regions)
               dfb_updates_init( this, regions, max_regions );
          else
               D_OOM();
     }

     ~DFBUpdates()
     {
          if (regions) {
               dfb_updates_deinit( this );

               delete regions;
          }
     }

     void Reset()
     {
          D_ASSERT( regions != NULL );

          dfb_updates_reset( this );
     }

     DFBUpdates& operator|= ( const DFBRegion &r )
     {
          D_ASSERT( regions != NULL );

          dfb_updates_add( this, &r );

          return *this;
     }
};



#define DFB_ADD_SURFACE_DESC(d,f)   (d) = static_cast<DFBSurfaceDescriptionFlags>  ((d) | (f))
#define DFB_ADD_SURFACE_CAPS(c,f)   (c) = static_cast<DFBSurfaceCapabilities>      ((c) | (f))
#define DFB_ADD_DRAWING_FLAG(d,f)   (d) = static_cast<DFBSurfaceDrawingFlags>      ((d) | (f))
#define DFB_ADD_BLITTING_FLAG(b,f)  (b) = static_cast<DFBSurfaceBlittingFlags>     ((b) | (f))



#endif

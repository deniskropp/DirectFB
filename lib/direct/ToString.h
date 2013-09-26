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



#ifndef ___Direct__ToString__H___
#define ___Direct__ToString__H___


#include <direct/Types++.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


#include <direct/String.h>


/*
 * In global namespace to allow universal usage without Direct::
 */

template <typename _Entity>
class ToString : public Direct::String
{
public:
     ToString( const _Entity &entity );
};


class FromStringBase
{
public:
     FromStringBase()
          :
          success( false )
     {
     }

     operator bool() const {
          return success;
     }

protected:
     bool success;
};

template <typename _Entity>
class FromString : public FromStringBase
{
public:
     FromString( _Entity &entity, const Direct::String &string );
};



template<>
inline ToString<int>::ToString( const int &n )
{
     PrintF( "%d", n );
}


#define D_FLAG_PRINTFn( __n, __flags, __FLAGS, __FLAG )          \
     if ((__flags) & (__FLAGS##__FLAG))                          \
          PrintF( "%s" #__FLAG, (__n)++ ? "," : "" );


#endif // __cplusplus

#endif


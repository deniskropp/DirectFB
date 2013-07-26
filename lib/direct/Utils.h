/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef ___Direct__Utils__H___
#define ___Direct__Utils__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/os/mutex.h>
#include <direct/os/waitqueue.h>

#include <direct/debug.h>


#ifdef __cplusplus
}

#include <direct/String.h>
#include <direct/ToString.h>

#include <map>
#include <memory>
#include <string>

#include <typeinfo>

#include <cxxabi.h>


D_DEBUG_DOMAIN( Direct_Utils, "Direct/Utils", "Direct Utils" );


namespace Direct {


template <typename _Map>
class MapLookup
{
public:
     operator bool() const {
          return success;
     }

protected:
     bool success;

public:
     MapLookup( const _Map                 &map,
                typename _Map::key_type    &key,
                typename _Map::mapped_type &val )
     {
          typename _Map::const_iterator it = map.find( key );

          if (it != map.end()) {
               val = (*it).second;

               success = true;
          }
          else
               success = false;
     }
};


template <typename _T>
class TypeID : public ToString<std::type_info>
{
public:
     TypeID()
          :
          ToString( typeid(_T) )
     {
     }
};


class Demangle : public Direct::String
{
public:
     Demangle( const char *symbol )
     {
#ifdef __x86_64__
          int   status;
          char *realname;

          realname = abi::__cxa_demangle(symbol, 0, 0, &status);

          if (status)
                    PrintF( "DEMANGLE-ERROR-(%d) <- [%s]", status, symbol );
          else {
                    PrintF( "%s", realname );
                    free(realname);
          }
#else
          PrintF( "%s", symbol );
#endif
     }
};


}


#endif // __cplusplus

#endif


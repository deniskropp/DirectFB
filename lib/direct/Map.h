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



#ifndef ___Direct__Map__H___
#define ___Direct__Map__H___

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}


#include <map>

#include <direct/LockWQ.h>
#include <direct/ToString.h>
#include <direct/Utils.h>


namespace Direct {


template <typename _Key, typename _Item>
class Map
{
public:
     typedef _Key  KeyType;
     typedef _Item ItemType;

     virtual ~Map()
     {
     }

     virtual void         Insert( const _Key  &key,
                                  const _Item &item ) = 0;
     virtual void         Remove( const _Key  &key ) = 0;

     virtual size_t       Length() const = 0;

     virtual void         Clear () = 0;

     virtual const _Item &Lookup( const _Key &key ) = 0;
};


template <typename _Key, typename _Item = _Key>
class MapSimple : public Map<_Key,_Item>
{
public:
     MapSimple()
          :
          map( map_static )
     {
     }

     MapSimple( std::map<_Key,_Item> &map )
          :
          map( map )
     {
     }

     virtual ~MapSimple()
     {
     }

     virtual void Insert( const _Key  &key,
                          const _Item &item )
     {
          map[ key ] = item;
     }

     virtual void Remove( const _Key &key )
     {
          map.erase( key );
     }

     virtual size_t Length() const
     {
          return map.size();
     }

     virtual void Clear()
     {
          map.clear();
     }

     virtual const _Item &Lookup( const _Key &key )
     {
          const_iterator it = map.find( key );

          if (it != map.end())
               return (*it).second;

          return Null();
     }

     static const _Item &Null()
     {
          static _Item null;
          return null;
     }

     typedef typename std::map<_Key,_Item>::const_iterator const_iterator;

     inline const_iterator begin() const { return map.begin(); }
     inline const_iterator end()   const { return map.end(); }

//     template<>
//     inline ToString<MapSimple>::ToString( const MapSimple &m )
//     {
//          for (const_iterator it=begin(); it!=end(); it++)
//               PrintF( "%s:%s", *ToString<_Key>( (*it).first ), *ToString<_Item>( (*it).second ) );
//     }

protected:
     std::map<_Key,_Item> &map;
     std::map<_Key,_Item>  map_static;
};


template <typename _Key, typename _Item>
class MapWrapped : public MapSimple<_Key,_Item>
{
public:
     MapWrapped( std::map<_Key,_Item> &wrapped_map )
          :
          MapSimple<_Key,_Item>( wrapped_map )
     {
     }

     virtual ~MapWrapped()
     {
     }
};


#if 0
template <typename _Key, typename _Item = _Key>
     inline ToString<typename Direct::MapSimple<_Key,_Item> >::ToString( const Direct::MapSimple<_Key,_Item> &m )
     {
          for (Direct::MapSimple<_Key,_Item>::const_iterator it=begin(); it!=end(); it++)
               PrintF( "%s:%s", *ToString<_Key>( (*it).first ), *ToString<_Item>( (*it).second ) );
     }
#endif


}


#endif // __cplusplus

#endif


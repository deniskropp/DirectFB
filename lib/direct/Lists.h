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



#ifndef ___Direct__Lists__H___
#define ___Direct__Lists__H___

#include <direct/Types++.h>

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}


#include <list>
#include <map>

#include <direct/LockWQ.h>


namespace Direct {


template <typename _Item>
class List
{
public:
     virtual ~List()
     {
     }

     virtual void   Append ( const _Item &item ) = 0;
     virtual void   Prepend( const _Item &item ) = 0;
     virtual void   Remove ( const _Item &item ) = 0;
     virtual size_t Length () const = 0;
     virtual void   Clear  () = 0;
};


template <typename _Item>
class ListSimpleSlow : public List<_Item>
{
public:
     ListSimpleSlow()
          :
          length( 0 )
     {
     }

     virtual ~ListSimpleSlow()
     {
          D_ASSUME( list.empty() );
     }

     virtual void Append( const _Item &item )
     {
          list.push_back( item );

          length++;
     }

     virtual void Prepend( const _Item &item )
     {
          list.push_front( item );

          length++;
     }

     virtual void Remove( const _Item &item )
     {
          D_ASSERT( length > 0 );

          list.remove( item );

          length--;
     }

     virtual size_t Length() const
     {
          return length;
     }

     virtual void Clear()
     {
          list.clear();
     }


     typedef typename std::list<_Item>::const_iterator const_iterator;

     inline const_iterator begin() const { return list.begin(); }
     inline const_iterator end()   const { return list.end(); }


private:
     std::list<_Item>    list;
     size_t              length;
};


template <typename _Item>
class ListSimple : public List<_Item>
{
public:
     ListSimple()
     {
     }

     virtual ~ListSimple()
     {
          D_ASSUME( map.empty() );
     }

     virtual void Append( const _Item &item )
     {
          map[ item ] = item;
     }

     virtual void Prepend( const _Item &item )
     {
          map[ item ] = item;
     }

     virtual void Remove( const _Item &item )
     {
          map.erase( item );
     }

     virtual size_t Length() const
     {
          return map.size();
     }

     virtual void Clear()
     {
          map.clear();
     }


     typedef typename std::map<_Item,_Item>::const_iterator const_iterator;

     inline const_iterator begin() const { return map.begin(); }
     inline const_iterator end()   const { return map.end(); }


private:
     std::map<_Item,_Item> map;
};


template <typename _Item>
class ListLockedSlow : public List<_Item>
{
public:
     ListLockedSlow()
          :
          length( 0 )
     {
     }

     virtual ~ListLockedSlow()
     {
          D_ASSUME( list.empty() );
     }

     virtual void Append( const _Item &item )
     {
          LockWQ::Lock l1( lwq );

          list.push_back( item );

          length++;
     }

     virtual void Prepend( const _Item &item )
     {
          LockWQ::Lock l1( lwq );

          list.push_front( item );

          length++;
     }

     virtual void Remove( const _Item &item )
     {
          D_ASSERT( length > 0 );

          LockWQ::Lock l1( lwq );

          list.remove( item );

          length--;

          if (length == 0)
               lwq.notifyAll();
     }

     virtual size_t Length() const
     {
          return length;
     }

     virtual void Clear()
     {
          LockWQ::Lock l1( lwq );

          if (length > 0) {
               list.clear();

               length = 0;

               lwq.notifyAll();
          }
     }


     void WaitEmpty()
     {
          LockWQ::Lock l1( lwq );

          while (!list.empty())
               l1.wait();
     }


private:
     LockWQ              lwq;
     std::list<_Item>    list;
     size_t              length;
};


template <typename _Item>
class ListLocked : public List<_Item>
{
public:
     ListLocked()
     {
     }

     virtual ~ListLocked()
     {
          D_ASSUME( map.empty() );
     }

     virtual void Append( const _Item &item )
     {
          LockWQ::Lock l1( lwq );

          map[ item ] = item;
     }

     virtual void Prepend( const _Item &item )
     {
          LockWQ::Lock l1( lwq );

          map[ item ] = item;
     }

     virtual void Remove( const _Item &item )
     {
          LockWQ::Lock l1( lwq );

          map.erase( item );

          if (map.empty())
               lwq.notifyAll();
     }

     virtual size_t Length() const
     {
          return map.size();
     }

     virtual void Clear()
     {
          LockWQ::Lock l1( lwq );

          if (!map.empty()) {
               map.clear();

               lwq.notifyAll();
          }
     }


     void WaitEmpty()
     {
          LockWQ::Lock l1( lwq );

          while (!map.empty())
               l1.wait();
     }


private:
     LockWQ                lwq;
     std::map<_Item,_Item> map;
};


}


#endif // __cplusplus

#endif


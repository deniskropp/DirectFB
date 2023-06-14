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

#ifndef ___Direct__Base__H___
#define ___Direct__Base__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/os/mutex.h>
#include <direct/os/waitqueue.h>

#include <direct/debug.h>


     void __D_base_init( void );
     void __D_base_deinit( void );

#ifdef __cplusplus
}

#include <direct/Types++.h>

#include <direct/Map.hxx>
#include <direct/String.hxx>
#include <direct/ToString.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <functional>
#include <stdexcept>

//#include <typeindex>
#include <typeinfo>


D_DEBUG_DOMAIN( Direct_Type, "Direct/Type", "Direct Type" );


namespace Direct {

class Mapping
{
public:
//     class Key {
//     public:
//          Direct::String  name;
//          Direct::String  impl;
//          void           *ctx;
//
//          Key( const Direct::String &name = "",
//               const Direct::String &impl = "",
//               void                 *ctx  = NULL )
//               :
//               name( name ),
//               impl( impl ),
//               ctx( ctx )
//          {
//          }
//
//          bool operator < (const Key &other) const {
//               if (other.name == name) {
//                    if (other.impl == impl) {
//                         if (other.ctx == ctx)
//                              return false;
//
//                         return ctx < other.ctx;
//                    }
//
//                    return impl < other.impl;
//               }
//
//               return name < other.name;
//          }
//     };

     typedef std::tuple<std::string,std::string,void*> Key;

     typedef Key MapKey;

     template<typename _Function>
     static _Function &Call( const Direct::String &name,
                             const Direct::String &impl,
                             void                 *ctx  = NULL )
     {
          std::map<MapKey,_Function> &map = Map<_Function>();

          Key        key( name, impl, ctx );
//          _Function &entry = map[ *String::F( "[%s][%s][%p]", *std::get<0>(key), *std::get<1>(key), std::get<2>(key) ) ];
          _Function &entry = map[ key ];

          //D_INFO( "Direct/Type/Call:  %s  [%s]  -> %p (%s) map: %p\n", *TypeID<_Function>(),
          //        *String::F( "[%s][%s][%p]", std::get<0>(key).c_str(), std::get<1>(key).c_str(), std::get<2>(key) ), &entry,
          //        *ToString<std::type_info>( entry.target_type() ), &map );

          return entry;
     }

     template <typename _Function>
     static void Register( const Direct::String &name,
                           const _Function      &f,
                           const Direct::String &impl,
                           void                 *ctx   = NULL )
     {
          std::map<MapKey,_Function> &map = Map<_Function>();

          Key        key( name, impl, ctx );
//          _Function &entry = map[ *String::F( "[%s][%s][%p]", *std::get<0>(key), *std::get<1>(key), std::get<2>(key) ) ];
          _Function &entry = map[ key ];

          //D_INFO( "Direct/Type/Register:  %s  [%s]  <- %p (%s) map: %p\n", *TypeID<_Function>(), index.c_str(), &entry,
          //        *ToString<std::type_info>( entry.target_type() ), &map );

          entry = f;

          //D_INFO( "Direct/Type/Register:  %s  [%s]  -> %p (%s) map: %p\n", *TypeID<_Function>(),
          //        *String::F( "[%s][%s][%p]", std::get<0>(key).c_str(), std::get<1>(key).c_str(), std::get<2>(key) ), &entry,
          //        *ToString<std::type_info>( entry.target_type() ), &map );
     }

     template <typename _Function>
     static std::map<MapKey,_Function> &Map()
     {
          static std::map<MapKey,_Function> map;

          return map;
     }

     template <typename _Function>
     static std::map<MapKey,_Function> Map( const Direct::String &name,
                                            void                 *ctx = NULL )
     {
          std::map<MapKey,_Function> &map = Map<_Function>();
          std::map<MapKey,_Function>  ret;

          //D_INFO( "MAPPING %s   %zu (map %p)\n", *name, map.size(), &map );

          for (auto e = map.begin(); e != map.end(); e++) {
               if (Direct::String( std::get<0>((*e).first).c_str() ) == name && (!ctx || std::get<2>((*e).first) == ctx)) {
                    //D_INFO( "MAPPING   %-30s  %s\n", std::get<0>((*e).first).c_str(), std::get<1>((*e).first).c_str() );
//                    D_INFO( "MAPPING   %s\n", (*e).first.c_str() );
                    ret.insert( *e );
               }
          }

          return ret;
     }


     template <typename _Function>
     static void Dispatch( const Direct::String &name,
                           void                 *ctx = NULL )
     {
          auto map = Map<_Function>( name, ctx );

          //D_INFO( "Direct/Type/Dispatch: all %s ctx %p...\n", *name, ctx );

          for (auto f = map.begin(); f != map.end(); f++) {
               //D_INFO( "  %s\n", (*f).first.c_str() );
               //D_INFO( "  %s %s (%p)\n",
               //        std::get<0>((*f).first).c_str(),
               //        std::get<1>((*f).first).c_str(),
               //        std::get<2>((*f).first) );

               (*f).second();
          }
     }
};



class Base {
public:
     class InfoBase;
     class TypeBase;

     typedef std::shared_ptr<InfoBase> InfoHandle;
     typedef std::shared_ptr<TypeBase> TypeHandle;


     class InfoBase {
     public:
          Direct::String        base_name;

          Direct::String        ns;

          Direct::String        name;
          Direct::String        real_name;

          const std::type_info &type_info;
          const std::type_info &real_info;

          Direct::String        parent_name;
          Direct::String        parent_real_name;

          const std::type_info &parent_type_info;
          const std::type_info &parent_real_info;


          InfoBase( const Direct::String &base_name,

                    const Direct::String &ns,

                    const Direct::String &name,
                    const Direct::String &real_name,

                    const std::type_info &type_info,
                    const std::type_info &real_info,

                    const Direct::String &parent_name,
                    const Direct::String &parent_real_name,

                    const std::type_info &parent_type_info,
                    const std::type_info &parent_real_info )
               :
               base_name(          base_name ),

               ns(                 ns ),

               name(               name ),
               real_name(          real_name ),

               type_info(          type_info ),
               real_info(          real_info ),

               parent_name(        parent_name ),
               parent_real_name(   parent_real_name ),

               parent_type_info(   parent_type_info ),
               parent_real_info(   parent_real_info )
          {
               D_DEBUG_AT( Direct_Type, "InfoBase::%s()\n", __FUNCTION__ );
          }

          virtual ~InfoBase() {
               D_DEBUG_AT( Direct_Type, "InfoBase::%s()\n", __FUNCTION__ );
          }
     };

     class TypeBase : public Mapping {
     public:
          class Info;

          typedef struct {} RealType;
          typedef TypeBase  MyType;
          typedef Info      MyInfo;

          TypeBase() {
               D_DEBUG_AT( Direct_Type, "TypeBase::%s()\n", __FUNCTION__ );
          }

          virtual ~TypeBase() {
               D_DEBUG_AT( Direct_Type, "TypeBase::%s()\n", __FUNCTION__ );
          }

          virtual InfoBase &GetInfo() = 0;
          virtual void      TypeInit() = 0;

          virtual const Direct::String &GetName() = 0;

          virtual void Update() {};

     protected:
          std::vector<TypeHandle> all_handles;
          std::map<std::string/*type_index*/,TypeHandle> handles;

     public:
          template <typename _Target>
          _Target &Convert()
          {
               InfoBase &source_info = GetInfo();
               InfoBase &target_info = _Target::GetTypeInstance().GetInfo();

               TypeHandle &handle = handles[ ToString<std::type_info>(target_info.real_info) ];

//               D_INFO( "Direct/Type/Map:    => CONVERT %p <=\n", & (_Target&) *handle );

               if (!handle) {
                    _Target *t = _Target::template Call< std::function<_Target *(TypeBase *)> >( *source_info.name, "" )( this );

                    handle.reset( t );

                    all_handles.push_back( handle );
               }
               else {
//                    D_INFO( "Direct/Type/Map:    => CACHED %p <=\n", & (_Target&) *handle );

                    // FIXME: Update?
               }

               return (_Target&) *handle;
          }

     public:
          template <typename _Target>
          operator _Target &()
          {
               return Convert<_Target>();
          }
     };


     static Base &GetInstance() {
          static Base base;

          return base;
     }


     typedef std::string                     InfoKey;
     typedef std::string                     TypeKey;

     typedef std::map<InfoKey,InfoBase*>     TypeMap;
     typedef std::map<TypeKey,TypeMap>       TypeMaps;

     TypeMaps maps;



     typedef std::list<InfoBase*>            InfoList;
     typedef std::list<TypeBase*>            TypeList;

     InfoList __list;

     void __Add( InfoBase *base );
     void __Remove( InfoBase *base );

     void __HandleLists();

     void __Init();
     void __Deinit();
};


}


#endif // __cplusplus

#endif


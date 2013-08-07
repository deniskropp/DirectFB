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

#include <direct/Map.h>
#include <direct/String.h>
#include <direct/ToString.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <stdexcept>

#include <typeindex>
#include <typeinfo>


D_DEBUG_DOMAIN( Direct_Type, "Direct/Type", "Direct Type" );



namespace Direct {


class Mapping
{
public:
     template <typename _Function>
     using FunctionMap = std::map<std::string,_Function>;

     template<typename _Function>
     static _Function &Call( const std::string &index = "" )
     {
          FunctionMap<_Function> &map = Map<_Function>();

          _Function &entry = map[ index ];

          D_INFO( "Direct/Type/Call:  %s  [%s]  -> %p (%s) map: %p\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ), &map );

          return entry;
     }

     template <typename _Function, typename _Object>
     static void Register( const std::string &index, _Object &f )
     {
          FunctionMap<_Function> &map = Map<_Function>();

          _Function &entry = map[ index ];

          D_INFO( "Direct/Type/Register:  %s  [%s]  <- %p (%s) map: %p\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ), &map );

          entry = f;

          D_INFO( "Direct/Type/Register:  %s  [%s]  -> %p (%s)\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ) );
     }

     template <typename _Function>
     static void Register( const std::string &index, _Function &f )
     {
          FunctionMap<_Function> &map = Map<_Function>();

          _Function &entry = map[ index ];

          D_INFO( "Direct/Type/Register:  %s  [%s]  <- %p (%s) map: %p\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ), &map );

          entry = f;

          D_INFO( "Direct/Type/Register:  %s  [%s]  -> %p (%s)\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ) );
     }

     template <typename _Function>
     static void Register( const std::string &index, _Function f )
     {
          FunctionMap<_Function> &map = Map<_Function>();

          _Function &entry = map[ index ];

          D_INFO( "Direct/Type/Register:  %s  [%s]  <- %p (%s) map: %p\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ), &map );

          entry = f;

          D_INFO( "Direct/Type/Register:  %s  [%s]  -> %p (%s)\n", *TypeID<_Function>(), index.c_str(), &entry,
                  *ToString<std::type_info>( entry.target_type() ) );
     }

     template <typename _Function>
     static _Function &GetEntry( const std::string &index = "" )
     {
          FunctionMap<_Function> &map = Map<_Function>();

          _Function &entry = map[ index ];

          return entry;
     }

     template <typename _Function>
     static FunctionMap<_Function> &Map()
     {
          static FunctionMap<_Function> map;

          return map;
     }
};


class Base {
public:
     class InfoBase;
     class TypeBase;

     template <typename _Item>
     using Handle = std::shared_ptr<_Item>;

     typedef Handle<InfoBase> InfoHandle;
     typedef Handle<TypeBase> TypeHandle;


     class InfoBase {
     public:
          Direct::String        base_name;

          Direct::String        ns;

          TypeBase             &type;
          Direct::String        name;
          Direct::String        real_name;

          const std::type_info &type_info;
          const std::type_info &real_info;

          //TypeBase             &parent_type;
          Direct::String        parent_name;
          Direct::String        parent_real_name;

          const std::type_info &parent_type_info;
          const std::type_info &parent_real_info;


          InfoBase( const Direct::String &base_name,

                    const Direct::String &ns,

                    TypeBase             &type,
                    const Direct::String &name,
                    const Direct::String &real_name,

                    const std::type_info &type_info,
                    const std::type_info &real_info,

                    //TypeBase             &parent_info,
                    const Direct::String &parent_name,
                    const Direct::String &parent_real_name,

                    const std::type_info &parent_type_info,
                    const std::type_info &parent_real_info )
               :
               base_name(          base_name ),

               ns(                 ns ),

               type(               type ),
               name(               name ),
               real_name(          real_name ),

               type_info(          type_info ),
               real_info(          real_info ),

               //parent_type(        parent_type ),
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

     protected:
          std::vector<TypeHandle> all_handles;
          std::map<std::type_index,TypeHandle> handles;

          template <typename _Target>
          _Target &Convert()
          {
               //D_INFO( "Direct/Type/Map: Converting %p [ %s ] [%s] -> [%s]\n",
               //        this, *TypeID<Type>(), *TypeID<_T>(), *TypeID<_Target>() );

               InfoBase &source_info = GetInfo();
               InfoBase &target_info = _Target::GetTypeInstance().GetInfo();

               D_INFO( "Direct/Type/Map:    => [  %s  (%p) --->  %s  ] <=\n",
                       *source_info.real_name, this, *target_info.real_name );


//               std::map<std::type_index,TypeHandle> &handles = GetHandleMap<_Target>();

               TypeHandle &handle = handles[ target_info.real_info ];

               if (!handle) {
                    _Target *t = _Target::template Call< std::function<_Target *(TypeBase *)> >( *source_info.name )( this );

                    D_INFO( "Direct/Type/Map:    => NEW %p <=\n", t );

                    handle.reset( t );

                    all_handles.push_back( handle );
               }
               else
                    D_INFO( "Direct/Type/Map:    => CACHED %p <=\n", & (_Target&) *handle );

               return (_Target&) *handle;
          }

//          template <typename _Target>
//          std::map<std::type_index,TypeHandle> &GetHandleMap()
//          {
//               static std::map<std::type_index,TypeHandle> handles;
//
//               return handles;
//          }

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


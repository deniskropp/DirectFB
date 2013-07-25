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

#ifndef ___Direct__Type__H___
#define ___Direct__Type__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/os/mutex.h>
#include <direct/os/waitqueue.h>

#include <direct/debug.h>


#ifdef __cplusplus
}

#include <direct/Base.h>
#include <direct/Map.h>
#include <direct/String.h>
#include <direct/ToString.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include <functional>
#include <typeinfo>


//D_DEBUG_DOMAIN( Direct_Type, "Direct/Type", "Direct Type" );



/*

  Attrib (EGLInt)
  Attrib (String)

    -> lookup in cached table
       -> lookup in dynamic table (with update call)
          -> resolve


 Type

   - std::functions
     - static or per instance
     - extensions (overrides and adding functions) can be made after instantiation
     -


*/



namespace Direct {


template <typename _NS, typename _Base = Base, typename _Root = Base>
class Types : public Mapping
{
public:
     typedef typename _Base::InfoBase      InfoBase;
     typedef typename _Base::TypeBase      TypeBase;
     typedef typename _Base::InfoHandle    InfoHandle;
     typedef typename _Base::TypeHandle    TypeHandle;

     Types() {
          D_DEBUG_AT( Direct_Type, "Types::%s()\n", __FUNCTION__ );
     }

     template <typename _T, typename _P=TypeBase>
     class Type : public _P::MyType
     {
     public:
          class Info;

          typedef typename _P::MyType   ParentType;
          typedef typename _P::MyInfo   ParentInfo;
          typedef _T                    RealType;
          typedef typename _P::RealType RealTypeParent;
          typedef Type                  MyType;
          typedef Info                  MyInfo;

          typedef Types::InfoHandle     InfoHandle;

          RealTypeParent &parent;

          Type( RealTypeParent &parent = * (RealTypeParent*) 0L )
               :
               parent( parent )
          {
               D_DEBUG_AT( Direct_Type, "Type::%s( %p ) <--= '%s' :: '%s' :: '%s'\n",
                           __FUNCTION__, this, *TypeID<_NS>(), *TypeID<_P>(), *TypeID<_T>() );
          }

          virtual ~Type() {
               D_DEBUG_AT( Direct_Type, "Type::%s( %p )\n", __FUNCTION__, this );
          }


          class Info : public InfoBase
          {
          public:
               Info( Type &type )
                    :
                    InfoBase( TypeID<TypeBase>(),

                              TypeID<_NS>(),

                              type,
                              TypeID<Type>(),
                              TypeID<RealType>(),

                              typeid(Type),
                              typeid(RealType),

                              //,
                              TypeID<ParentType>(),
                              TypeID<RealTypeParent>(),

                              typeid(ParentType),
                              typeid(RealTypeParent) )
               {
               }

               static InfoHandle New( Type &type ) {
                    return std::make_shared<Info>( type );
               }
          };


     public:
          template <typename _Target>
          operator _Target &()
          {
               return Convert<_Target>();
          }


          virtual InfoBase &GetInfo() {
               static InfoHandle info = Info::New( *this );

               return *info;
          }

          virtual const Direct::String &GetName() {
               if (!all_handles.empty()) {
                    return all_handles[0]->GetName();
               }

               return GetTypeInstance().GetInfo().real_name;
          }


          virtual void TypeInit() {
          }



          static Type &GetTypeInstance() {
               static Type type;

               return type;
          }



          static void __init() {
               _Root::GetInstance().__Add( &GetTypeInstance().GetInfo() );
          }

          static void __deinit() {
               _Root::GetInstance().__Remove( &GetTypeInstance().GetInfo() );
          }


     private:
          std::vector<Types::TypeHandle> all_handles;

          template <typename _Target>
          _Target &Convert()
          {
               D_INFO( "Direct/Type/Map: Converting %p [ %s ] [%s] -> [%s]\n",
                       this, *TypeID<Type>(), *TypeID<_T>(), *TypeID<_Target>() );

               InfoBase &source_info = GetInfo();
               InfoBase &target_info = _Target::GetTypeInstance().GetInfo();

               D_INFO( "Direct/Type/Map:    => [  %s  --->  %s  ] <=\n",
                       *source_info.real_name, *target_info.real_name );


               std::map<std::type_index,Types::TypeHandle> &handles = GetHandleMap<_Target>();

               Types::TypeHandle &handle = handles[ target_info.real_info ];

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

          template <typename _Target>
          std::map<std::type_index,Types::TypeHandle> &GetHandleMap()
          {
               static std::map<std::type_index,Types::TypeHandle> handles;

               return handles;
          }

     public:
          template <typename _Source, typename... _ArgTypes>
          static void RegisterConversion( _ArgTypes&&... __args )
          {
               RealType::template Register<
                    std::function< RealType * ( Types::TypeBase *source ) >
                    >( _Source::GetTypeInstance().GetInfo().name,
                       std::bind( []( Types::TypeBase *source,
                                      _ArgTypes&&...   __args )
                                   {
                                        return new RealType( (_Source&) *source, std::forward<_ArgTypes>(__args)... );
                                   },
               std::placeholders::_1, std::forward<_ArgTypes>(__args)... ) );
          }
     };
};


#define D_TYPE_INIT( _Type )                                                    \
     D_TYPE_INIT_( _Type, _Type )

#define D_TYPE_INIT_( _Type, _Name )                                            \
     __constructor__                                                            \
     void __##_Name##__Register()                                               \
     {                                                                          \
          _Type::__init();                                                      \
     }                                                                          \
     __destructor__                                                             \
     void __##_Name##__Unregister()                                             \
     {                                                                          \
          _Type::__deinit();                                                    \
     }                                                                          \


}


#endif // __cplusplus

#endif


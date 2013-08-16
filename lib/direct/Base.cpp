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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>


extern "C" {
#include <direct/debug.h>
}

#include <direct/Base.h>

/*********************************************************************************************************************/

extern "C" {

void
__D_base_init()
{
     Direct::Base &base = Direct::Base::GetInstance();

     base.__Init();
}

void
__D_base_deinit()
{
     Direct::Base &base = Direct::Base::GetInstance();

     base.__Deinit();
}

}



namespace Direct {


void
Base::__Add( InfoBase *base )
{
     D_INFO( "Direct/Type/Base: Adding type %p (%s)\n", base, *base->real_name );

     __list.push_back( base );

     direct_trace_print_stack(NULL);
}

void
Base::__Remove( InfoBase *base )
{
     __list.remove( base );
}

void
Base::__HandleLists()
{
     size_t num_maps  = 0;
     size_t num_types = 0;
     size_t num_mapped_types = 0;

     D_INFO( "Direct/Type/Base: Registering types..........................\n" );

//     for (auto info : __list) {
     for (InfoList::iterator it = __list.begin(); it != __list.end(); it++) {
          InfoBase* info = *it;

          D_INFO( "Direct/Type/Base: [%2zu] %s\n", num_types, *ToString<InfoBase>( *info ) );

          num_types++;

          TypeMap &map  = maps[info->ns];
          auto     item = map.find( info->real_name );

          if (item != map.end()) {
          }
          else {
               info->type.TypeInit();

               map.insert( std::pair<TypeKey,InfoBase*>( info->real_name, info ) );
          }
     }

     __list.clear();

//     for (auto map : maps) {
     for (TypeMaps::iterator it = maps.begin(); it != maps.end(); it++) {
          num_maps++;
          num_mapped_types += (*it).first.size();
     }

     D_INFO( "Direct/Type/Base: Got %zu new/updated types (total %zu maps with %zu types)\n", num_types, num_maps, num_mapped_types );

//     for (auto map : maps) {
     for (TypeMaps::iterator it = maps.begin(); it != maps.end(); it++) {
          D_INFO( "Direct/Type/Base: .============ [%s] ============.\n", (*it).first.c_str() );

          num_types = 0;

//          for (auto info : (*it).second) {
          for (TypeMap::iterator it2 = (*it).second.begin(); it2 != (*it).second.end(); it2++) {
               D_INFO( "Direct/Type/Base: [%2zu] %s\n", num_types, *ToString<InfoBase>( *(*it2).second ) );

               num_types++;
          }
     }
}


void
Base::__Init()
{
     //__HandleLists();
}

void
Base::__Deinit()
{
}


}


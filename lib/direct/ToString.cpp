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

#include <direct/Map.h>
#include <direct/ToString.h>
#include <direct/Type.h>

/*********************************************************************************************************************/

template <>
ToString<const std::string>::ToString( const std::string &string )
{
     Set( string );
}


template <>
ToString<Direct::Base::InfoBase>::ToString( const Direct::Base::InfoBase &info )
{
     PrintF( "%-10s %-30s %-30s", *info.base_name, *info.parent_real_name, *info.real_name );
}

template <>
ToString<Direct::Base::TypeBase>::ToString( const Direct::Base::TypeBase &type )
{
     const Direct::Base::InfoBase &info = ((Direct::Base::TypeBase&) type).GetInfo();

     PrintF( "%s", *ToString<Direct::Base::InfoBase>( info ) );
}

template <>
ToString<Direct::Base::InfoHandle>::ToString( const Direct::Base::InfoHandle &info )
{
     PrintF( "%s", *ToString<Direct::Base::InfoBase>( *info ) );
}

template <>
ToString<Direct::Base::TypeHandle>::ToString( const Direct::Base::TypeHandle &type )
{
     PrintF( "%s", *ToString<Direct::Base::TypeBase>( *type ) );
}





template <>
ToString<std::type_info>::ToString( const std::type_info &ti )
{
     Direct::Demangle demangle( ti.name() );

     Set( demangle );
}


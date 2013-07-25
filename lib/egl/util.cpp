/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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

#include <config.h>

#include <stdio.h>
#include <string.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>
#include <direct/Utils.h>

#include <egl/dfbegl.h>
#include <egl/dfbegl_int_names.h>


D_LOG_DOMAIN( DFBEGL_Util, "DFBEGL/Util", "DirectFB EGL Utility" );



static const DirectFBEGLIntNames( egl_ints );

template<>
ToString<DirectFB::EGL::EGLInt>::ToString( const DirectFB::EGL::EGLInt &egl_int )
{
     Direct::String str;

     DirectFB::EGL::EGLInt::Maps &maps = DirectFB::EGL::EGLInt::GetMaps();

     if (Direct::MapLookup<DirectFB::EGL::EGLInt::ToStringMap>( maps.toString, egl_int.value, str )) {
          Set( str );
          return;
     }

     for (int i=0; egl_ints[i].value; i++) {
          if (egl_int.value == egl_ints[i].value) {
               PrintF( "%s", egl_ints[i].name );
               DirectFB::EGL::EGLInt::Register( egl_int.value, egl_ints[i].name );
               return;
          }
     }

     PrintF( "_UNKNOWN_<0x%08x>", egl_int.value );
}

template<>
FromString<DirectFB::EGL::EGLInt>::FromString( DirectFB::EGL::EGLInt &egl_int, const Direct::String &string )
{
//     DirectFB::EGL::EGLInt::Maps &maps = DirectFB::EGL::EGLInt::GetMaps();

//     if (Direct::MapLookup<DirectFB::EGL::EGLInt::FromStringMap>( maps.fromString, string, egl_int.value )) {
//          success = true;
//          return;
//     }

     for (int i=0; egl_ints[i].value; i++) {
          if (!strcmp( *string, egl_ints[i].name )) {
               egl_int.value = egl_ints[i].value;
//               DirectFB::EGL::EGLInt::Register( egl_int.value, egl_ints[i].name );
               success = true;
               return;
          }
     }
}


namespace DirectFB {

namespace EGL {


EGLInt::operator Direct::String ()
{
     return ToString<EGLInt>( *this );
}

EGLInt::operator std::string ()
{
     return ToString<EGLInt>( *this );
}

EGLInt::Maps &
EGLInt::GetMaps()
{
     static Maps maps;

     return maps;
}

void
EGLInt::Register( const EGLint         &egl_int,
                  const Direct::String &egl_str )
{
     D_DEBUG_AT( DFBEGL_Util, "EGLInt::%s( 0x%04x, '%s' )\n", __FUNCTION__, egl_int, *egl_str );

     Maps &maps = GetMaps();

     maps.toString[egl_int]   = egl_str;
     maps.fromString[egl_str] = egl_int;
}





Direct::String
Util::APIToString( EGLenum api, EGLint version )
{
     D_DEBUG_AT( DFBEGL_Util, "EGL::Util::%s( api 0x%04x )\n", __FUNCTION__, api );

     Direct::String egl_api;

     switch (api) {
          case EGL_OPENGL_ES_API:
               egl_api = "OpenGL ES";

               switch (version) {
                    case 1:
                         egl_api = "OpenGL ES";
                         break;

                    case 2:
                         egl_api = "OpenGL ES2";
                         break;

                    case 3:
                         egl_api = "OpenGL ES3";
                         break;

                    default:
                         D_ERROR( "DFBEGL/Util: Invalid OpenGL ES version %d!\n", version );
                         break;
               }
               break;

          case EGL_OPENVG_API:
               egl_api = "OpenVG";
               break;

          case EGL_OPENGL_API:
               egl_api = "OpenGL";
               break;

          default:
               D_ERROR( "DFBEGL/Util: Invalid api 0x%04x!\n", api );
               break;
     }

     return egl_api;
}

EGLenum
Util::StringToAPI( const Direct::String &api )
{
     D_DEBUG_AT( DFBEGL_Util, "EGL::Util::%s( '%s' )\n",
                 __FUNCTION__, *api );

     if (api == "OpenGL ES")
          return EGL_OPENGL_ES_API;

     if (api == "OpenGL ES2")
          return EGL_OPENGL_ES_API;

     if (api == "OpenGL ES3")
          return EGL_OPENGL_ES_API;

     if (api == "OpenVG")
          return EGL_OPENVG_API;

     if (api == "OpenGL")
          return EGL_OPENGL_API;

     D_ERROR( "DFBEGL/Util: Invalid api '%s'!\n", *api );

     return 0;
}

DFBResult
Util::GetOptions( Graphics::Options &options,
                  const EGLint      *attrib_list )
{
     D_DEBUG_AT( DFBEGL_Util, "EGL::Util::%s( attrib_list %p )\n", __FUNCTION__, attrib_list );

     for (const EGLint *v=attrib_list; *v != EGL_NONE; v+=2) {
          Option *option = new Option( v[0], v[1] );

          D_DEBUG_AT( DFBEGL_Util, "  -> %-20s : %s\n", *option->GetName(), *option->GetString() );

          options.Add( option );
     }

     return DFB_OK;
}

DFBResult
Util::GetSurfaceAttribs( Graphics::Options   &options,
                         std::vector<EGLint> &attribs )
{
     D_DEBUG_AT( DFBEGL_Util, "EGL::Util::%s()\n", __FUNCTION__ );

     for (Graphics::Options::const_iterator it=options.begin(); it!=options.end(); it++) {
          Graphics::OptionBase *option = (*it).second;

          D_DEBUG_AT( DFBEGL_Util, "  -> %-20s : %s\n", *option->GetName(), *option->GetString() );

          EGLInt v0, v1;

          if (FromString<EGLInt>( v0, option->GetName() ) && FromString<EGLInt>( v1, option->GetString() )) {
               D_DEBUG_AT( DFBEGL_Util, "  => 0x%04x : 0x%04x\n", v0.value, v1.value );

               attribs.push_back( v0.value );
               attribs.push_back( v1.value );
          }
     }

     return DFB_OK;
}


DFBResult
Util::GetSurfaceDescription( Graphics::Options     &options,
                             DFBSurfaceDescription &desc )
{
     long val;

     D_DEBUG_AT( DFBEGL_Util, "EGL::Util::%s()\n", __FUNCTION__ );

     desc.flags = DSDESC_CAPS;
     desc.caps  = DSCAPS_FLIPPING;

     if (options.Get<long>( "WIDTH", val )) {
          D_DEBUG_AT( DFBEGL_Util, "  -> WIDTH     %ld\n", val );

          D_FLAGS_SET( desc.flags, DSDESC_WIDTH );

          desc.width = val;
     }

     if (options.Get<long>( "HEIGHT", val )) {
          D_DEBUG_AT( DFBEGL_Util, "  -> HEIGHT    %ld\n", val );

          D_FLAGS_SET( desc.flags, DSDESC_HEIGHT );

          desc.height = val;
     }

     if (options.Get<long>( "VG_ALPHA_FORMAT", val )) {
          D_DEBUG_AT( DFBEGL_Util, "  -> VG_ALPHA_FORMAT %ld\n", val );

          if (val != EGL_ALPHA_FORMAT_NONPRE)
               D_FLAGS_SET( desc.caps, DSCAPS_PREMULTIPLIED );
     }

     return DFB_OK;
}


}

}


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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <string.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>



D_LOG_DOMAIN( DFBEGL_Config, "DFBEGL/Config", "DirectFB EGL Config" );


namespace DirectFB {

namespace EGL {

/*********************************************************************************************************************/

enum {
   /* types */
   ATTRIB_TYPE_INTEGER,
   ATTRIB_TYPE_BOOLEAN,
   ATTRIB_TYPE_BITMASK,
   ATTRIB_TYPE_ENUM,
   ATTRIB_TYPE_PSEUDO, /* non-queryable */
   ATTRIB_TYPE_PLATFORM, /* platform-dependent */
   /* criteria */
   ATTRIB_CRITERION_EXACT,
   ATTRIB_CRITERION_ATLEAST,
   ATTRIB_CRITERION_MASK,
   ATTRIB_CRITERION_SPECIAL,
   ATTRIB_CRITERION_IGNORE
};

/* EGL spec Table 3.1 and 3.4 */
static const struct {
   EGLint attr;
   EGLint type;
   EGLint criterion;
   EGLint default_value;
} _eglValidationTable[] =
{
   /* core */
   { EGL_BUFFER_SIZE,               ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_RED_SIZE,                  ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_GREEN_SIZE,                ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_BLUE_SIZE,                 ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_LUMINANCE_SIZE,            ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_ALPHA_SIZE,                ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_ALPHA_MASK_SIZE,           ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_BIND_TO_TEXTURE_RGB,       ATTRIB_TYPE_BOOLEAN,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_BIND_TO_TEXTURE_RGBA,      ATTRIB_TYPE_BOOLEAN,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_COLOR_BUFFER_TYPE,         ATTRIB_TYPE_ENUM,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_RGB_BUFFER },
   { EGL_CONFIG_CAVEAT,             ATTRIB_TYPE_ENUM,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_CONFIG_ID,                 ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_CONFORMANT,                ATTRIB_TYPE_BITMASK,
                                    ATTRIB_CRITERION_MASK,
                                    0 },
   { EGL_DEPTH_SIZE,                ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_LEVEL,                     ATTRIB_TYPE_PLATFORM,
                                    ATTRIB_CRITERION_EXACT,
                                    0 },
   { EGL_MAX_PBUFFER_WIDTH,         ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_IGNORE,
                                    0 },
   { EGL_MAX_PBUFFER_HEIGHT,        ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_IGNORE,
                                    0 },
   { EGL_MAX_PBUFFER_PIXELS,        ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_IGNORE,
                                    0 },
   { EGL_MAX_SWAP_INTERVAL,         ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_MIN_SWAP_INTERVAL,         ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_NATIVE_RENDERABLE,         ATTRIB_TYPE_BOOLEAN,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_NATIVE_VISUAL_ID,          ATTRIB_TYPE_PLATFORM,
                                    ATTRIB_CRITERION_IGNORE,
                                    0 },
   { EGL_NATIVE_VISUAL_TYPE,        ATTRIB_TYPE_PLATFORM,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_RENDERABLE_TYPE,           ATTRIB_TYPE_BITMASK,
                                    ATTRIB_CRITERION_MASK,
                                    EGL_OPENGL_ES_BIT },
   { EGL_SAMPLE_BUFFERS,            ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_SAMPLES,                   ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_STENCIL_SIZE,              ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_ATLEAST,
                                    0 },
   { EGL_SURFACE_TYPE,              ATTRIB_TYPE_BITMASK,
                                    ATTRIB_CRITERION_MASK,
                                    EGL_WINDOW_BIT },
   { EGL_TRANSPARENT_TYPE,          ATTRIB_TYPE_ENUM,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_NONE },
   { EGL_TRANSPARENT_RED_VALUE,     ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_TRANSPARENT_GREEN_VALUE,   ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_TRANSPARENT_BLUE_VALUE,    ATTRIB_TYPE_INTEGER,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE },
   { EGL_MATCH_NATIVE_PIXMAP,       ATTRIB_TYPE_PSEUDO,
                                    ATTRIB_CRITERION_SPECIAL,
                                    EGL_NONE },
   /* extensions */
   { EGL_Y_INVERTED_NOK,            ATTRIB_TYPE_BOOLEAN,
                                    ATTRIB_CRITERION_EXACT,
                                    EGL_DONT_CARE }
};

/*********************************************************************************************************************/

GraphicsConfig::GraphicsConfig( Graphics::Implementation &impl )
     :
     Config( &impl ),
     impl( impl )
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p )\n", __FUNCTION__, this );

}

GraphicsConfig::~GraphicsConfig()
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p )\n", __FUNCTION__, this );

}

DFBResult
GraphicsConfig::CheckOptions( const Graphics::Options &options )
{
     D_DEBUG_AT( DFBEGL_Config, "GraphicsConfig::%s( %p, options %p )\n", __FUNCTION__, this, &options );

     long attr, val, i;
     EGLBoolean matched = EGL_TRUE;

     for (i = 0; i < D_ARRAY_SIZE(_eglValidationTable); i++) {
        long cmp;

        if (_eglValidationTable[i].criterion == ATTRIB_CRITERION_IGNORE)
           continue;

        attr = _eglValidationTable[i].attr;

        cmp = EGL_DONT_CARE;
        cmp = options.GetValue( ToString<EGL::EGLInt>( EGL::EGLInt(attr) ), cmp );
        if (cmp == EGL_DONT_CARE)
           continue;

        GetOption( ToString<EGL::EGLInt>( EGL::EGLInt(attr) ), val );

        D_DEBUG_AT( DFBEGL_Config, "  -> checking '%s', shall be %4ld, is %4ld, criterion %d\n",
                    *ToString<EGL::EGLInt>( EGL::EGLInt(attr) ), cmp, val, _eglValidationTable[i].criterion );

        switch (_eglValidationTable[i].criterion) {
        case ATTRIB_CRITERION_EXACT:
           if (val != cmp) {
              D_DEBUG_AT( DFBEGL_Config, "  -> EXACT FAIL\n" );
              matched = EGL_FALSE;
           }
           else
              D_DEBUG_AT( DFBEGL_Config, "  -> exact match\n" );
           break;
        case ATTRIB_CRITERION_ATLEAST:
           if (val < cmp) {
              D_DEBUG_AT( DFBEGL_Config, "  -> AT LEAST FAIL\n" );
              matched = EGL_FALSE;
           }
           else
              D_DEBUG_AT( DFBEGL_Config, "  -> at least match\n" );
           break;
        case ATTRIB_CRITERION_MASK:
           if ((val & cmp) != cmp) {
              D_DEBUG_AT( DFBEGL_Config, "  -> MASK FAIL\n" );
              matched = EGL_FALSE;
           }
           else
              D_DEBUG_AT( DFBEGL_Config, "  -> mask match\n" );
           break;
        case ATTRIB_CRITERION_SPECIAL:
           /* ignored here */
           D_DEBUG_AT( DFBEGL_Config, "  -> SPECIAL\n" );
           break;
        default:
           D_ASSERT(0);
           break;
        }
     }

     if (!matched)
          return DFB_UNSUPPORTED;

     return Graphics::Config::CheckOptions( options );
}

/*********************************************************************************************************************/

Config::Config( Display          *display,
                Graphics::Config *gfx_config )
     :
     display( display ),
     gfx_config( gfx_config )
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p, display %p, gfx_config %p )\n",
                 __FUNCTION__, this, display, gfx_config );

}

Config::~Config()
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p )\n", __FUNCTION__, this );
}

EGLint
Config::GetAttrib( EGLint attribute, EGLint *value ) const
{
     D_DEBUG_AT( DFBEGL_Config, "EGL::Config::%s( %p, attribute 0x%08x (%d) '%s' )\n",
                 __FUNCTION__, this, attribute, attribute, *ToString<EGLInt>( EGLInt(attribute) ) );

     if (gfx_config) {
          long v;

//          if (attribute == EGL_SURFACE_TYPE) {
//               *value = EGL_WINDOW_BIT | EGL_PIXMAP_BIT | EGL_PBUFFER_BIT;
//               return EGL_SUCCESS;
//          }

          if (gfx_config->GetOption( ToString<EGLInt>( EGLInt(attribute) ), v ) == DFB_OK) {
               *value = v;
               return EGL_SUCCESS;
          }
     }

     return EGL_BAD_ATTRIBUTE;
}


}

}


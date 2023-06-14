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



#ifndef ___DirectFB__Util__H___
#define ___DirectFB__Util__H___

#ifdef __cplusplus

#include <direct/Types++.h>

extern "C" {
#endif

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/object.h>

#include <core/coretypes.h>


// C Wrapper

void FPS_Delete( DFB_Util_FPS *fps );


#ifdef __cplusplus
}


#include <direct/Magic.h>
#include <direct/String.hxx>
#include <direct/Type.h>


namespace DirectFB {


namespace Util {


#define DIRECTFB_UTIL_MAX_STACK    2048


template <typename T>
class TempArray
{
public:
     TempArray( size_t   size,
                const T *source = NULL )
          :
          size( size )
     {
          D_ASSERT( size > 0 );

          if (size > DIRECTFB_UTIL_MAX_STACK / sizeof(T))
               array = new T[size];
          else
               array = fixed;

          if (source != NULL)
               direct_memcpy( array, source, size * sizeof(T) );
     }

     ~TempArray()
     {
          if (array != fixed)
               delete[] array;
     }

     inline T * operator *() const {
          return array;
     }

     inline operator T * () const {
          return array;
     }

     size_t  size;
     T       fixed[DIRECTFB_UTIL_MAX_STACK / sizeof(T)];
     T      *array;
};


class FPS : public Direct::Magic<FPS>
{
private:
     long long frames;
     int       frames_per_1000s;
     long long fps_time;

public:
     FPS()
          :
          frames(0),
          frames_per_1000s(0),
          fps_time(direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ))
     {
     }

     bool
     Count( long long interval = 1000 )
     {
          long long diff;
          long long now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          CHECK_MAGIC();

          frames++;

          diff = (now - fps_time) / 1000LL;
          if (diff >= interval) {
               frames_per_1000s = frames * 1000000LL / diff;

               fps_time = now;
               frames   = 0;

               return true;
          }

          return false;
     }

     Direct::String
     Get()
     {
          CHECK_MAGIC();

          return Direct::String::F( "%d.%03d", frames_per_1000s / 1000, frames_per_1000s % 1000 );
     }
};




class Types : public Direct::Types<Types>
{
};


template <typename _FusionObject>
class FusionObjectWrapper : public Types::Type<FusionObjectWrapper<_FusionObject>>
{
public:
     _FusionObject *object;

     FusionObjectWrapper( _FusionObject *object = NULL )
          :
          object( object )
     {
          if (object) {
               D_INFO( "FusionObjectWrapper 0x%08x %s\n",
                       ((FusionObject*) object)->id, ((FusionObject*) object)->pool->name );
          }

          if (object != NULL) {
               DirectResult ret = fusion_object_ref( (FusionObject*) object );
               if (ret) {
                    D_DERROR( ret, "FusionObjectWrapper: fusion_object_ref() failed!\n" );
                    object = NULL;
               }
          }
     }

     ~FusionObjectWrapper()
     {
          if (object != NULL)
               fusion_object_unref( (FusionObject*) object );
     }

     FusionObjectWrapper &operator =( _FusionObject *object )
     {
          if (object) {
               D_INFO( "FusionObjectWrapper 0x%08x %s\n",
                       ((FusionObject*) object)->id, ((FusionObject*) object)->pool->name );
          }

          if (this->object != NULL)
               fusion_object_unref( (FusionObject*) this->object );

          this->object = object;

          if (object != NULL) {
               DirectResult ret = fusion_object_ref( (FusionObject*) object );
               if (ret) {
                    D_DERROR( ret, "FusionObjectWrapper: fusion_object_ref() failed!\n" );
                    this->object = NULL;
               }
          }

          return *this;
     }

     operator bool() { return object != NULL; }

     operator _FusionObject*() { return object; }

     _FusionObject* operator ->() { return object; }
};


}


}


#endif // __cplusplus

#endif


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

#ifndef ___DirectFB__DRMKMS__DFBGBM__H___
#define ___DirectFB__DRMKMS__DFBGBM__H___


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/KHR_image.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


namespace GBM {

using namespace DirectFB;


D_LOG_DOMAIN( DFBGBM_Device,        "DFBGBM/Device",         "DirectFB GBM Device" );
D_LOG_DOMAIN( DFBGBM_BoSurface,     "DFBGBM/BoSurface",      "DirectFB GBM Bo/Surface" );


class Types : public Direct::Types<Types>
{
};

class BoSurface;


class Device : public Types::Type<Device>
{
private:
     int                fd;
public:
     struct gbm_device *gbm;

public:
     Device()
          :
          fd( -1 ),
          gbm( NULL )
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p )\n", __FUNCTION__, this );
     }

     virtual ~Device()
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p )\n", __FUNCTION__, this );

          Close();
     }

     bool Open( const Direct::String &filename )
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p, '%s' )\n", __FUNCTION__, this, *filename );

          D_ASSERT( gbm == NULL );

          fd = open( *filename, O_RDWR );
          if (fd < 0) {
               D_PERROR( "GBM/Device: Opening '%s' failed!\n", *filename );
               return false;
          }

          gbm = gbm_create_device( fd );
          if (!gbm) {
               D_PERROR( "GBM/Device: gbm_create_device( fd %d from '%s' ) failed!\n", fd, *filename );
               Close();
               return false;
          }

          D_INFO( "DFBGBM: Opened '%s' (fd %d) as gbm_device %p\n", *filename, fd, gbm );

          return true;
     }

     bool OpenFd( int fd )
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p, fd %d )\n", __FUNCTION__, this, fd );

          D_ASSERT( gbm == NULL );

          gbm = gbm_create_device( fd );
          if (!gbm) {
               D_PERROR( "GBM/Device: gbm_create_device( fd %d ) failed!\n", fd );
               return false;
          }

          D_INFO( "DFBGBM: Opened fd %d as gbm_device %p\n", fd, gbm );

          this->fd = fd;

          return true;
     }

     void Close()
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p ) <- gbm %p\n", __FUNCTION__, this, gbm );

          if (gbm) {
               D_ASSERT( fd >= 0 );

               D_INFO( "DFBGBM: Closing fd %d from gbm_device %p\n", fd, gbm );

               gbm_device_destroy( gbm );
               gbm = NULL;

               close( fd );
               fd = -1;
          }
     }

     int Ioctl( unsigned long request, void *arg )
     {
          D_DEBUG_AT( DFBGBM_Device, "Device::%s( %p, request 0x%08lx, arg %p ) <- gbm %p\n", __FUNCTION__, this, request, arg, gbm );

          D_ASSERT( fd >= 0 );

          return drmIoctl( fd, request, arg );
     }
};



class BoSurface : public Types::Type<BoSurface> {
public:
     GBM::Device        &dev;
     struct gbm_surface *gbm_surface;
     struct gbm_bo      *gbm_bo;
     u32                 handle;
     u32                 name;
     bool                locked;

     CoreSurfaceAllocation *allocation;

     BoSurface( GBM::Device        &dev,
                struct gbm_surface *gbm_surface,
                struct gbm_bo      *gbm_bo )
          :
          dev( dev ),
          gbm_surface( gbm_surface ),
          gbm_bo( gbm_bo ),
          handle( 0 ),
          name( 0 ),
          locked( true ),
          allocation( NULL )
     {
          D_DEBUG_AT( DFBGBM_BoSurface, "BoSurface::%s( %p, surface %p, bo %p )\n", __FUNCTION__, this, gbm_surface, gbm_bo );

          DFBResult             ret;
          struct drm_gem_flink  fl;

          handle = gbm_bo_get_handle( gbm_bo ).u32;

          fl.handle = handle;
          fl.name   = 0;

          ret = (DFBResult) dev.Ioctl( DRM_IOCTL_GEM_FLINK, &fl );
          if (ret) {
               D_ERROR_AT( DFBGBM_BoSurface, "DRM_IOCTL_GEM_FLINK( 0x%x ) failed (%s)!\n", fl.handle, strerror(errno) );
          }
          else {
               name = fl.name;

               D_DEBUG_AT( DFBGBM_BoSurface, "  -> name      0x%x\n", name );
          }
     }

     ~BoSurface()
     {
          D_DEBUG_AT( DFBGBM_BoSurface, "BoSurface::%s( %p, surface %p, bo %p )\n", __FUNCTION__, this, gbm_surface, gbm_bo );

          release();

          if (allocation)
               dfb_surface_allocation_unref( allocation );
     }

     void release()
     {
          D_DEBUG_AT( DFBGBM_BoSurface, "BoSurface::%s( %p )\n", __FUNCTION__, this );

          D_ASSERT( locked );

          if (locked) {
               D_DEBUG_AT( DFBGBM_BoSurface, "  -> releasing bo %p of surface %p...\n", gbm_bo, gbm_surface );

               gbm_surface_release_buffer( gbm_surface, gbm_bo );

               locked = false;
          }
     }

     void reclaim()
     {
          D_DEBUG_AT( DFBGBM_BoSurface, "BoSurface::%s( %p )\n", __FUNCTION__, this );

          D_ASSERT( !locked );

          D_DEBUG_AT( DFBGBM_BoSurface, "  -> reclaimed bo %p of surface %p...\n", gbm_bo, gbm_surface );

          locked = true;
     }


public:
     static void
     destroy_user_data( struct gbm_bo *bo, void *data )
     {
          BoSurface *thiz = (BoSurface*) data;

          delete thiz;
     }
};


}


#endif


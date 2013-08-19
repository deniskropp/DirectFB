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

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#include <direct/Types++.h>

extern "C" {
#include <direct/messages.h>
#include <direct/thread.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/state.h>
#include <core/surface.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <display/idirectfbsurface.h>
}

#include <core/CoreSurface.h>
#include <core/Renderer.h>
#include <core/Task.h>


D_DEBUG_DOMAIN( Test_MyEngine, "Test/MyEngine", "Test MyEngine" );
D_DEBUG_DOMAIN( Test_MyTask,   "Test/MyTask",   "Test MyTask" );


class LockTask : public DirectFB::SurfaceTask
{
public:
     LockTask()
          :
          SurfaceTask( CSAID_CPU ),
          pushed( false )
     {
          direct_mutex_init( &lock );
          direct_waitqueue_init( &wq );
     }

     virtual ~LockTask()
     {
          direct_mutex_deinit( &lock );
          direct_waitqueue_deinit( &wq );
     }

     void Wait()
     {
          direct_mutex_lock( &lock );

          while (!pushed)
               direct_waitqueue_wait( &wq, &lock );

          direct_mutex_unlock( &lock );
     }

protected:
     virtual DFBResult Push()
     {
          direct_mutex_lock( &lock );

          pushed = true;

          direct_waitqueue_broadcast( &wq );

          direct_mutex_unlock( &lock );

          return DFB_OK;
     }

     virtual DFBResult Run()
     {
          return DFB_OK;
     }

private:
     DirectMutex     lock;
     DirectWaitQueue wq;
     bool            pushed;
};




int
main( int argc, char *argv[] )
{
     DFBResult              ret;
     IDirectFB             *dfb;
     DFBSurfaceDescription  desc;
     IDirectFBSurface      *dest;
     IDirectFBSurface_data *dest_data;

     CoreDFB               *core;
     CoreSurface           *dst;

     CardState              state;
     DirectFB::Renderer    *renderer;

     LockTask              *lock_task;

     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;


     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: DirectFBInit() failed!\n" );
          return ret;
     }


     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "CoreTest/Blit2: DirectFBCreate() failed!\n" );
          return ret;
     }

     /* Fill description for a primary surface. */
     desc.flags       = (DFBSurfaceDescriptionFlags)( DSDESC_PIXELFORMAT | DSDESC_CAPS );
     desc.caps        = (DFBSurfaceCapabilities)( DSCAPS_PRIMARY | DSCAPS_FLIPPING );
     desc.pixelformat = DSPF_ARGB;

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     /* Create a primary surface. */
     ret = dfb->CreateSurface( dfb, &desc, &dest );
     if (ret) {
          D_DERROR( ret, "DFBTest/FillRectangle: IDirectFB::CreateSurface() failed!\n" );
          goto error_dest;
     }

     dest->GetSize( dest, &desc.width, &desc.height );
     dest->GetPixelFormat( dest, &desc.pixelformat );

     D_INFO( "DFBTest/FillRectangle: Destination is %dx%d using %s\n",
             desc.width, desc.height, dfb_pixelformat_name(desc.pixelformat) );





     dfb_core_create( &core );


     dest_data = (IDirectFBSurface_data *)dest->priv;

     dst = dest_data->surface;



     long long               ms;

     ms = direct_clock_get_abs_millis();

     while (true) {
          long long now;


          dfb_surface_lock( dst );

          buffer = dfb_surface_get_buffer3( dst, CSBR_BACK, DSSE_LEFT, dst->flips );

          dfb_surface_unlock( dst );


          dfb_state_init( &state, core );

          dfb_state_set_destination( &state, dst );


          renderer = new DirectFB::Renderer( &state, NULL );


          for (int i=0; i<100000; i++) {
               DFBColor color = {
                    (u8)rand(), (u8)rand(), (u8)rand(), (u8)rand()
               };

               dfb_state_set_color( &state, &color );


               DFBRectangle rect = {
                    (u8)rand()%100, (u8)rand()%100, (u8)rand()%100, (u8)rand()%100
               };

               renderer->FillRectangles( &rect, 1 );
          }

          renderer->Flush();



          dfb_surface_lock( dst );

          allocation = dfb_surface_buffer_find_allocation( buffer, CSAID_CPU, CSAF_READ, true );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, CSAID_CPU, CSAF_READ, &allocation );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Buffer allocation failed!\n" );
                    _exit(0);
               }
          }

          dfb_surface_unlock( dst );

          lock_task = new LockTask();

          lock_task->AddAccess( allocation, CSAF_READ );

          lock_task->Flush();

          D_INFO( "Waiting...\n" );

          lock_task->Wait();

          D_INFO( "Done.\n" );

          lock_task->Done();



          dest->Flip( dest, NULL, DSFLIP_NONE );

          //sleep(5);
          
          

          now = direct_clock_get_abs_millis();

          D_INFO( "Took %lld ms\n", now - ms );

          ms = now;


          delete renderer;

          dfb_state_set_destination( &state, NULL );

          dfb_state_destroy( &state );
     }


//error:
     dfb_core_destroy( core, false );

     dest->Release( dest );

error_dest:
     /* Shutdown DirectFB. */
     dfb->Release( dfb );

     return ret;
}


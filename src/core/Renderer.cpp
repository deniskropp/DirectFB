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

#include <config.h>

#include "Renderer.h"

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>
}

D_DEBUG_DOMAIN( DirectFB_Renderer, "DirectFB/Renderer", "DirectFB Renderer" );

/*********************************************************************************************************************/

namespace DirectFB {


Renderer::Renderer( CardState *state )
     :
     state( state ),
     engine( NULL ),
     setup( NULL )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

}

Renderer::~Renderer()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     Flush();
}


void
Renderer::Flush()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (engine)
          unbindEngine();
}


DFBResult
Renderer::enterLock( CoreSurfaceBufferLock  *lock,
                     CoreSurfaceAllocation  *allocation,
                     CoreSurfaceAccessFlags  flags )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     /*
        => move to engine / task
     */
     dfb_surface_buffer_lock_init( lock, setup->tasks[0]->accessor, flags );
     dfb_surface_pool_lock( allocation->pool, allocation, lock );

     return DFB_OK;
}

DFBResult
Renderer::leaveLock( CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     /*
        => move to engine / task
     */
     if (lock->buffer) {
          dfb_surface_pool_unlock( lock->allocation->pool, lock->allocation, lock );
          dfb_surface_buffer_lock_deinit( lock );
     }

     return DFB_OK;
}

DFBResult
Renderer::updateLock( CoreSurfaceBufferLock  *lock,
                      CoreSurface            *surface,
                      CoreSurfaceBufferRole   role,
                      DFBSurfaceStereoEye     eye,
                      u32                     flips,
                      CoreSurfaceAccessFlags  flags )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     SurfaceAllocationMap::iterator it;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );


     leaveLock( lock );


     SurfaceAllocationKey key( surface->object.id, role, eye, flips );

     it = allocations.find( key );
     if (it != allocations.end()) {
          allocation = (*it).second;

     }
     else {
          dfb_surface_lock( surface );

          // FIXME: move to helper class
          //

          buffer = dfb_surface_get_buffer3( surface, role, eye, flips );

          allocation = dfb_surface_buffer_find_allocation( buffer, setup->tasks[0]->accessor, flags, true );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, setup->tasks[0]->accessor, flags, &allocation );
               if (ret) {
                    D_DERROR( ret, "DirectFB/Renderer: Buffer allocation failed!\n" );
                    dfb_surface_unlock( surface );
                    return ret;
               }
          }

          // FIXME: sync allocation

          setup->tasks[0]->AddAccess( allocation, flags );

          dfb_surface_unlock( surface );

          // FIXME: this is a temporary solution, slaves will be blocked via kernel module later
          while (allocation->task_count > 3)
               usleep( 1000 );


          allocations.insert( SurfaceAllocationMapPair( key, allocation ) );
     }


     enterLock( lock, allocation, flags );

     return DFB_OK;
}

void
Renderer::update( DFBAccelerationMask accel )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     state->mod_hw = (StateModificationFlags)(state->mod_hw | state->modified);

     // TODO: finish state updates
//     if (state->modified) {
          D_ASSERT( state->destination != NULL );

          if (state->modified & SMF_DESTINATION || !state->dst.buffer)
               updateLock( &state->dst, state->destination, state->to, state->to_eye, state->destination->flips,
                           (CoreSurfaceAccessFlags)( CSAF_WRITE | CSAF_READ ) );

          // FIXME: may miss an update?
          if (DFB_BLITTING_FUNCTION( accel )) {
               D_ASSERT( state->source != NULL );

               if (state->modified & SMF_SOURCE || !state->src.buffer)
                    updateLock( &state->src, state->source, state->from, state->from_eye, state->source->flips, CSAF_READ );

               // TODO: Add other sources
          }
//     }

     // TODO: optimise for relevant changes only?
     if (state->mod_hw || !(state->set & accel)) {
          DFBRegion              clip     = state->clip;
          StateModificationFlags modified = state->mod_hw;

          /// loop, clip switch, task mask (total clip)

          for (unsigned int i=0; i<setup->tiles; i++) {
               state->clip.x1 = MAX( clip.x1, setup->clips[i].x1 );
               state->clip.y1 = MAX( clip.y1, setup->clips[i].y1 );
               state->clip.x2 = MIN( clip.x2, setup->clips[i].x2 );
               state->clip.y2 = MIN( clip.y2, setup->clips[i].y2 );

               engine->SetState( setup->tasks[i], state, modified, accel );
          }

          state->clip = clip;
     }

     state->modified = SMF_NONE;
}

void
Renderer::FillRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_FILLRECTANGLE )) {
          update( DFXL_FILLRECTANGLE );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++)
               engine->FillRectangles( setup->tasks[i], rects, num_rects );
     }
}

void
Renderer::Blit( const DFBRectangle     *rects,
                const DFBPoint         *points,
                u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (checkEngine( DFXL_BLIT )) {
          update( DFXL_BLIT );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles; i++)
               engine->Blit( setup->tasks[i], rects, points, num );
     }
}


bool
Renderer::checkEngine( DFBAccelerationMask accel )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (engine) {
          // TODO: add cache as in gfxcard.c ('checked')

          /// use task[0]?
          if (!(state->modified & SMF_DESTINATION) && engine->CheckState( setup->tasks[0], state, accel ) == DFB_OK)
               return true;

          unbindEngine();
     }

     for (std::list<Engine*>::const_iterator it = engines.begin(); it != engines.end(); ++it) {
          Engine *engine = *it;

          // TODO: add engine mask for selection by user
          // TODO: add cache as in gfxcard.c ('checked')

          if (engine->CheckState( NULL, state, accel ) == DFB_OK) {
               ret = bindEngine( engine );
               if (ret == DFB_OK)
                    return true;
          }
     }

     return false;
}

DFBResult
Renderer::bindEngine( Engine *engine )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( this->engine == NULL );
     D_ASSERT( setup == NULL );

     /// loop
     setup = new Setup( state->destination->config.size.w, state->destination->config.size.h, engine->cores );

     ret = engine->bind( setup );
     if (ret) {
          D_DERROR( ret, "DirectFB/Renderer: Failed to bind engine!\n" );
          return ret;
     }

#if D_DEBUG_ENABLED
     for (unsigned int i=0; i<setup->tiles; i++) {
          D_ASSERT( setup->tasks[i] != NULL );
          DFB_REGION_ASSERT( &setup->clips[i] );
     }
#endif

     /// prepare for par flush
     for (unsigned int i=1; i<setup->tiles; i++) {
          setup->tasks[0]->AddSlave( setup->tasks[i] );
     }

     state->modified = SMF_ALL;
     state->set      = DFXL_NONE;

     this->engine = engine;

     return DFB_OK;
}

void
Renderer::unbindEngine()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( engine != NULL );
     D_ASSERT( setup != NULL );

     leaveLock( &state->src );
     leaveLock( &state->dst );

     /// par flush
     setup->tasks[0]->Flush();

     delete setup;
     setup = NULL;

     engine = NULL;

     allocations.clear();
}


/*********************************************************************************************************************/

std::list<Engine*>  Renderer::engines;


DFBResult
Renderer::RegisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.push_back( engine );

     return DFB_OK;
}

void
Renderer::UnregisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.remove( engine );
}

/*********************************************************************************************************************/

DFBResult
Engine::FillRectangles( SurfaceTask        *task,
                        const DFBRectangle *rects,
                        unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::Blit( SurfaceTask        *task,
              const DFBRectangle *rects,
              const DFBPoint     *points,
              u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}



















D_DEBUG_DOMAIN( Test_MyEngine, "Test/MyEngine", "Test MyEngine" );
D_DEBUG_DOMAIN( Test_MyTask,   "Test/MyTask",   "Test MyTask" );










class MyEngine;

class MyTask : public DirectFB::SurfaceTask
{
public:
     MyTask( MyEngine *engine )
          :
          SurfaceTask( CSAID_CPU ),
          engine( engine )
     {
     }

     virtual ~MyTask()
     {
     }

protected:
     virtual DFBResult Push();
     virtual DFBResult Run();

private:
     friend class MyEngine;

     MyEngine *engine;

     typedef enum {
          TYPE_SET_DESTINATION,
          TYPE_SET_SOURCE,
          TYPE_SET_COLOR,
          TYPE_FILL_RECTS,
          TYPE_BLIT
     } Type;

     std::vector<u32> commands;
};


class MyEngine : public DirectFB::Engine {
public:
     MyEngine()
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (int i=0; i<1; i++) {
               char name[] = "MyEngineX";

               name[8] = '0' + i;

               threads[i] = direct_thread_create( DTT_DEFAULT, myEngineLoop, this, name );
          }
     }


     virtual DFBResult bind          ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->tasks[i] = new MyTask( this );
          }

          return DFB_OK;
     }

     virtual DFBResult CheckState    ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       DFBAccelerationMask     accel )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( task %p )\n", __FUNCTION__, task );

          if (task) {
               MyTask *mytask = (MyTask *)task;

               if (mytask->commands.size() > 40000)
                    return DFB_LIMITEXCEEDED;
          }

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (accel != DFXL_BLIT || //state->blittingflags ||
                   state->destination->config.format != DSPF_ARGB || state->source->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }
          else {
               if (accel != DFXL_FILLRECTANGLE || state->drawingflags ||
                   state->destination->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }

          return DFB_OK;
     }

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          if (modified & SMF_DESTINATION) {
               mytask->commands.push_back( MyTask::TYPE_SET_DESTINATION );
               mytask->commands.push_back( (long long)(long)state->dst.addr >> 32 );
               mytask->commands.push_back( (u32)(long)state->dst.addr );
               mytask->commands.push_back( state->dst.pitch );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_DESTINATION);
          }

          if (modified & SMF_COLOR) {
               mytask->commands.push_back( MyTask::TYPE_SET_COLOR );
               mytask->commands.push_back( PIXEL_ARGB( state->color.a, state->color.r, state->color.g, state->color.b ) );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_COLOR);
          }

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (modified & SMF_SOURCE) {
                    mytask->commands.push_back( MyTask::TYPE_SET_SOURCE );
                    mytask->commands.push_back( (long long)(long)state->src.addr >> 32 );
                    mytask->commands.push_back( (u32)(long)state->src.addr );
                    mytask->commands.push_back( state->src.pitch );

                    state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_SOURCE);
               }
          }

          return DFB_OK;
     }


     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num_rects );

          mytask->commands.push_back( MyTask::TYPE_FILL_RECTS );
          mytask->commands.push_back( num_rects );

          for (unsigned int i=0; i<num_rects; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
          }

          return DFB_OK;
     }


     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             u32                     num )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num );

          mytask->commands.push_back( MyTask::TYPE_BLIT );
          mytask->commands.push_back( num );

          for (unsigned int i=0; i<num; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
               mytask->commands.push_back( points[i].x );
               mytask->commands.push_back( points[i].y );
          }

          return DFB_OK;
     }


private:
     friend class MyTask;

     DirectFB::FIFO<MyTask*>  fifo;
     DirectThread            *threads[1];

     static void *
     myEngineLoop( DirectThread *thread,
                   void         *arg )
     {
          MyEngine *engine = (MyEngine *)arg;
          MyTask   *task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          while (true) {
               task = engine->fifo.pull();

               task->Run();
          }

          return NULL;
     }
};


DFBResult
MyTask::Push()
{
     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     engine->fifo.push( this );

     return DFB_OK;
}

DFBResult
MyTask::Run()
{
     u32     ptr1      = 0;
     u32     ptr2      = 0;
     void   *ptr       = 0;
     u32     pitch     = 0;
     void   *src_ptr   = 0;
     u32     src_pitch = 0;
     u32     color     = 0;
     u32     num;
     size_t  size;

     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     size = commands.size();
     if (size > 0) {
          u32 *buffer = &commands[0];

          for (unsigned int i=0; i<size; i++) {
               D_DEBUG_AT( Test_MyTask, "  -> next command at [%d]\n", i );

               switch (buffer[i]) {
                    case MyTask::TYPE_SET_DESTINATION:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_DESTINATION\n" );

                         ptr1  = buffer[++i];
                         ptr2  = buffer[++i];
                         ptr   = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, ptr );

                         pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", pitch );
                         break;

                    case MyTask::TYPE_SET_SOURCE:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_SOURCE\n" );

                         ptr1    = buffer[++i];
                         ptr2    = buffer[++i];
                         src_ptr = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, src_ptr );

                         src_pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", src_pitch );
                         break;

                    case MyTask::TYPE_SET_COLOR:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_COLOR\n" );

                         color = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x\n", color );
                         break;

                    case MyTask::TYPE_FILL_RECTS:
                         D_DEBUG_AT( Test_MyTask, "  -> FILL_RECTS\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x = buffer[++i];
                              int y = buffer[++i];
                              int w = buffer[++i];
                              int h = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d\n", x, y, w, h );

                              u32 *d = (u32 *)((u8*)ptr + pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = color;
                                   }

                                   d += pitch/4;
                              }
                         }
                         break;

                    case MyTask::TYPE_BLIT:
                         D_DEBUG_AT( Test_MyTask, "  -> BLIT\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x  = buffer[++i];
                              int y  = buffer[++i];
                              int w  = buffer[++i];
                              int h  = buffer[++i];
                              int dx = buffer[++i];
                              int dy = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d\n", x, y, w, h, dx, dy );

                              u32 *d = (u32 *)((u8*)ptr + pitch * dy + dx * 4);
                              u32 *s = (u32 *)((u8*)src_ptr + src_pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = s[X];
                                   }

                                   d += pitch/4;
                                   s += src_pitch/4;
                              }
                         }
                         break;

                    default:
                         D_BUG( "unknown type %d", buffer[i] );
               }
          }
     }

     Done();

     return DFB_OK;
}


extern "C" {
     void
     register_myengine()
     {
          Renderer::RegisterEngine( new MyEngine() );
     }
}



}


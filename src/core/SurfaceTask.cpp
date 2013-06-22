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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#include "SurfaceTask.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <misc/conf.h>
}

#include <direct/Lists.h>

#include <core/Debug.h>
#include <core/Task.h>

/*********************************************************************************************************************/

namespace DirectFB {


extern "C" {

SurfaceTask *
SurfaceTask_New( CoreSurfaceAccessorID accessor )
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     return new SurfaceTask( accessor );
}

DFBResult
SurfaceTask_AddAccess( SurfaceTask            *task,
                       CoreSurfaceAllocation  *allocation,
                       CoreSurfaceAccessFlags  flags )
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     return task->AddAccess( allocation, flags );
}

}

/*********************************************************************************************************************/

const Direct::String SurfaceTask::_Type( "Surface" );

const Direct::String &
SurfaceTask::TypeName() const
{
     return _Type;
}

SurfaceTask::SurfaceTask( CoreSurfaceAccessorID accessor )
     :
     accessor( accessor )
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( accessor 0x%02x )\n", __FUNCTION__, accessor );

}

/*********************************************************************************************************************/

DFBResult
SurfaceTask::AddAccess( CoreSurfaceAllocation  *allocation,
                        CoreSurfaceAccessFlags  flags )
{
     DirectResult ret;

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( %p, allocation %p [%dx%d], flags 0x%02x )\n",
                 __FUNCTION__, this, allocation, allocation->config.size.w, allocation->config.size.h, flags );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, return DFB_BUG );

     ret = dfb_surface_allocation_ref( allocation );
     if (ret)
          return (DFBResult) ret;

     accesses.push_back( SurfaceAllocationAccess( allocation, flags ) );

     D_SYNC_ADD( &allocation->task_count, 1 );

     return DFB_OK;
}

DFBResult
SurfaceTask::AddHook( Hook *hook )
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( %p, hook %p )\n", __FUNCTION__, this, hook );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, return DFB_BUG );

     hooks.push_back( hook );

     return DFB_OK;
}

DFBResult
SurfaceTask::Setup()
{
     DFBResult ret;

     DFB_TASK_LOG( "SurfaceTask::Setup()" );

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( hooks:%zu accesses:%zu )\n", __FUNCTION__, hooks.size(), accesses.size() );

     DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return DFB_BUG );

     for (std::vector<Hook*>::const_iterator it = hooks.begin(); it != hooks.end(); ++it) {
          ret = (*it)->setup( this );
          if (ret) {
               D_DERROR( ret, "DirectFB/SurfaceTask: Hook's setup() failed!\n" );
               return ret;
          }
     }

     for (size_t a=0; a<accesses.size(); a++) {
          SurfaceAllocationAccess &access = accesses[a];

          if (!access.allocation->read_tasks)
               access.allocation->read_tasks = new DFB_SurfaceTaskListSimple;

          DFB_SurfaceTaskListSimple &read_tasks = *access.allocation->read_tasks;

          D_DEBUG_AT( DirectFB_Task, "  [%zu] %s\n", a, *ToString<SurfaceAllocationAccess>(access) );

          /* set invalidate flag in case this accessor has not yet invalidated its cache for this allocation */
          if (!(access.allocation->invalidated & (1 << accessor))) {
               D_FLAGS_SET( access.flags, CSAF_CACHE_INVALIDATE );

               /* set this accessor's invalidated flag */
               access.allocation->invalidated |= (1 << accessor);
          }

          if (D_FLAGS_IS_SET( access.flags, CSAF_WRITE )) {
               /* clear all accessors' invalidated flag except our own */
               access.allocation->invalidated &= (1 << accessor);
               D_ASSUME( access.allocation->invalidated & (1 << accessor) );

               if (read_tasks.Length()) {
                    size_t r = 0;

                    D_DEBUG_AT( DirectFB_Task, "    -> read_tasks:%zu (clearing)\n", read_tasks.Length() );

                    for (DFB_SurfaceTaskListSimple::const_iterator it=read_tasks.begin(); it != read_tasks.end(); it++, r++) {
                         SurfaceTask *read_task = (*it).second;

                         D_DEBUG_AT( DirectFB_Task, "       [%zu] %s\n", r, *ToString<Task>(*read_task) );

                         read_task->AddNotify( this, (read_task->accessor == accessor && read_task->qid == qid) ||
                                                     (flags & TASK_FLAG_FOLLOW_READER) );
                    }

                    read_tasks.Clear();
               }
               else if (access.allocation->write_task) {
                    SurfaceTask             *write_task   = access.allocation->write_task;
                    SurfaceAllocationAccess *write_access = (SurfaceAllocationAccess *) access.allocation->write_access;

                    D_ASSERT( write_access != NULL );

                    D_DEBUG_AT( DirectFB_Task, "    -> write_task   %s\n", *ToString<Task>(*write_task) );
                    D_DEBUG_AT( DirectFB_Task, "    -> write_access %s\n", *ToString<SurfaceAllocationAccess>(*write_access) );

                    /* if the last write task still exists from same accessor (ready/running), clear its
                       flush flags, hoping the task implementation can avoid the flush (still) */
                    if (write_task->accessor == accessor)
                         D_FLAGS_CLEAR( write_access->flags, CSAF_CACHE_FLUSH );

                    write_task->AddNotify( this, (write_task->accessor == accessor && write_task->qid == qid) ||
                                                 (flags & TASK_FLAG_FOLLOW_WRITER) );
               }
               else
                    D_ASSERT( access.allocation->write_access == NULL );

               /* set flush flag per default, will be cleared when another task from same accessor is following */
               D_FLAGS_SET( access.flags, CSAF_CACHE_FLUSH );

               access.allocation->write_task   = this;
               access.allocation->write_access = &access;
          }
          else {
               if (access.allocation->write_task) {
                    SurfaceTask             *write_task   = access.allocation->write_task;
                    SurfaceAllocationAccess *write_access = (SurfaceAllocationAccess *) access.allocation->write_access;

                    (void) write_access;
                    D_ASSERT( write_access != NULL );

                    D_DEBUG_AT( DirectFB_Task, "    -> write_task   %s\n", *ToString<Task>(*write_task) );
                    D_DEBUG_AT( DirectFB_Task, "    -> write_access %s\n", *ToString<SurfaceAllocationAccess>(*write_access) );

                    // TODO: avoid cache flush in write task if accessor equals,
                    // requires special handling to take care about other read tasks
                    // and to carry on the flush flag to the read task

                    write_task->AddNotify( this, (write_task->accessor == accessor && write_task->qid == qid) ||
                                                 (flags & TASK_FLAG_FOLLOW_WRITER) );
               }

               // TODO: optimise in case we are already added, then just replace the task, maybe use static array with entry per accessor
               read_tasks.Append( this );
          }
     }

     return Task::Setup();
}

DFBResult
SurfaceTask::CacheInvalidate()
{
     DFBResult ret = DFB_OK;

     DFB_TASK_LOG( "SurfaceTask::CacheInvalidate()" );

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     if (slaves)
          DFB_TASK_CHECK_STATE( this, TASK_RUNNING | TASK_DONE, return DFB_BUG );
     else
          DFB_TASK_CHECK_STATE( this, TASK_RUNNING, return DFB_BUG );

     /* Slaves need to call master task which manages the accesses */
     if (master)
          return ((SurfaceTask*)master)->CacheInvalidate();

     /* Invalidate cache for allocations which require it */
     for (unsigned int i=0; i<accesses.size(); i++) {
          CoreSurfaceAllocation *allocation = accesses[i].allocation;

          if (accesses[i].flags & CSAF_CACHE_INVALIDATE) {
               DFBResult r = dfb_surface_pool_cache_op( allocation->pool, allocation, accessor, false, true );
               D_DEBUG_AT( DirectFB_Task, "  -> %s\n", DirectResultString((DirectResult)r) );
               ret = r ? r : ret;
          }
     }

     return ret;
}

DFBResult
SurfaceTask::CacheFlush()
{
     DFBResult ret = DFB_OK;

     DFB_TASK_LOG( "SurfaceTask::CacheFlush()" );

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     if (slaves)
          DFB_TASK_CHECK_STATE( this, TASK_RUNNING | TASK_DONE, return DFB_BUG );
     else
          DFB_TASK_CHECK_STATE( this, TASK_RUNNING, return DFB_BUG );

     /* Slaves need to call master task which manages the accesses */
     if (master)
          return ((SurfaceTask*)master)->CacheFlush();

     /* Flush cache for allocations which require it */
     for (unsigned int i=0; i<accesses.size(); i++) {
          CoreSurfaceAllocation *allocation = accesses[i].allocation;

          if (accesses[i].flags & CSAF_CACHE_FLUSH) {
               DFBResult r = dfb_surface_pool_cache_op( allocation->pool, allocation, accessor, true, false );
               D_DEBUG_AT( DirectFB_Task, "  -> %s\n", DirectResultString((DirectResult)r) );
               ret = r ? r : ret;
          }
     }

     return DFB_OK;
}

void
SurfaceTask::Finalise()
{
     DFB_TASK_LOG( "SurfaceTask::Finalise()" );

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     DFB_TASK_CHECK_STATE( this, TASK_DONE, return );

     for (std::vector<Hook*>::const_iterator it = hooks.begin(); it != hooks.end(); ++it)
          (*it)->finalise( this );

     Task::Finalise();

     for (std::vector<SurfaceAllocationAccess>::const_iterator it = accesses.begin(); it != accesses.end(); ++it) {
          const SurfaceAllocationAccess &access     = *it;
          CoreSurfaceAllocation         *allocation = access.allocation;

          if (D_FLAGS_IS_SET( access.flags, CSAF_WRITE )) {
               if (allocation->write_task == this) {
                    allocation->write_task   = NULL;
                    allocation->write_access = NULL;
               }
          }
          else {
               DFB_SurfaceTaskListSimple &read_tasks = *access.allocation->read_tasks;

               read_tasks.Remove( this );
          }

          D_SYNC_ADD( &allocation->task_count, -1 );

          dfb_surface_allocation_unref( allocation );
     }

     accesses.clear();
}

void
SurfaceTask::Describe( Direct::String &string ) const
{
     Task::Describe( string );

     string.PrintF( "  accessor 0x%02x, accesses %zu, allocation %p, index %d", accessor, accesses.size(),
                    accesses.size() > 0 ? accesses[0].allocation : NULL,
                    accesses.size() > 0 ? accesses[0].allocation->index : -1 );
}


}

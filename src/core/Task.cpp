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

#include "Task.h"

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/surface_allocation.h>
}

D_DEBUG_DOMAIN( DirectFB_Task, "DirectFB/Task", "DirectFB Task" );

/*********************************************************************************************************************/

namespace DirectFB {


extern "C" {

DFBResult
TaskManager_Initialise()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     return TaskManager::Initialise();
}

void
TaskManager_Shutdown()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     TaskManager::Shutdown();
}

void
TaskManager_Sync()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     TaskManager::Sync();
}


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

void
SurfaceTask_Flush( SurfaceTask *task )
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     task->Flush();
}

}



Task::Task()
     :
     block_count(0),
     slaves( 0 ),
     master( NULL ),
     next_slave( NULL ),
     finished( false )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     state = TASK_NEW;
}


Task::~Task()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );


}

void
Task::AddSlave( Task *slave )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_NEW );
     D_ASSERT( slave->state == TASK_NEW );

     slave->master = this;

     slaves++;

     if (next_slave)
          slave->next_slave = next_slave;

     next_slave = slave;
}


void
Task::Flush()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_NEW );

     state = TASK_FLUSHED;

     TaskManager::pushTask( this );
}

DFBResult
Task::emit()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_READY );
     D_ASSERT( block_count == 0 );

     state = TASK_RUNNING;

     // FIXME: error handling
     Push();

     Task *next = next_slave;

     while (next) {
          Task *slave = next;

          next = slave->next_slave;

          slave->state = TASK_RUNNING;

          // FIXME: error handling
          slave->Push();
     }

     return DFB_OK;
}

DFBResult
Task::finish()
{
     Task *shutdown = NULL;

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     finished = true;

     if (master) { /* has master? */
          D_ASSERT( slaves == 0 );
          D_ASSERT( master->slaves > 0 );

          if (!--(master->slaves)) {
               if (master->finished) {
                    D_DEBUG_AT( DirectFB_Task, "  -> I am the last slave, shutting down master\n" );

                    shutdown = master;
               }
               else
                    D_DEBUG_AT( DirectFB_Task, "  -> I am the last slave, but master is not finished\n" );
          }
          else
               D_DEBUG_AT( DirectFB_Task, "  -> I am slave, remaining running slaves %d\n", master->slaves );
     }
     else if (slaves) { /* has running slaves? */
          D_DEBUG_AT( DirectFB_Task, "  -> I am master, but there are still slaves running\n" );

          return DFB_OK;
     }
     else {
          D_DEBUG_AT( DirectFB_Task, "  -> I am master, no more slaves running, shutting down\n" );

          shutdown = this;
     }

     /*
      * master task shutdown
      */
     if (shutdown) {
          shutdown->notifyAll();
          shutdown->Finalise();

          Task *next = shutdown->next_slave;

          while (next) {
               Task *slave = next;

               next = slave->next_slave;

               delete slave;
          }

          delete shutdown;

          D_SYNC_ADD( &TaskManager::task_count, -1 );
     }

     return DFB_OK;
}

void
Task::Done()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_RUNNING );

     state = TASK_DONE;

     TaskManager::pushTask( this );
}


DFBResult
Task::Setup()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_FLUSHED );

     state = TASK_READY;

     return DFB_OK;
}

DFBResult
Task::Push()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_RUNNING );

     return Run();
}

DFBResult
Task::Run()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_RUNNING );

     Done();

     return DFB_OK;
}

void
Task::Finalise()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_DONE );
}

void
Task::addNotify( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state != TASK_NEW );
     D_ASSERT( state != TASK_FLUSHED );

     notifies.push_back( task );

     task->block_count++;
}

void
Task::notifyAll()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_DONE );

     for (std::vector<Task*>::const_iterator it = notifies.begin(); it != notifies.end(); ++it)
          (*it)->handleNotify();
}

void
Task::handleNotify()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_READY );
     D_ASSERT( block_count > 0 );

     if (--block_count == 0) {
          long long t1, t2;

          t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          emit();

          t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          if (t2 - t1 >= 5000)
               D_WARN( "Task::Emit took more than 5ms (%lld)", (t2 - t1) / 1000 );
     }
}


/*********************************************************************************************************************/

DirectThread *TaskManager::thread;
FIFO<Task*>   TaskManager::fifo;
unsigned int  TaskManager::task_count;


DFBResult
TaskManager::Initialise()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     D_ASSERT( thread == NULL );

     thread = direct_thread_create( DTT_CRITICAL, managerLoop, NULL, "Task Manager" );

     return DFB_OK;
}

void
TaskManager::Shutdown()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     if (thread != NULL) {
          // FIXME: wakeup managerLoop

          direct_thread_join( thread );
          direct_thread_destroy( thread );

          thread = NULL;
     }
}

void
TaskManager::Sync()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     // FIXME: this is a hack, will avoid Sync() at all
     while (task_count)
          usleep( 1000 );
}

void
TaskManager::pushTask( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s( %p )\n", __FUNCTION__, task );

     if (task->state == TASK_FLUSHED)
          D_SYNC_ADD( &task_count, 1 );

     fifo.push( task );
}

Task *
TaskManager::pullTask()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     return fifo.pull();
}

DFBResult
TaskManager::handleTask( Task *task )
{
     DFBResult ret;
     long long t1, t2;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s( %p )\n", __FUNCTION__, task );

     switch (task->state) {
          case TASK_FLUSHED:
               D_DEBUG_AT( DirectFB_Task, "  -> FLUSHED\n" );

               t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

               ret = task->Setup();
               if (ret) {
                    D_DERROR( ret, "DirectFB/TaskManager: Task::Setup() failed!\n" );
                    task->state = TASK_DONE;
                    goto finish;
               }

               t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
               if (t2 - t1 >= 5000)
                    D_WARN( "Task::Setup took more than 5ms (%lld)", (t2 - t1) / 1000 );


               if (task->block_count == 0) {
                    t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

                    ret = task->emit();
                    if (ret) {
                         D_DERROR( ret, "DirectFB/TaskManager: Task::Emit() failed!\n" );
                         task->state = TASK_DONE;
                         goto finish;
                    }

                    t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
                    if (t2 - t1 >= 5000)
                         D_WARN( "Task::Emit took more than 5ms (%lld)", (t2 - t1) / 1000 );
               }

               // TODO: also emit subsequent tasks if possible, e.g. same accelerator having another FIFO anyways
               break;

          case TASK_DONE:
               D_DEBUG_AT( DirectFB_Task, "  -> DONE\n" );

finish:
               task->finish();
               break;

          default:
               D_BUG( "invalid task state %d", task->state );
     }

     return DFB_OK;
}

void *
TaskManager::managerLoop( DirectThread *thread,
                          void         *arg )
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     while (true) {
          Task *task = TaskManager::pullTask();

          D_DEBUG_AT( DirectFB_Task, "  =-> Task %p\n", task );

          TaskManager::handleTask( task );
     }

     return NULL;
}

/*********************************************************************************************************************/


SurfaceTask::SurfaceTask( CoreSurfaceAccessorID accessor )
     :
     accessor( accessor )
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( accessor 0x%02x )\n", __FUNCTION__, accessor );

}

DFBResult
SurfaceTask::AddAccess( CoreSurfaceAllocation  *allocation,
                        CoreSurfaceAccessFlags  flags )
{
     DirectResult ret;

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s( allocation %p [%dx%d], flags 0x%02x )\n", __FUNCTION__, allocation,
                 allocation->config.size.w, allocation->config.size.h, flags );

     D_ASSERT( state == TASK_NEW );

     ret = dfb_surface_allocation_ref( allocation );
     if (ret)
          return (DFBResult) ret;

     accesses.push_back( SurfaceAllocationAccess( allocation, flags ) );

     return DFB_OK;
}

DFBResult
SurfaceTask::Setup()
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_FLUSHED );

     for (std::vector<SurfaceAllocationAccess>::const_iterator it = accesses.begin(); it != accesses.end(); ++it) {
          SurfaceAllocationAccess  access     = *it;
          CoreSurfaceAllocation   *allocation = access.first;
          CoreSurfaceAccessFlags   flags      = access.second;

          // TODO: implement cache invalidate / flush tagging incl. optimisation for subsequent tasks of same accessor

          D_DEBUG_AT( DirectFB_Task, "  -> allocation %p, task count %d\n", allocation, allocation->task_count );

//          if (allocation->task_count > 3) {
//               D_DEBUG_AT( DirectFB_Task, "  -> post poning task\n" );
//               return DFB_BUSY;
//          }

          allocation->task_count++;

          if ((flags & CSAF_WRITE) != 0) {
               SurfaceTask *read_task;

               D_DEBUG_AT( DirectFB_Task, "  -> WRITE\n" );

               if (allocation->read_tasks.count) {
                    int index;

                    fusion_vector_foreach (read_task, index, allocation->read_tasks) {
                         read_task->addNotify( this );
                    }

                    // FIXME: reset vector (free array)?
                    D_ASSUME( allocation->read_tasks.count < 10 );
                    allocation->read_tasks.count = 0;
               }
               else if (allocation->write_task) {
                    SurfaceTask *write_task = (SurfaceTask *)allocation->write_task;

                    //if (write_task->accessor != accessor)
                         write_task->addNotify( this );
               }

               allocation->write_task = this;
          }
          else {
               D_DEBUG_AT( DirectFB_Task, "  -> READ\n" );

               if (allocation->write_task) {
                    SurfaceTask *write_task = (SurfaceTask *)allocation->write_task;

                    //if (write_task->accessor != accessor)
                         write_task->addNotify( this );
               }

               fusion_vector_add( &allocation->read_tasks, this );
          }
     }

     state = TASK_READY;

     return DFB_OK;
}

void
SurfaceTask::Finalise()
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_DONE );

     for (std::vector<SurfaceAllocationAccess>::const_iterator it = accesses.begin(); it != accesses.end(); ++it) {
          SurfaceAllocationAccess  access     = *it;
          CoreSurfaceAllocation   *allocation = access.first;
          int                      index;

          if (allocation->write_task == this) {
               allocation->write_task = NULL;

               D_ASSERT( fusion_vector_index_of( &allocation->read_tasks, this ) < 0 );
          }
          else {
               index = fusion_vector_index_of( &allocation->read_tasks, this );
               if (index >= 0)
                    fusion_vector_remove( &allocation->read_tasks, index );
          }

          allocation->task_count--;

          dfb_surface_allocation_unref( allocation );
     }

     accesses.clear();
}

}

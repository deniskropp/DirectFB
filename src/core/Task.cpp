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

#include "Task.h"
#include "Util.h"

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/surface_allocation.h>

#include <misc/conf.h>
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

void
SurfaceTask_Done( SurfaceTask *task )
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     task->Done();
}

}

/*********************************************************************************************************************/

static inline const char *
state_name( TaskState state )
{
     switch (state) {
          case TASK_NEW:
               return "NEW";

          case TASK_FLUSHED:
               return "FLUSHED";

          case TASK_READY:
               return "READY";

          case TASK_RUNNING:
               return "RUNNING";

          case TASK_DONE:
               return "DONE";
     }

     return "invalid";
}

/*********************************************************************************************************************/

Task::Task()
     :
     state( TASK_NEW ),
     flags( TASK_FLAG_NONE ),
     block_count(0),
     slaves( 0 ),
     master( NULL ),
     next_slave( NULL ),
     finished( false )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );
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
Task::emit( bool following )
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

     if (flags & TASK_FLAG_EMITNOTIFIES) {
          notifyAll();
     }
     else if (following && !slaves) {
          std::vector<TaskNotify>::iterator it = notifies.begin();
     
          while (it != notifies.end()) {
               if ((*it).second) {
                    (*it).first->handleNotify( false );
     
                    it = notifies.erase( it );
               }
               else
                    ++it;
          }
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

          //direct_mutex_lock( &TaskManager::lock );

          D_SYNC_ADD( &TaskManager::task_count, -1 );
          //TaskManager::task_count--;

          if (!(shutdown->flags & TASK_FLAG_NOSYNC))
               D_SYNC_ADD( &TaskManager::task_count_sync, -1 );
               //TaskManager::task_count_sync--;

          TaskManager::tasks.remove( shutdown );

          //direct_mutex_unlock( &TaskManager::lock );

          delete shutdown;
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

std::string
Task::Describe()
{
     return Util::PrintF( "0x%08lx   %-7s  0x%04x   %2d   %2d   %2d   %s   %s",
                          (unsigned long) this, state_name(state), flags, notifies.size(), block_count,
                          slaves, master ? "><" : "  ", finished ? "YES" : "no" );
}

void
Task::addNotify( Task *task,
                 bool  follow )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     if (task == this) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, this is myself!\n" );
          D_ASSERT( state == TASK_FLUSHED );
          return;
     }

     D_ASSERT( state != TASK_NEW );
     D_ASSERT( state != TASK_FLUSHED );

     if (follow && !slaves && (state == TASK_RUNNING || state == TASK_DONE)) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, following running task!\n" );

          return;
     }

     if (state == TASK_RUNNING && (flags & TASK_FLAG_EMITNOTIFIES)) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, running task notified on emit!\n" );

          return;
     }

     notifies.push_back( TaskNotify( task, follow ) );

     task->block_count++;
}

void
Task::notifyAll()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_DONE || (state == TASK_RUNNING && (flags & TASK_FLAG_EMITNOTIFIES)) );

     for (std::vector<TaskNotify>::const_iterator it = notifies.begin(); it != notifies.end(); ++it)
          (*it).first->handleNotify( true );

     notifies.clear();
}

void
Task::handleNotify( bool following )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_READY );
     D_ASSERT( block_count > 0 );

     if (--block_count == 0) {
          long long t1, t2;

          t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          emit( following );

          t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          if (t2 - t1 >= 2000)
               ;//D_WARN( "Task::Emit took more than 5ms (%lld)", (t2 - t1) / 1000 );
     }
}

/*********************************************************************************************************************/

DirectThread     *TaskManager::thread;
FIFO<Task*>       TaskManager::fifo;
unsigned int      TaskManager::task_count;
unsigned int      TaskManager::task_count_sync;
std::list<Task*>  TaskManager::tasks;
DirectMutex       TaskManager::lock;


DFBResult
TaskManager::Initialise()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     D_ASSERT( thread == NULL );

     direct_mutex_init( &lock );

     if (dfb_config->task_manager)
          thread = direct_thread_create( DTT_CRITICAL, managerLoop, NULL, "Task Manager" );

     return DFB_OK;
}

void
TaskManager::Shutdown()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     if (thread != NULL) {
          // FIXME: wakeup managerLoop
          fifo.push( NULL );

          direct_thread_join( thread );
          direct_thread_destroy( thread );

          thread = NULL;
     }

     direct_mutex_deinit( &lock );
}

void
TaskManager::Sync()
{
     int timeout = 20000;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );
//direct_trace_print_stack(NULL);

     // FIXME: this is a hack, will avoid Sync() at all
     while (task_count_sync) {
          if (!--timeout) {
               D_ERROR( "TaskManager: Timeout while syncing (task count %d, nosync %d, tasks %d)!\n", task_count_sync, task_count, tasks.size() );
               dumpTasks();
               return;
          }

          usleep( 1000 );
     }
}

void
TaskManager::pushTask( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s( %p )\n", __FUNCTION__, task );

     if (task->state == TASK_FLUSHED) {
          //direct_mutex_lock( &lock );

          D_SYNC_ADD( &task_count, 1 );
          //task_count++;

          if (!(task->flags & TASK_FLAG_NOSYNC))
               D_SYNC_ADD( &task_count_sync, 1 );
               //task_count_sync++;

          tasks.push_back( task );

          //direct_mutex_unlock( &lock );
     }

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
               if (t2 - t1 >= 2000)
                    ;//D_WARN( "Task::Setup took more than 5ms (%lld)", (t2 - t1) / 1000 );


               if (task->block_count == 0) {
                    t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

                    ret = task->emit( true );
                    if (ret) {
                         D_DERROR( ret, "DirectFB/TaskManager: Task::Emit() failed!\n" );
                         task->state = TASK_DONE;
                         goto finish;
                    }

                    t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
                    if (t2 - t1 >= 2000)
                         ;//D_WARN( "Task::Emit took more than 5ms (%lld)", (t2 - t1) / 1000 );
               }
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

          if (!task) {
               D_DEBUG_AT( DirectFB_Task, "  =-> SHUTDOWN\n" );
               return NULL;
          }

          D_DEBUG_AT( DirectFB_Task, "  =-> Task %p\n", task );

          TaskManager::handleTask( task );
     }

     return NULL;
}

void
TaskManager::dumpTasks()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     direct_log_printf( NULL, "task       | state   | flags | no | bl | sl | is | finished\n" );

     for (std::list<Task*>::const_iterator it = tasks.begin(); it != tasks.end(); it++) {
          Task *task = *it;

          direct_log_printf( NULL, "%s\n", task->Describe().c_str() );

          for (std::vector<TaskNotify>::const_iterator it = task->notifies.begin(); it != task->notifies.end(); it++) {
               direct_log_printf( NULL, "                       %p\n", (*it).first );
          }
     }
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

//     printf("adding..., count %d\n", allocation->task_count);
     D_SYNC_ADD( &allocation->task_count, 1 );
//     printf("%p added, count %d\n", allocation, allocation->task_count);

     return DFB_OK;
}

DFBResult
SurfaceTask::Setup()
{
     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     D_ASSERT( state == TASK_FLUSHED );

     for (std::vector<SurfaceAllocationAccess>::const_iterator it = accesses.begin(); it != accesses.end(); ++it) {
          CoreSurfaceAllocation   *allocation = (*it).first;
          CoreSurfaceAccessFlags   flags      = (*it).second;

          // TODO: implement cache invalidate / flush tagging

          D_DEBUG_AT( DirectFB_Task, "  -> allocation %p, task count %d\n", allocation, allocation->task_count );

//          if (allocation->task_count > 3) {
//               D_DEBUG_AT( DirectFB_Task, "  -> post poning task\n" );
//               return DFB_BUSY;
//          }

//          allocation->task_count++;

          if ((flags & CSAF_WRITE) != 0) {
               SurfaceTask *read_task;

               D_DEBUG_AT( DirectFB_Task, "  -> WRITE\n" );

               if (allocation->read_tasks.count) {
                    int index;

                    fusion_vector_foreach (read_task, index, allocation->read_tasks) {
                         read_task->addNotify( this, read_task->accessor == accessor );
                    }

                    // FIXME: reset vector (free array)?
                    D_ASSUME( allocation->read_tasks.count < 10 );
                    allocation->read_tasks.count = 0;
               }
               else if (allocation->write_task) {
                    SurfaceTask *write_task = (SurfaceTask *)allocation->write_task;

                    write_task->addNotify( this, write_task->accessor == accessor );
               }

               allocation->write_task = this;
          }
          else {
               D_DEBUG_AT( DirectFB_Task, "  -> READ\n" );

               if (allocation->write_task) {
                    SurfaceTask *write_task = (SurfaceTask *)allocation->write_task;

                    write_task->addNotify( this, write_task->accessor == accessor );
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

               //D_ASSERT( fusion_vector_index_of( &allocation->read_tasks, this ) < 0 );
          }
          else {
               index = fusion_vector_index_of( &allocation->read_tasks, this );
               if (index >= 0)
                    fusion_vector_remove( &allocation->read_tasks, index );
          }

//          allocation->task_count--;
          D_SYNC_ADD( &allocation->task_count, -1 );
//          printf("%p subbed, count %d\n", allocation, allocation->task_count);

          dfb_surface_allocation_unref( allocation );
     }

     accesses.clear();
}

std::string
SurfaceTask::Describe()
{
     return Task::Describe() + Util::PrintF( "  accessor 0x%02x, accesses %d", accessor, accesses.size() );
}

}

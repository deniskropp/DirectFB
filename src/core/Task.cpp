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

#include "Task.h"
#include "Util.h"

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

#include "Task.h"

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

void
TaskManager_SyncAll()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     TaskManager::SyncAll();
}

/*********************************************************************************************************************/

DFB_TaskList *
TaskList_New( bool locked )
{
     if (locked)
          return new Direct::ListLocked<DirectFB::Task*>;

     return new Direct::ListSimple<DirectFB::Task*>;
}

bool
TaskList_IsEmpty( DFB_TaskList *list )
{
     return list->Length() == 0;
}

DFBResult
TaskList_WaitEmpty( DFB_TaskList *list )
{
     DFB_TaskListLocked *locked = dynamic_cast<DFB_TaskListLocked*>( list );

     locked->WaitEmpty();

     return DFB_OK;
}

void
TaskList_Delete( DFB_TaskList *list )
{
     D_ASSERT( list != NULL );
     D_ASSUME( list->Length() == 0 );

     delete list;
}

/*********************************************************************************************************************/

void
Task_AddRef( DFB_Task *task )
{
     task->AddRef();
}

void
Task_Release( DFB_Task *task )
{
     task->Release();
}

void
Task_AddNotify( Task *task,
                Task *notified,
                bool  follow )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p->%p, %sfollow )\n", __FUNCTION__, task, notified, follow ? "" : "NO " );

     task->AddNotify( notified, follow );
}

void
Task_Flush( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, task );

     task->Flush();
}

void
Task_Done( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, task );

     task->Done();
}

void
Task_DoneFail( Task      *task,
               DFBResult  result )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, task );

     D_ASSUME( result != DFB_OK );

     task->Done( result );
}

void
Task_Log( Task       *task,
          const char *action )
{
#if DFB_TASK_DEBUG_LOG
     task->Log( action );
#endif
}

/*********************************************************************************************************************/

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

/*********************************************************************************************************************/

DFBResult
DisplayTask_Generate( CoreLayerRegion      *region,
                      const DFBRegion      *left_update,
                      const DFBRegion      *right_update,
                      DFBSurfaceFlipFlags   flags,
                      long long             pts,
                      DisplayTask         **ret_task )
{
     D_DEBUG_AT( DirectFB_Task, "%s( region %p, "DFB_RECT_FORMAT", "DFB_RECT_FORMAT", flags 0x%04x, ret_task %p )\n", __FUNCTION__,
                 region, DFB_RECTANGLE_VALS_FROM_REGION( left_update ), DFB_RECTANGLE_VALS_FROM_REGION( right_update ), flags, ret_task );

     return DisplayTask::Generate( region, left_update, right_update, flags, pts, ret_task );
}

long long
DisplayTask_GetPTS( DFB_DisplayTask *task )
{
     long long pts;

     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, task );

     pts = task->GetPTS();

     D_DEBUG_AT( DirectFB_Task, "  -> %lld\n", pts );

     return pts;
}

/*********************************************************************************************************************/

class SimpleTask : public Task
{
public:
     SimpleTask( SimpleTaskFunc *push,
                 SimpleTaskFunc *run,
                 void           *ctx )
          :
          push( push ),
          run( run ),
          ctx( ctx )
     {
     }

protected:
     virtual DFBResult Push()
     {
          if (push)
               return push( ctx, this );

          return Task::Push();
     }

     virtual DFBResult Run()
     {
          if (run)
               return run( ctx, this );

          return Task::Run();
     }

private:
     SimpleTaskFunc *push;
     SimpleTaskFunc *run;
     void           *ctx;
};

DFBResult
SimpleTask_Create( SimpleTaskFunc  *push,
                   SimpleTaskFunc  *run,
                   void            *ctx,
                   DFB_Task       **ret_task )
{
     Task *task;

     D_DEBUG_AT( DirectFB_Task, "%s( push %p, run %p, ctx %p, ret_task %p )\n", __FUNCTION__, push, run, ctx, ret_task );

     task = new SimpleTask( push, run, ctx );

     if (ret_task)
          *ret_task = task;
     else
          task->Flush();

     return DFB_OK;
}

}

/*********************************************************************************************************************/

Task::Task()
     :
     magic( D_MAGIC("Task") ),
     state( TASK_NEW ),
     flags( TASK_FLAG_NONE ),
     refs( 1 ),
     block_count( 0 ),
     slaves( 0 ),
     master( NULL ),
     next_slave( NULL ),
     qid( 0 ),
     next( NULL ),
     hwid( 0 ),
     listed( 0 ),
     finished( false ),
     dump( false )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s()\n", __FUNCTION__ );

     DFB_TASK_LOG( "Task()" );

     D_DEBUG_AT( DirectFB_Task, "  <- %p\n", this );
}

void
Task::AddRef()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread)
          DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return );
     else
          DFB_TASK_CHECK_STATE( this, TASK_NEW, return );
#endif

     refs++;
}

void
Task::Release()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread) {
          DFB_TASK_CHECK_STATE( this, TASK_DONE, return );
     }
     else {
          if (refs == 1)
               DFB_TASK_CHECK_STATE( this, TASK_NEW, return );
          else
               DFB_TASK_CHECK_STATE( this, TASK_NEW | TASK_RUNNING, return );
     }
#endif

     if (! --refs)
          delete this;
}

Task::~Task()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread)
          DFB_TASK_CHECK_STATE( this, TASK_DONE, );
     else
          DFB_TASK_CHECK_STATE( this, TASK_NEW, );
#endif

     if (dump)
          DumpLog( DirectFB_Task, DIRECT_LOG_VERBOSE );

     state = TASK_INVALID;

     D_MAGIC_CLEAR( this );
}

void
Task::AddSlave( Task *slave )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, slave %p )\n", __FUNCTION__, this, slave );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW | TASK_RUNNING, );
     DFB_TASK_CHECK_STATE( slave, TASK_NEW, );

     D_ASSERT( slave->master == NULL );

     slave->master = this;

     slaves++;

     if (next_slave)
          slave->next_slave = next_slave;

     next_slave = slave;
}


void
Task::AddToList( DFB_TaskList *list )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, );

     DFB_TASK_LOG( Direct::String::F( "AddToList(%p)", list ) );

     D_ASSERT( list != NULL );

     listed++;

     list->Append( this );
}

void
Task::RemoveFromList( DFB_TaskList *list )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, );

     DFB_TASK_LOG( Direct::String::F( "AddToList(%p)", list ) );

     D_ASSERT( list != NULL );

     list->Remove( this );

     listed--;
}


void
Task::Flush()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, );

     DFB_TASK_LOG( "Flush()" );

     //D_INFO("XX FLUSH   %s\n",Description().buffer());

     state = TASK_FLUSHED;

     TaskManager::pushTask( this );
}

DFBResult
Task::emit( int following )
{
     DFBResult ret;

     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_MAGIC_ASSERT( this, Task );

#if D_DEBUG_ENABLED
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, %sfollowing ) <- [%s]\n", __FUNCTION__, this, following ? "" : "NOT ", Description().buffer() );
#endif

     DFB_TASK_CHECK_STATE( this, TASK_READY, return DFB_BUG );

     DFB_TASK_LOG( "emit()" );

     //D_INFO("XX EMIT    %s\n",Description().buffer());

     D_ASSERT( block_count == 0 );

     state = TASK_RUNNING;

     ret = Push();
     switch (ret) {
          case DFB_BUSY:
               D_ASSERT( block_count > 0 );
               state = TASK_READY;
               return DFB_OK;

          case DFB_OK:
               break;

          default:
               // FIXME: error handling
               D_BREAK( "Push() error" );
     }

     if (flags & TASK_FLAG_NEED_SLAVE_PUSH) {
          Task *slave = next_slave;

          while (slave) {
               DFB_TASK_CHECK_STATE( slave, TASK_NEW, );

               slave->state = TASK_RUNNING;

               ret = slave->Push();
               switch (ret) {
                    case DFB_BUSY:
                         D_ASSERT( slave->block_count > 0 );
                         slave->state = TASK_READY;
                         break;

                    case DFB_OK:
                         break;

                    default:
                         // FIXME: error handling
                         D_BREAK( "slave Push() error" );
               }

               slave = slave->next_slave;
          }
     }

     if (flags & TASK_FLAG_EMITNOTIFIES) {
          notifyAll();
     }
     else if (following) {
          std::vector<TaskNotify>::iterator it = notifies.begin();

          while (it != notifies.end()) {
               DFB_TASK_CHECK_STATE( (*it).first, TASK_READY, );

               if ((*it).second) {
                    (*it).first->handleNotify( following - 1 );

                    it = notifies.erase( it );
               }
               else
                    ++it;
          }
     }
     else {
          std::vector<TaskNotify>::iterator it = notifies.begin();

          while (it != notifies.end()) {
               DFB_TASK_CHECK_STATE( (*it).first, TASK_READY, );

               ++it;
          }
     }

     return DFB_OK;
}

DFBResult
Task::finish()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_MAGIC_ASSERT( this, Task );

     Task *shutdown = NULL;

#if D_DEBUG_ENABLED
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p ) <- [%s]\n", __FUNCTION__, this, Description().buffer() );
#endif

     DFB_TASK_CHECK_STATE( this, TASK_DONE, return DFB_BUG );

     DFB_TASK_LOG( "finish()" );

     //D_INFO("XX FINISH  %s\n",Description().buffer());

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

               slave->Finalise();
               slave->Release();
          }


          D_SYNC_ADD( &TaskManager::task_count, -1 );

          if (!(shutdown->flags & TASK_FLAG_NOSYNC))
               D_SYNC_ADD( &TaskManager::task_count_sync, -1 );


#if DFB_TASK_DEBUG_TASKS
          direct_mutex_lock( &TaskManager::tasks_lock );
          TaskManager::tasks.remove( shutdown );
          direct_mutex_unlock( &TaskManager::tasks_lock );
#endif

          shutdown->Release();
     }

     return DFB_OK;
}

void
Task::Done( DFBResult ret )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     DFB_TASK_ASSERT( this, );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING, );

     DFB_TASK_LOG( "Done()" );

     state = TASK_DONE;

     TaskManager::pushTask( this );
//     TaskManager::fifo.pushFront( this );
}


DFBResult
Task::Setup()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return DFB_BUG );

     DFB_TASK_LOG( "Setup()" );

     state = TASK_READY;

     return DFB_OK;
}

DFBResult
Task::Push()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING, return DFB_BUG );

     DFB_TASK_LOG( "Push()" );

//     return Run();

     static TaskThreads *threads;

     if (!threads)
          threads = new TaskThreads( "Task", 4 );

     threads->Push( this );

     return DFB_OK;
}

DFBResult
Task::Run()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING, return DFB_BUG );

     DFB_TASK_LOG( "Run()" );

     Done();

     return DFB_OK;
}

void
Task::Finalise()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_DONE, );

     DFB_TASK_LOG( "Finalise()" );
}

void
Task::Describe( Direct::String &string )
{
     string.PrintF( "0x%08lx   %-7s  0x%04x   %2zu   %2d   %2d   %s   %s  [%llx]",
                    (unsigned long) this, dfb_task_state_name(state), flags, notifies.size(), block_count,
                    slaves, master ? "><" : "  ", finished ? "YES" : "no ", (unsigned long long) qid );
}

void
Task::AddNotify( Task *notified,
                 bool  follow )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, notified %p, %sfollow )\n", __FUNCTION__, this, notified, follow ? "" : "NO " );

     D_MAGIC_ASSERT( this, Task );

     if (notified == this) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, this is myself!\n" );
          DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, );
          return;
     }

     D_MAGIC_ASSERT( notified, Task );

     DFB_TASK_CHECK_STATE( this, ~TASK_FLUSHED, return );

     /* May only call addNotify from outside TaskManager thread when task wasn't flushed to manager yet */
#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() != TaskManager::thread) {
          DFB_TASK_CHECK_STATE( this, TASK_NEW, return );
          DFB_TASK_CHECK_STATE( notified, TASK_NEW, return );
     }
#endif

     DFB_TASK_LOG( Direct::String::F( "AddNotify( %p, %sfollow )", notified, follow ? "" : "NO " ) );

     if (follow /*&& !slaves*/ && (state == TASK_RUNNING || state == TASK_DONE)) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, following running task!\n" );

          return;
     }

     if (state == TASK_RUNNING && (flags & TASK_FLAG_EMITNOTIFIES)) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, running task notified on emit!\n" );

          return;
     }

     notifies.push_back( TaskNotify( notified, follow ) );

     notified->block_count++;

     D_DEBUG_AT( DirectFB_Task, "Task::%s() done\n", __FUNCTION__ );
}

void
Task::notifyAll()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     if (flags & TASK_FLAG_EMITNOTIFIES)
          DFB_TASK_CHECK_STATE( this, TASK_DONE | TASK_RUNNING, return );
     else
          DFB_TASK_CHECK_STATE( this, TASK_DONE, return );

     DFB_TASK_LOG( Direct::String::F( "notifyAll(%zu)", notifies.size() ) );

     for (std::vector<TaskNotify>::const_iterator it = notifies.begin(); it != notifies.end(); ++it)
          (*it).first->handleNotify( 1 );

     notifies.clear();


}

void
Task::handleNotify( int following )
{
     DFBResult ret;

     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, %sfollowing )\n", __FUNCTION__, this, following ? "" : "NOT " );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_READY, return );

     DFB_TASK_LOG( Direct::String::F( "handleNotify( %sfollowing )", following ? "" : "NOT " ) );

     D_ASSERT( block_count > 0 );

     if (--block_count == 0) {
#if DFB_TASK_DEBUG_TIMES
          long long t1, t2;

          t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

          ret = emit( following );
          if (ret) {
               D_DERROR( ret, "DirectFB/TaskManager: Task::Emit() failed!\n" );
               state = TASK_DONE;
               enableDump();
               TaskManager::pushTask( this );
          }

#if DFB_TASK_DEBUG_TIMES
          t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          if (t2 - t1 > DFB_TASK_WARN_EMIT) {
               D_WARN( "Task::Emit took more than %dus (%lld) [%s]", DFB_TASK_WARN_EMIT, t2 - t1, Description().buffer() );
               enableDump();
          }
#endif
     }
}

void
Task::enableDump()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_STATE_ALL & ~TASK_INVALID, return );

     dump = true;
}

void
Task::append( Task *task )
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, %p )\n", __FUNCTION__, this, task );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING | TASK_DONE, return );
     DFB_TASK_CHECK_STATE( task, TASK_RUNNING, return );

     DFB_TASK_LOG( Direct::String::F( "append( %p )", task ) );

     D_ASSERT( next == NULL );

     next = task;
}

void
Task::Log( const std::string &action )
{
#if DFB_TASK_DEBUG_LOG
     const char *name = direct_thread_self_name();

     LogEntry entry;

     entry.thread = name ? name : "  NO NAME  ";
     entry.action = action;
     entry.micros = direct_clock_get_micros();
     entry.trace  = direct_trace_copy_buffer( NULL );


     Direct::Mutex::Lock lock( tasklog_lock );

     tasklog.push_back( entry );
#endif
}

void
Task::DumpLog( DirectLogDomain &domain, DirectLogLevel level )
{
#if DFB_TASK_DEBUG_LOG
     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_STATE_ALL & ~TASK_INVALID, return );

     Direct::Mutex::Lock lock( tasklog_lock );

     direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                            "==[ TASK DUMP for %p, state %d, flags 0x%x, log size %zu ]\n", this, state, flags, tasklog.size() );

     direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                            "  [ %s ]\n", Description().buffer() );

     for (std::vector<LogEntry>::const_iterator it=tasklog.begin(); it!=tasklog.end(); it++) {
          direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                                 "  [%-16.16s %3lld.%03lld,%03lld]  %-30s\n",
                                 (*it).thread.c_str(),
                                 (*it).micros / 1000000LL,
                                 ((*it).micros / 1000LL) % 1000LL,
                                 (*it).micros % 1000LL,
                                 (*it).action.c_str() );

          if ((*it).trace)
               direct_trace_print_stack( (*it).trace );
     }
#endif
}

Direct::String &
Task::Description()
{
     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_STATE_ALL & ~TASK_INVALID, );

     description.Clear();

     Describe( description );

     return description;
}

/*********************************************************************************************************************/

DirectThread     *TaskManager::thread;
FIFO<Task*>       TaskManager::fifo;
unsigned int      TaskManager::task_count;
unsigned int      TaskManager::task_count_sync;
#if DFB_TASK_DEBUG_TASKS
std::list<Task*>  TaskManager::tasks;
DirectMutex       TaskManager::tasks_lock;
#endif


DFBResult
TaskManager::Initialise()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     D_ASSERT( thread == NULL );

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_init( &tasks_lock );
#endif

     if (dfb_config->task_manager)
          thread = direct_thread_create( DTT_CRITICAL, managerLoop, NULL, "Task Manager" );

     return DFB_OK;
}

void
TaskManager::Shutdown()
{
     D_ASSERT( direct_thread_self() != TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     if (thread != NULL) {
          fifo.push( NULL );

          direct_thread_join( thread );
          direct_thread_destroy( thread );

          thread = NULL;
     }

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_deinit( &tasks_lock );
#endif
}

void
TaskManager::Sync()
{
     D_ASSERT( direct_thread_self() != TaskManager::thread );

     int timeout = 10000;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     // FIXME: this is a hack, will avoid Sync() at all
     while (*(volatile unsigned int*)&task_count_sync) {
          if (!--timeout) {
#if DFB_TASK_DEBUG_TASKS
               D_ERROR( "TaskManager: Timeout while syncing (task count %d, nosync %d, tasks %zu)!\n", task_count_sync, task_count, tasks.size() );
#else
               D_ERROR( "TaskManager: Timeout while syncing (task count %d, nosync %d)!\n", task_count_sync, task_count );
#endif
               dumpTasks();
               return;
          }

          usleep( 1000 );
     }
}

void
TaskManager::SyncAll()
{
     D_ASSERT( direct_thread_self() != TaskManager::thread );

     int timeout = 10000;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     // FIXME: this is a hack, will avoid Sync() at all
     while (*(volatile unsigned int*)&task_count) {
          if (!--timeout) {
#if DFB_TASK_DEBUG_TASKS
               D_ERROR( "TaskManager: Timeout while syncing for all (task count %d, nosync %d, tasks %zu)!\n", task_count_sync, task_count, tasks.size() );
#else
               D_ERROR( "TaskManager: Timeout while syncing for all (task count %d, nosync %d)!\n", task_count_sync, task_count );
#endif
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
          D_SYNC_ADD( &task_count, 1 );

          if (!(task->flags & TASK_FLAG_NOSYNC))
               D_SYNC_ADD( &task_count_sync, 1 );

#if DFB_TASK_DEBUG_TASKS
          direct_mutex_lock( &TaskManager::tasks_lock );
          TaskManager::tasks.push_back( task );
          direct_mutex_unlock( &TaskManager::tasks_lock );
#endif
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
#if DFB_TASK_DEBUG_TIMES
     long long t1, t2;
#endif

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s( %p )\n", __FUNCTION__, task );

     switch (task->state) {
          case TASK_FLUSHED:
               D_DEBUG_AT( DirectFB_Task, "  -> FLUSHED\n" );

#if DFB_TASK_DEBUG_TIMES
               t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

               ret = task->Setup();
               if (ret) {
                    D_DERROR( ret, "DirectFB/TaskManager: Task::Setup() failed!\n" );
                    task->state = TASK_DONE;
                    task->enableDump();
                    goto finish;
               }

#if DFB_TASK_DEBUG_TIMES
               t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
               if (t2 - t1 > DFB_TASK_WARN_SETUP) {
                    D_WARN( "Task::Setup took more than %dus (%lld)  [%s]", DFB_TASK_WARN_SETUP, t2 - t1, task->Description().buffer() );
                    task->enableDump();
               }
#endif


               if (task->block_count == 0) {
#if DFB_TASK_DEBUG_TIMES
                    t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

                    ret = task->emit( 1 );
                    if (ret) {
                         D_DERROR( ret, "DirectFB/TaskManager: Task::Emit() failed!\n" );
                         task->state = TASK_DONE;
                         task->enableDump();
                         goto finish;
                    }

#if DFB_TASK_DEBUG_TIMES
                    t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
                    if (t2 - t1 > DFB_TASK_WARN_EMIT) {
                         D_WARN( "Task::Emit took more than %dus (%lld)  [%s]", DFB_TASK_WARN_EMIT, t2 - t1, task->Description().buffer() );
                         task->enableDump();
                    }
#endif
               }
               break;

          case TASK_DONE:
               D_DEBUG_AT( DirectFB_Task, "  -> DONE\n" );

finish:
#if DFB_TASK_DEBUG_TIMES
               {
               std::string desc = task->Description();

               t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

               ret = task->finish();
               if (ret) {
                    D_DERROR( ret, "DirectFB/TaskManager: Task::finish() failed, leaking memory!\n" );
                    task->DumpLog( DirectFB_Task, DIRECT_LOG_VERBOSE );
                    task->state = TASK_INVALID;
                    break;
               }

#if DFB_TASK_DEBUG_TIMES
               t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
               if (t2 - t1 > DFB_TASK_WARN_FINISH)
                    D_WARN( "Task::finish took more than %dus (%lld)  [%p] %s", DFB_TASK_WARN_FINISH, t2 - t1, task, desc.c_str() );
               }
#endif
               break;

          case TASK_INVALID:
               D_BUG( "invalid task state %d (task %p)", task->state, task );
               task->DumpLog( DirectFB_Task, DIRECT_LOG_VERBOSE );
               break;

          default:
               D_BUG( "invalid task state %d (task %p)", task->state, task );
     }

     return DFB_OK;
}

void *
TaskManager::managerLoop( DirectThread *thread,
                          void         *arg )
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     fusion_config->skirmish_warn_on_thread = direct_thread_get_tid( thread );

     while (true) {
          Task *task = TaskManager::pullTask();

          if (!task) {
               D_DEBUG_AT( DirectFB_Task, "  =-> SHUTDOWN\n" );
               return NULL;
          }

          D_DEBUG_AT( DirectFB_Task, "  =-> pulled a Task [%s]\n", task->Description().buffer() );

          TaskManager::handleTask( task );
     }

     return NULL;
}

void
TaskManager::dumpTasks()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_lock( &TaskManager::tasks_lock );

     direct_log_printf( NULL, "task       | state   | flags | no | bl | sl | is | finished\n" );

     for (std::list<Task*>::const_iterator it = tasks.begin(); it != tasks.end(); it++) {
          Task *task = *it;

          direct_log_printf( NULL, "%s\n", task->Description().buffer() );

          for (std::vector<TaskNotify>::const_iterator it = task->notifies.begin(); it != task->notifies.end(); it++)
               direct_log_printf( NULL, "   ->  %p\n", (*it).first );

          task->DumpLog( DirectFB_Task, DIRECT_LOG_VERBOSE );

          while ((task = task->next_slave) != NULL)
               direct_log_printf( NULL, "%s\n", task->Description().buffer() );

          direct_log_printf( NULL, "\n" );
     }

     direct_mutex_unlock( &TaskManager::tasks_lock );
#endif
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
SurfaceTask::Setup()
{
     DFB_TASK_LOG( "SurfaceTask::Setup()" );

     D_DEBUG_AT( DirectFB_Task, "SurfaceTask::%s()\n", __FUNCTION__ );

     DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return DFB_BUG );

     for (std::vector<SurfaceAllocationAccess>::iterator it = accesses.begin(); it != accesses.end(); ++it) {
          SurfaceAllocationAccess &access = *it;

          if (!access.allocation->read_tasks)
               access.allocation->read_tasks = new DFB_SurfaceTaskListSimple;

          DFB_SurfaceTaskListSimple &read_tasks = *access.allocation->read_tasks;

          D_DEBUG_AT( DirectFB_Task, "  -> allocation %p, task count %d\n", access.allocation, access.allocation->task_count );

          /* set invalidate flag in case this accessor has not yet invalidated its cache for this allocation */
          if (!(access.allocation->invalidated & (1 << accessor))) {
               D_FLAGS_SET( access.flags, CSAF_CACHE_INVALIDATE );

               /* set this accessor's invalidated flag */
               access.allocation->invalidated |= (1 << accessor);
          }

          if (D_FLAGS_IS_SET( access.flags, CSAF_WRITE )) {
               D_DEBUG_AT( DirectFB_Task, "  -> WRITE\n" );

               /* clear all accessors' invalidated flag except our own */
               access.allocation->invalidated &= (1 << accessor);
               D_ASSUME( access.allocation->invalidated & (1 << accessor) );

               if (read_tasks.Length()) {
                    for (DFB_SurfaceTaskListSimple::const_iterator it=read_tasks.begin(); it != read_tasks.end(); it++)
                         (*it)->AddNotify( this, (*it)->accessor == accessor && (*it)->qid == qid );

                    read_tasks.Clear();
               }
               else if (access.allocation->write_task) {
                    SurfaceTask *write_task = access.allocation->write_task;

                    D_ASSERT( access.allocation->write_access != NULL );

                    /* if the last write task still exists from same accessor (ready/running), clear its
                       flush flags, hoping the task implementation can avoid the flush (still) */
                    if (write_task->accessor == accessor)
                         D_FLAGS_CLEAR( ((SurfaceAllocationAccess *)access.allocation->write_access)->flags, CSAF_CACHE_FLUSH );

                    write_task->AddNotify( this, write_task->accessor == accessor && write_task->qid == qid );
               }
               else
                    D_ASSERT( access.allocation->write_access == NULL );

               /* set flush flag per default, will be cleared when another task from same accessor is following */
               D_FLAGS_SET( access.flags, CSAF_CACHE_FLUSH );

               access.allocation->write_task   = this;
               access.allocation->write_access = &access;
          }
          else {
               D_DEBUG_AT( DirectFB_Task, "  -> READ\n" );

               if (access.allocation->write_task) {
                    SurfaceTask *write_task = access.allocation->write_task;

                    D_ASSERT( access.allocation->write_access != NULL );

                    // TODO: avoid cache flush in write task if accessor equals,
                    // requires special handling to take care about other read tasks
                    // and to carry on the flush flag to the read task

                    write_task->AddNotify( this, write_task->accessor == accessor && write_task->qid == qid );
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
SurfaceTask::Describe( Direct::String &string )
{
     Task::Describe( string );

     string.PrintF( "  accessor 0x%02x, accesses %zu, allocation %p, index %d", accessor, accesses.size(),
                    accesses.size() > 0 ? accesses[0].allocation : NULL,
                    accesses.size() > 0 ? accesses[0].allocation->index : -1 );
}

}

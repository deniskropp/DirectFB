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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"
#include <directfb_util.h>

#include <direct/Types++.h>


extern "C" {
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
#include <core/TaskManager.h>
#include <core/Util.h>

/*********************************************************************************************************************/

namespace DirectFB {


extern "C" {

DFB_TaskList *
TaskList_New( bool locked )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %slock )\n", __FUNCTION__, locked ? "no " : "" );

     if (locked)
          return new Direct::ListLocked<DirectFB::Task*>;

     DFB_TaskList *list = new Direct::ListSimple<DirectFB::Task*>;

     D_DEBUG_AT( DirectFB_Task, "  <- %p\n", list );

     return list;
}

bool
TaskList_IsEmpty( DFB_TaskList *list )
{
     return list->Length() == 0;
}

DFBResult
TaskList_WaitEmpty( DFB_TaskList *list )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, list );

     DFB_TaskListLocked *locked = dynamic_cast<DFB_TaskListLocked*>( list );

     locked->WaitEmpty();

     D_DEBUG_AT( DirectFB_Task, "%s( %p ) done.\n", __FUNCTION__, list );

     return DFB_OK;
}

void
TaskList_Delete( DFB_TaskList *list )
{
     D_DEBUG_AT( DirectFB_Task, "%s( %p )\n", __FUNCTION__, list );

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

//     D_ASSUME( result != DFB_OK );

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
     ts_emit( 0 ),
     dump( false )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     DFB_TASK_LOG( "Task()" );

#if DFB_TASK_DEBUG_TIMING
     ts_flushed = 0;
     ts_ready   = 0;
     ts_running = 0;
     ts_done    = 0;
#endif

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_lock( &TaskManager::tasks_lock );
     TaskManager::tasks.push_back( this );
     direct_mutex_unlock( &TaskManager::tasks_lock );
#endif
}

Task::~Task()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread)
          DFB_TASK_CHECK_STATE( this, TASK_DEAD, );
     else
          DFB_TASK_CHECK_STATE( this, TASK_NEW, );
#endif

#if DFB_TASK_DEBUG_TIMING
     long long flushed_to_ready = direct_config_get_int_value( "task-dump-on-flushed-to-ready" );
     long long ready_to_running = direct_config_get_int_value( "task-dump-on-ready-to-running" );
     long long running_to_done  = direct_config_get_int_value( "task-dump-on-running-to-done" );
     if (flushed_to_ready && ts_ready - ts_flushed >= flushed_to_ready)
          dump = true;
     if (ready_to_running && ts_running - ts_ready >= ready_to_running)
          dump = true;
     if (running_to_done && ts_done - ts_running >= running_to_done)
          dump = true;
#endif

     if (notifies.size() != 0) {
          D_BUG( "notifies list not empty (%zu entries)", notifies.size() );
          dump = true;
     }

     if (dump)
          DumpLog( DirectFB_Task, DIRECT_LOG_VERBOSE );

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_lock( &TaskManager::tasks_lock );
     TaskManager::tasks.remove( this );
     direct_mutex_unlock( &TaskManager::tasks_lock );
#endif

     state = TASK_INVALID;

     D_ASSERT( notifies.size() == 0 );

     D_MAGIC_CLEAR( this );
}

void
Task::AddRef()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p ) <- refs:%u\n", __FUNCTION__, this, refs );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread)
          DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return );
     else
          DFB_TASK_CHECK_STATE( this, TASK_NEW | TASK_RUNNING, return );
#endif

     D_ASSERT( refs > 0 );

     unsigned int refs_now = D_SYNC_ADD_AND_FETCH( &refs, 1 );

     (void) refs_now;

     D_ASSERT( refs_now > 1 );

     D_DEBUG_AT( DirectFB_Task, "  -> %u refs now\n", refs_now );
}

void
Task::Release()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p ) <- refs:%u\n", __FUNCTION__, this, refs );

#if DFB_TASK_DEBUG_STATE
     if (direct_thread_self() == TaskManager::thread) {
          DFB_TASK_CHECK_STATE( this, TASK_RUNNING | TASK_FINISH, return );
     }
     else {
          if (refs == 1)
               DFB_TASK_CHECK_STATE( this, TASK_NEW | TASK_FINISH, return );
          else
               DFB_TASK_CHECK_STATE( this, TASK_NEW | TASK_RUNNING | TASK_DONE | TASK_FINISH, return );
     }
#endif

     D_ASSERT( refs > 0 );

     unsigned int refs_now = D_SYNC_ADD_AND_FETCH( &refs, -1 );

     D_DEBUG_AT( DirectFB_Task, "  -> %u refs left\n", refs_now );

     if (refs_now == 0) {
          state = TASK_DEAD;

          if (direct_thread_self() == TaskManager::thread) {
               D_DEBUG_AT( DirectFB_Task, "  -> in manager thread, deleting Task\n" );
               delete this;
          }
          else {
               D_DEBUG_AT( DirectFB_Task, "  -> NOT in manager thread, pushing Task\n" );
               TaskManager::pushTask( this );
          }
     }
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
Task::Flush()
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, );

     DFB_TASK_LOG( "Flush()" );

     state = TASK_FLUSHED;

#if DFB_TASK_DEBUG_TIMING
     ts_flushed = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

     TaskManager::pushTask( this );
}

DFBResult
Task::emit()
{
     DFBResult ret;

     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_MAGIC_ASSERT( this, Task );

#if D_DEBUG_ENABLED
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p ) <- [%s]\n", __FUNCTION__, this, Description().buffer() );
#endif

     DFB_TASK_CHECK_STATE( this, TASK_READY, return DFB_BUG );

     DFB_TASK_LOG( "emit()" );

     D_ASSERT( block_count == 0 );

     state = TASK_RUNNING;

#if DFB_TASK_DEBUG_TIMING
     ts_running = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

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

#if DFB_TASK_DEBUG_TIMING
               ts_running = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

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

     notifyAll( TASK_RUNNING );

     if (flags & TASK_FLAG_EMITNOTIFIES)
          notifyAll( TASK_FINISH );

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

     state = TASK_FINISH;

     if (master) { /* has master? */
          D_ASSERT( slaves == 0 );
          D_ASSERT( master->slaves > 0 );

          if (!--(master->slaves)) {
               if (master->state == TASK_FINISH) {
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
          shutdown->notifyAll( TASK_FINISH );
          shutdown->Finalise();

          Task *next = shutdown->next_slave;

          while (next) {
               Task *slave = next;

               next = slave->next_slave;

               slave->Finalise();  // TODO: OPTIMISE: have extra Finalize?
               slave->Release();
          }


          shutdown->Release();
     }

     return DFB_OK;
}

void
Task::Done( DFBResult ret )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, ret %u )\n", __FUNCTION__, this, ret );

     DFB_TASK_ASSERT( this, );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING, );

     DFB_TASK_LOG( Direct::String::F( "Done( %u )", ret ) );

     state = TASK_DONE;

#if DFB_TASK_DEBUG_TIMING
     ts_done = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

     if (ret)
          enableDump();

     TaskManager::pushTask( this );
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

#if DFB_TASK_DEBUG_TIMING
     ts_ready = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

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

     TaskManager::threads->Push( this );

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

     DFB_TASK_CHECK_STATE( this, TASK_FINISH, );

     DFB_TASK_LOG( "Finalise()" );
}

void
Task::Describe( Direct::String &string ) const
{
     string.PrintF( "0x%08lx %-12s [%-7s  %-20s] %u refs (n:%zu bl:%2d sl:%d m:%p qid:%lld hw:%u emit:%lld) : next %p",
                    (unsigned long) this,
                    *TypeName(),
                    *ToString<DirectFB::TaskState>(state),
                    *ToString<DirectFB::TaskFlags>(flags),
                    refs,
                    notifies.size(),
                    block_count,
                    slaves,
                    master,
                    (unsigned long long) qid,
                    hwid,
                    ts_emit,
                    next );
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

     if (follow && D_FLAGS_IS_SET( state, TASK_RUNNING | TASK_DONE | TASK_FINISH )) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, following running task!\n" );

          return;
     }

#if FIXME_ORDERING_VS_OPTIMISE
     if (follow && state == TASK_READY && block_count == 0) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, ready with zero block count (about to be pushed)!\n" );

          return;
     }
#endif

     if (D_FLAGS_IS_SET( state, TASK_RUNNING | TASK_DONE ) && (flags & TASK_FLAG_EMITNOTIFIES)) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, running task notified on emit!\n" );

          return;
     }

     if (D_FLAGS_IS_SET( state, TASK_FINISH ) && slaves == 0) {
          D_DEBUG_AT( DirectFB_Task, "  -> avoiding notify, done already!\n" );

          return;
     }

     notifies.push_back( TaskNotify( notified, follow ? TASK_RUNNING : TASK_FINISH ) );

     notified->block_count++;

     D_DEBUG_AT( DirectFB_Task, "Task::%s() done\n", __FUNCTION__ );
}

void
Task::notifyAll( TaskState state )
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_LOG( Direct::String::F( "notifyAll(%zu, %s)", notifies.size(), *ToString<TaskState>(state) ) );

     for (std::vector<TaskNotify>::iterator it = notifies.begin(); it != notifies.end(); ) {
          if ((*it).second & state) {
               DFB_TASK_LOG( Direct::String::F( "  notifying %p", (*it).first ) );

               (*it).first->handleNotify();

               it = notifies.erase( it );
          }
          else
               it++;
     }
}

void
Task::checkEmit()
{
     DFBResult ret;

     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_READY, return );

     if (block_count == 0 && !D_FLAGS_IS_SET( flags, TASK_FLAG_WAITING_TIMED_EMIT )) {
#if DFB_TASK_DEBUG_TIMES
          long long t1, t2;

          t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
#endif

          ret = emit();
          if (ret) {
               D_DERROR( ret, "DirectFB/TaskManager: Task::Emit() failed!\n" );
               Done( ret );
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
Task::handleNotify()
{
     D_ASSERT( direct_thread_self() == TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_READY, return );

     DFB_TASK_LOG( Direct::String::F( "handleNotify()" ) );

     D_ASSERT( block_count > 0 );

     if (--block_count == 0)
          checkEmit();
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

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING | TASK_DONE | TASK_FINISH, return );
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
     D_MAGIC_ASSUME( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_STATE_ALL & ~TASK_INVALID, return );

#if DFB_TASK_DEBUG_LOG
     Direct::Mutex::Lock lock( tasklog_lock );
#endif

     direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                            "==[ TASK DUMP for %p, state %d, flags 0x%x, log size %zu ]\n", this, state, flags,
#if DFB_TASK_DEBUG_LOG
                            tasklog.size() );
#else
                            (size_t) 0 );
#endif

#if DFB_TASK_DEBUG_TIMING
     direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                            "  [ timing: %6lld %6lld %6lld %6lld (flushed->ready->running->done->now) ]\n", ts_ready - ts_flushed, ts_running - ts_ready, ts_done - ts_running,
                            direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) - ts_done );
#endif

     direct_log_domain_log( &domain, level, __FUNCTION__, __FILE__, __LINE__,
                            "  [ %s ]\n", Description().buffer() );

#if DFB_TASK_DEBUG_LOG
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

void
Task::DumpTree( int indent, int max )
{
     D_MAGIC_ASSERT( this, Task );

     if (indent > max) {
          direct_log_printf( NULL, ".%*s[...]\n", indent, "" );
     }
     else {
          direct_log_printf( NULL, ".%*s%s\n", indent, "", *Description() );

          for (std::vector<TaskNotify>::iterator it = notifies.begin(); it != notifies.end(); it++) {
               Task *task = (*it).first;

               task->DumpTree( indent + 4, max );
          }
     }
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

void
Task::AddFlags( TaskFlags flags )
{
     D_DEBUG_AT( DirectFB_Task, "Task::%s( %p, %s )\n", __FUNCTION__, this, *ToString<TaskFlags>(flags) );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_NEW, return );

     DFB_TASK_LOG( Direct::String::F( "AddFlags( %s )", *ToString<TaskFlags>(flags) ) );

     this->flags = (TaskFlags)(this->flags | flags);
}

/*********************************************************************************************************************/

const Direct::String Task::_Type( "Task" );

const Direct::String &
Task::TypeName() const
{
     return _Type;
}


}

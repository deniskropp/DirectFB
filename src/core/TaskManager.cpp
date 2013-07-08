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
#include <core/TaskManager.h>
#include <core/Util.h>

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
TaskManager_SyncAll()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     TaskManager::SyncAll();
}

void
TaskManager_DumpTasks()
{
     D_DEBUG_AT( DirectFB_Task, "%s()\n", __FUNCTION__ );

     TaskManager::dumpTasks();
}


}

/*********************************************************************************************************************/

bool              TaskManager::running;
DirectThread     *TaskManager::thread;
FIFO<Task*>       TaskManager::fifo;
TaskThreads      *TaskManager::threads;
#if DFB_TASK_DEBUG_TASKS
std::list<Task*>  TaskManager::tasks;
DirectMutex       TaskManager::tasks_lock;
#endif
long long                     TaskManager::pull_timeout;
std::set<Task*,TaskManager>   TaskManager::timed_emits;


DFBResult
TaskManager::Initialise()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     D_ASSERT( thread == NULL );

#if DFB_TASK_DEBUG_TASKS
     direct_recursive_mutex_init( &tasks_lock );
#endif

     if (dfb_config->task_manager) {
          running = true;

          thread = direct_thread_create( DTT_CRITICAL, managerLoop, NULL, "Task Manager" );

          threads = new TaskThreads( "Task", 4 );
     }

     return DFB_OK;
}

void
TaskManager::Shutdown()
{
     D_ASSERT( direct_thread_self() != TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     if (thread != NULL) {
          running = false;

          fifo.push( NULL );

          direct_thread_join( thread );
          direct_thread_destroy( thread );

          thread = NULL;
     }

     if (threads != NULL) {
          delete threads;
          threads = NULL;
     }

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_deinit( &tasks_lock );
#endif
}

void
TaskManager::SyncAll()
{
     D_ASSERT( direct_thread_self() != TaskManager::thread );

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

#if DFB_TASK_DEBUG_TASKS
     int timeout = 10000;

     direct_mutex_lock( &TaskManager::tasks_lock );

     D_ASSUME( tasks.size() == 0 );

     while (tasks.size()) {
          if (!--timeout) {
               D_ERROR( "TaskManager: Timeout while waiting for all %zu remaining tasks!\n", tasks.size() );
               direct_trace_print_stacks();
               dumpTasks();
               break;
          }

          direct_mutex_unlock( &TaskManager::tasks_lock );
          usleep( 1000 );
          direct_mutex_lock( &TaskManager::tasks_lock );
     }

     direct_mutex_unlock( &TaskManager::tasks_lock );
#endif
}

void
TaskManager::pushTask( Task *task )
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s( %p )\n", __FUNCTION__, task );

     D_DEBUG_AT( DirectFB_Task, "  =-> pushTask [%s]\n", *task->Description() );

     fifo.push( task );
}

Task *
TaskManager::pullTask()
{
     DirectResult ret;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

#if 0
     static int c;

     if (c++ % 1000 == 0)
          dumpTasks();
#endif

     if (pull_timeout) {
          Task      *task;
          long long  now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          if (now >= pull_timeout)
               return NULL;

          ret = fifo.pull( &task, pull_timeout, now );
          if (ret)
               return NULL;

          return task;
     }

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

               if (task->ts_emit) {
                    long long now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

                    if (task->ts_emit > now) {
                         D_DEBUG_AT( DirectFB_Task, "  -> timed emit at %lld us (%lld from now)\n", task->ts_emit, task->ts_emit - now );
                         D_FLAGS_SET( task->flags, TASK_FLAG_WAITING_TIMED_EMIT );
                         TaskManager::timed_emits.insert( task );

                         if (task->ts_emit < TaskManager::pull_timeout || TaskManager::pull_timeout == 0)
                              TaskManager::pull_timeout = task->ts_emit;
                         break;
                    }
               }
               /* fall through */

          case TASK_READY:
               D_DEBUG_AT( DirectFB_Task, "  -> READY\n" );

               task->checkEmit();
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

          case TASK_DEAD:
               D_DEBUG_AT( DirectFB_Task, "  -> DEAD\n" );

               delete task;
               break;

          case TASK_INVALID:
               D_BUG( "invalid task %p", task );
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

          if (task) {
               D_DEBUG_AT( DirectFB_Task, "  =-> pulled a Task [%s]\n", *task->Description() );

               TaskManager::handleTask( task );
          }
          else if (!running) {
               D_DEBUG_AT( DirectFB_Task, "  =-> SHUTDOWN\n" );
               return NULL;
          }

          TaskManager::handleTimedEmits();
     }

     return NULL;
}

void
TaskManager::handleTimedEmits()
{
     long long                 now;
     std::set<Task*>::iterator it;

     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

     now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     while ((it = TaskManager::timed_emits.begin()) != TaskManager::timed_emits.end()) {
          Task *task = *it;

          if (now < task->ts_emit) {
               TaskManager::pull_timeout = task->ts_emit;

               D_DEBUG_AT( DirectFB_Task, "  -> next timed task at %lld us (%lld from now)\n", task->ts_emit, task->ts_emit - now );
               return;
          }

          D_DEBUG_AT( DirectFB_Task, "  =-> handling timed Task [%s]\n", *task->Description() );

          D_FLAGS_CLEAR( task->flags, TASK_FLAG_WAITING_TIMED_EMIT );

          task->checkEmit();

          TaskManager::timed_emits.erase( it );
     }

     TaskManager::pull_timeout = 0;
}

void
TaskManager::dumpTasks()
{
     D_DEBUG_AT( DirectFB_Task, "TaskManager::%s()\n", __FUNCTION__ );

#if DFB_TASK_DEBUG_TASKS
     direct_mutex_lock( &TaskManager::tasks_lock );

     direct_log_printf( NULL, "\ntask tree\n" );

     for (std::list<Task*>::const_iterator it = tasks.begin(); it != tasks.end(); it++) {
          Task *task = *it;

          task->DumpTree( 5 );
     }


     direct_log_printf( NULL, "\ntask       | state   | flags | no | bl | sl | is | finished\n" );

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


}


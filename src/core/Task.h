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

#ifndef ___DirectFB__Task__H___
#define ___DirectFB__Task__H___


#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <gfx/util.h>

#include <directfb.h>

#if D_DEBUG_ENABLED
#define DFB_TASK_DEBUG_LOG    (1)  // Task::Log(), DumpLog() enabled
#define DFB_TASK_DEBUG_STATE  (1)  // DFB_TASK_CHECK_STATE with warning and task log if enabled
#define DFB_TASK_DEBUG_TASKS  (1)  // TaskManager::dumpTasks() enabled
#else
#define DFB_TASK_DEBUG_LOG    (0)  // Task::Log(), DumpLog() enabled
#define DFB_TASK_DEBUG_STATE  (0)  // DFB_TASK_CHECK_STATE with warning and task log if enabled
#define DFB_TASK_DEBUG_TASKS  (0)  // TaskManager::dumpTasks() enabled
#endif

#define DFB_TASK_DEBUG_TIMES  (0)  // print warnings when task operations exceed time limits (set below)
#define DFB_TASK_DEBUG_TIMING (0)  // measure time it took the task in each state

/* max times in micro seconds before warning appears */
#define DFB_TASK_WARN_EMIT    3000
#define DFB_TASK_WARN_SETUP   3000
#define DFB_TASK_WARN_FINISH  3000


DFB_TaskList *TaskList_New( bool locked );
bool          TaskList_IsEmpty( DFB_TaskList *list );
DFBResult     TaskList_WaitEmpty( DFB_TaskList *list );
void          TaskList_Delete( DFB_TaskList *list );


void             Task_AddRef          ( DFB_Task                *task );
void             Task_Release         ( DFB_Task                *task );
void             Task_AddNotify       ( DFB_Task                *task,
                                        DFB_Task                *notified, // Task that should block on 'task'
                                        bool                     follow );
void             Task_Flush           ( DFB_Task                *task );
void             Task_Done            ( DFB_Task                *task );
void             Task_DoneFail        ( DFB_Task                *task,
                                        DFBResult                result );
void             Task_Log             ( DFB_Task                *task,
                                        const char              *action );


typedef DFBResult SimpleTaskFunc( void *ctx, DFB_Task *task );

DFBResult        SimpleTask_Create    ( SimpleTaskFunc          *push,     // If NULL, Push() will just call Run() in TaskManager thread!
                                        SimpleTaskFunc          *run,      // Can be NULL, but Push() must make sure the task gets Done()
                                        void                    *ctx,
                                        DFB_Task               **ret_task );


/*********************************************************************************************************************/

D_DEBUG_DOMAIN( DirectFB_Task, "DirectFB/Task", "DirectFB Task" );

#if DFB_TASK_DEBUG_STATE

#define DFB_TASK_CHECK_STATE( _task, _states, _ret )                                 \
     do {                                                                            \
          if (!((_task)->state & (_states))) {                                       \
               D_WARN( "task state (0x%02x %s) does not match 0x%02x -- %p",         \
                       (_task)->state, *ToString<DirectFB::TaskState>((_task)->state), (_states), _task );     \
                                                                                     \
               (_task)->DumpLog( DirectFB_Task, DIRECT_LOG_INFO );                   \
                                                                                     \
               D_ASSERT( (_task)->state & (_states) );                               \
                                                                                     \
               _ret;                                                                 \
          }                                                                          \
     } while (0)

#else

#define DFB_TASK_CHECK_STATE( _task, _states, _ret )                                 \
     do {                                                                            \
          if (!((_task)->state & (_states))) {                                       \
               _ret;                                                                 \
          }                                                                          \
     } while (0)

#endif


#if DFB_TASK_DEBUG_LOG
#define DFB_TASK_LOG(x) Log(x)
#else
#define DFB_TASK_LOG(x) do {} while (0)
#endif


#if D_DEBUG_ENABLED
#define DFB_TASK_ASSERT( _task, _ret )                                                    \
     do {                                                                                 \
          if (!D_MAGIC_CHECK( _task, Task )) {                                            \
               if (_task) {                                                               \
                    D_WARN( "Task '%p' magic (0x%08x) does not match 0x%08x",             \
                            _task, (_task)->magic, D_MAGIC("Task") );                     \
                    (_task)->DumpLog( DirectFB_Task, DIRECT_LOG_INFO );                   \
               }                                                                          \
               else                                                                       \
                    D_WARN( "task is NULL" );                                             \
          }                                                                               \
          D_MAGIC_ASSERT( _task, Task );                                                  \
     } while (0)
#else
#define DFB_TASK_ASSERT( _task, _ret )                                                    \
     do {                                                                                 \
          if (!D_MAGIC_CHECK( _task, Task )) {                                            \
               _ret;                                                                      \
          }                                                                               \
     } while (0)
#endif


#ifdef __cplusplus
}


#include <direct/Magic.h>
#include <direct/Mutex.h>
#include <direct/Performer.h>
#include <direct/String.h>

#include <core/Fifo.h>
#include <core/Util.h>

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>


namespace DirectFB {


typedef enum {
     TASK_STATE_NONE = 0x00000000,

     TASK_NEW        = 0x00000001,
     TASK_FLUSHED    = 0x00000002,
     TASK_READY      = 0x00000004,
     TASK_RUNNING    = 0x00000008,
     TASK_DONE       = 0x00000010,
     TASK_FINISH     = 0x00000020,
     TASK_DEAD       = 0x00000040,
     TASK_INVALID    = 0x00000080,

     TASK_STATE_ALL  = 0x000000FF
} TaskState;

typedef enum {
     TASK_FLAG_NONE               = 0x00000000,

     TASK_FLAG_NOSYNC             = 0x00000001,     /* Task is not accounted when TaskManager::Sync() is called */
     TASK_FLAG_EMITNOTIFIES       = 0x00000002,     /* Task runs notifyAll(0 already on emit(), not on finish() */
     TASK_FLAG_CACHE_FLUSH        = 0x00000004,     /*  */
     TASK_FLAG_CACHE_INVALIDATE   = 0x00000008,     /*  */
     TASK_FLAG_NEED_SLAVE_PUSH    = 0x00000010,     /* Push() of master does not take care about Push() for slaves */
     TASK_FLAG_LAST_IN_QUEUE      = 0x00000020,     /* Task has been last in queue and no next task was pushed to FIFO */
     TASK_FLAG_FOLLOW_READER      = 0x00000040,     /* Use 'follow' when depending on read tasks */
     TASK_FLAG_FOLLOW_WRITER      = 0x00000080,     /* Use 'follow' when depending on write tasks */
     TASK_FLAG_WAITING_TIMED_EMIT = 0x00000100,     /* Waiting on timed_emits list */

     TASK_FLAG_ALL                = 0x000001FF
} TaskFlags;


class Task;
class TaskManager;
class TaskThreads;

class DisplayTask;

typedef std::pair<Task*,TaskState> TaskNotify;


class Task
{
public:
     int magic;

protected:
     Task();
     virtual ~Task();

public:
     void      AddRef();
     void      Release();

     void      AddNotify( Task *notified,
                          bool  follow );
     void      AddSlave ( Task *slave );
     void      AddToList( DFB_TaskList *list );
     void      RemoveFromList( DFB_TaskList *list );

     virtual void      Done( DFBResult ret = DFB_OK );
     virtual void      Flush();


protected:
     virtual DFBResult Setup();
     virtual DFBResult Push();
     virtual DFBResult Run();
     virtual void      Finalise();
public:
     virtual void                  Describe( Direct::String &string ) const;
     virtual const Direct::String &TypeName() const;

protected:
     TaskState state;
     TaskFlags flags;

     DFBResult emit();
     DFBResult finish();

     void notifyAll( TaskState state );
     void checkEmit();
     void handleFlushed();
     void handleDone();
     void handleNotify();
     void enableDump();
     void append( Task *task );

private:
     friend class TaskManager;
     friend class TaskThreads;
     friend class TaskThreadsQ;

     /* reference counting */
     unsigned int             refs;

     /* building dependency tree */
     std::vector<TaskNotify>  notifies;      // FIXME: Use Direct::Vector
     unsigned int             block_count;

protected:
     /* queueing parallel tasks */
     unsigned int             slaves;
     Task                    *master;
     Task                    *next_slave;

     /* queueing sequential tasks */
     u64                      qid;
     Task                    *next;

     /* allocation on hwid */
     u32                      hwid;

     /* timing */
     long long                ts_emit;

#if DFB_TASK_DEBUG_TIMING
     long long                ts_flushed;
     long long                ts_ready;
     long long                ts_running;
     long long                ts_done;
#endif

private:
     bool                     dump;     // TODO: OPTIMISE: only buid with bool if needed

#if DFB_TASK_DEBUG_LOG
     class LogEntry {
     public:
          std::string         thread;
          std::string         action;
          long long           micros;
          DirectTraceBuffer  *trace;

          LogEntry()
               :
               micros(0),
               trace(NULL)
          {
          }
     };

     std::vector<LogEntry> tasklog;
     Direct::Mutex         tasklog_lock;
#endif


     Direct::String description;

public:
     void Log( const std::string &action );
     void DumpLog( DirectLogDomain &domain, DirectLogLevel level );
     void DumpTree( int indent = 0, int max = 50 );
     Direct::String &Description();
     void AddFlags( TaskFlags flags );

private:
     static const Direct::String _Type;
};


class TaskThreads : public Direct::Magic<TaskThreads> {
private:
     DirectFB::FIFO<Task*>      fifo;
     std::vector<DirectThread*> threads;

public:
     TaskThreads( const std::string &name, size_t num, DirectThreadType type = DTT_DEFAULT )
     {
          for (size_t i=0; i<num; i++) {
               DirectThread *thread = direct_thread_create( type, taskLoop, this, (num > 1) ?
                                                            Direct::String::F( "%s/%zu", name.c_str(), i ).buffer() :
                                                            Direct::String::F( "%s", name.c_str() ).buffer() );
               if (!thread)
                    break;

               threads.push_back( thread );
          }

          D_ASSUME( threads.size() == num );
     }

     ~TaskThreads()
     {
          for (size_t i=0; i<threads.size(); i++)
               fifo.push( NULL );

          for (std::vector<DirectThread*>::const_iterator it = threads.begin(); it != threads.end(); it++) {
               direct_thread_join( *it );
               direct_thread_destroy( *it );
          }
     }

     void Push( Task *task )
     {
          fifo.push( task );
     }

     static void *
     taskLoop( DirectThread *thread,
               void         *arg )
     {
          DFBResult    ret;
          TaskThreads *thiz = (TaskThreads *)arg;
          Task        *task;

          D_DEBUG_AT( DirectFB_Task, "TaskThreads::%s()\n", __FUNCTION__ );

          /* Preinitialize state client for possible blits from layer driver (avoids dead lock with region lock) */
          dfb_gfx_init_tls();

          while (true) {
               task = thiz->fifo.pull();
               if (!task) {
                    D_DEBUG_AT( DirectFB_Task, "TaskThreads::%s()  -> got NULL task (exit signal)\n", __FUNCTION__ );
                    return NULL;
               }

               ret = task->Run();
               if (ret) {
                    D_DERROR( ret, "TaskThreads: Task::Run() failed! [%s]\n", task->Description().buffer() );
                    task->Done( ret );
               }
          }

          return NULL;
     }
};


/*

Task
- optional recycle in Creator
- futuretime spec for emission
- limits (memory,count,futuretime) of tasks



= Graphics Operations =

   Dispatch Thread            Task Manager Thread             GPU / SW Thread

     - new GraphicsTask : SurfaceTask

       .
       .
       .

     - ref allocations

       .
       .

     - encode operations / locks


     - Task::Flush()  ->      - Task::Setup()
                              - put on hold or emit

                                    .

                              - Task::Emit()
                                - Task::Push()        ->     Task::Run()
                                                             .
                                                             .
                                                             .
                              - emit blocked tasks    <-     Task::Done()
                              - SurfaceTask::Finalise()
                                - unref allocations






= IDirectFBSurface::Lock() =

   Dispatch Thread            Task Manager Thread             GPU / SW Thread

     - new LockTask : SurfaceTask

       .
       .
       .

     - ref allocation

     - Task::Flush()  ->      - Task::Setup()
                              - put on hold or emit

              & wait                .
       .
       .                      - Task::Emit()
       .         <-- wakeup     - AppTask::Push()
     - Task::Run()

pool_lock / read / write / unlock

       .
       .
      Task::Done()    ->      - emit blocked tasks (if the Lock is blocking)
                              - SurfaceTask::Finalise()
                                - unref allocations



*/



}


/*********************************************************************************************************************/


#endif // __cplusplus


#endif


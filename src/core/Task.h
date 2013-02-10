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

#ifndef ___DirectFB__Task__H___
#define ___DirectFB__Task__H___


#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <directfb.h>


#define DFB_TASK_DEBUG_LOG    (0)  // Task::Log(), DumpLog() enabled
#define DFB_TASK_DEBUG_STATE  (0)  // DFB_TASK_CHECK_STATE with warning and task log if enabled
#define DFB_TASK_DEBUG_TASKS  (0)  // TaskManager::dumpTasks() enabled
#define DFB_TASK_DEBUG_TIMES  (0)  // print warnings when task operations exceed time limits (set below)

/* max times in micro seconds before warning appears */
#define DFB_TASK_WARN_EMIT    3000
#define DFB_TASK_WARN_SETUP   3000
#define DFB_TASK_WARN_FINISH  3000


DFBResult    TaskManager_Initialise( void );
void         TaskManager_Shutdown( void );
void         TaskManager_Sync( void );


void             Task_AddNotify       ( DFB_Task                *task,
                                        DFB_Task                *notified, // Task that should block on 'task'
                                        bool                     follow );
void             Task_Flush           ( DFB_Task                *task );
void             Task_Done            ( DFB_Task                *task );
void             Task_Log             ( DFB_Task                *task,
                                        const char              *action );


DFB_SurfaceTask *SurfaceTask_New      ( CoreSurfaceAccessorID    accessor );
DFBResult        SurfaceTask_AddAccess( DFB_SurfaceTask         *task,
                                        CoreSurfaceAllocation   *allocation,
                                        CoreSurfaceAccessFlags   flags );

DFBResult        DisplayTask_Generate ( CoreLayerRegion         *region,
                                        const DFBRegion         *update,
                                        DFBSurfaceFlipFlags      flags,
                                        DFB_DisplayTask        **ret_task );


typedef DFBResult SimpleTaskFunc( void *ctx, DFB_Task *task );

DFBResult        SimpleTask_Create    ( SimpleTaskFunc          *push,     // If NULL, Push() will just call Run() in TaskManager thread!
                                        SimpleTaskFunc          *run,      // Can be NULL, but Push() must make sure the task gets Done()
                                        void                    *ctx,
                                        DFB_Task               **ret_task );


D_DEBUG_DOMAIN( DirectFB_Task, "DirectFB/Task", "DirectFB Task" );

#if DFB_TASK_DEBUG_STATE

#define DFB_TASK_CHECK_STATE( _task, _states, _ret )                                 \
     do {                                                                            \
          if (!((_task)->state & (_states))) {                                       \
               D_WARN( "task state (0x%02x) does not match 0x%02x -- %s",            \
                       (_task)->state, (_states), (_task)->Description().buffer() ); \
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


#include <direct/Mutex.h>
#include <direct/String.h>

#include <core/Fifo.h>
#include <core/Util.h>

#include <list>
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
     TASK_INVALID    = 0x00000020,

     TASK_STATE_ALL  = 0x0000003F
} TaskState;

typedef enum {
     TASK_FLAG_NONE             = 0x00000000,

     TASK_FLAG_NOSYNC           = 0x00000001,     /* Task is not accounted when TaskManager::Sync() is called */
     TASK_FLAG_EMITNOTIFIES     = 0x00000002,     /* Task runs notifyAll(0 already on emit(), not on finish() */
     TASK_FLAG_CACHE_FLUSH      = 0x00000004,     /*  */
     TASK_FLAG_CACHE_INVALIDATE = 0x00000008,     /*  */
     TASK_FLAG_NEED_SLAVE_PUSH  = 0x00000010,     /* Push() of master does not take care about Push() for slaves */
     TASK_FLAG_LAST_IN_QUEUE    = 0x00000020,     /* Task has been last in queue and no next task was pushed to FIFO */

     TASK_FLAG_ALL              = 0x0000003F
} TaskFlags;


class Task;
class TaskManager;

class DisplayTask;

typedef std::pair<Task*,bool> TaskNotify;


class Task
{
public:
     int magic;

public:
     Task();
     virtual ~Task();

     void      AddNotify( Task *notified,
                          bool  follow );
     void      AddSlave ( Task *slave );

     void      Flush();
     void      Done();
     // TODO: Add Failed() ...


protected:
     virtual DFBResult Setup();
     virtual DFBResult Push();
     virtual DFBResult Run();
     virtual void      Finalise();
     virtual void      Describe( Direct::String &string );

protected:
     TaskState state;
     TaskFlags flags;

     DFBResult emit( int following );
     DFBResult finish();

     void notifyAll();
     void handleNotify( int following );
     void enableDump();
     void append( Task *task );

private:
     friend class TaskManager;
     friend class TaskThreads;
     friend class TaskThreadsQ;

     /* building dependency tree */
     std::vector<TaskNotify>  notifies;
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

private:
     bool                     finished;
     bool                     dump;

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
     Direct::String &Description();
};


class TaskManager
{
public:
     static DFBResult Initialise();
     static void      Shutdown();
     static void      Sync();

private:
     friend class Task;

     static DirectThread      *thread;
     static FIFO<Task*>        fifo;
     static unsigned int       task_count;
     static unsigned int       task_count_sync;

#if DFB_TASK_DEBUG_TASKS
     static std::list<Task*>   tasks;
     static DirectMutex        tasks_lock;
#endif

     static void       pushTask  ( Task *task );
     static Task      *pullTask  ();
     static DFBResult  handleTask( Task *task );

     static void      *managerLoop( DirectThread *thread,
                                    void         *arg );

public:
     static void       dumpTasks();
};


class TaskThreads {
private:
     DirectFB::FIFO<Task*>      fifo;
     std::vector<DirectThread*> threads;

public:
     TaskThreads( const std::string &name, size_t num, DirectThreadType type = DTT_DEFAULT )
     {
          for (size_t i=0; i<num; i++) {
               DirectThread *thread = direct_thread_create( type, taskLoop, this, (num > 1) ?
                                                            Direct::String( "%s/%zu", name.c_str(), i ).buffer() :
                                                            Direct::String( "%s", name.c_str() ).buffer() );
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

          while (true) {
               task = thiz->fifo.pull();
               if (!task) {
                    D_DEBUG_AT( DirectFB_Task, "TaskThreads::%s()  -> got NULL task (exit signal)\n", __FUNCTION__ );
                    return NULL;
               }

               ret = task->Run();
               if (ret)
                    D_DERROR( ret, "TaskThreads: Task::Run() failed! [%s]\n", task->Description().buffer() );
          }

          return NULL;
     }
};

class TaskThreadsQ {
private:
     class Runner {
     public:
          TaskThreadsQ *threads;
          unsigned int  index;
          DirectThread *thread;

          Runner( TaskThreadsQ         *threads,
                  unsigned int          index,
                  DirectThreadType      type,
                  const Direct::String &name );

          ~Runner();
     };

public:
     DirectFB::FIFO<Task*>              fifo;   // TODO: try to find better structure without lock
     std::vector<Runner*>               runners;
     std::map<u64,Task*>                queues;
     std::map<u64,Direct::PerfCounter>  perfs;

public:
     TaskThreadsQ( const std::string &name, size_t num, DirectThreadType type = DTT_DEFAULT );

     ~TaskThreadsQ();

     void Push( Task *task );

     void Finalise( Task *task );

private:
     static void *
     taskLoop( DirectThread *thread,
               void         *arg );
};


class SurfaceAllocationAccess {
public:
     CoreSurfaceAllocation  *allocation;
     CoreSurfaceAccessFlags  flags;

     SurfaceAllocationAccess( CoreSurfaceAllocation  *allocation,
                              CoreSurfaceAccessFlags  flags )
          :
          allocation( allocation ),
          flags( flags )
     {
     }
};

class SurfaceTask : public Task
{
public:
     SurfaceTask( CoreSurfaceAccessorID accessor );

     DFBResult AddAccess( CoreSurfaceAllocation  *allocation,
                          CoreSurfaceAccessFlags  flags );

protected:
     virtual DFBResult Setup();
     virtual void      Finalise();
     virtual void      Describe( Direct::String &string );

     virtual DFBResult CacheFlush();
     virtual DFBResult CacheInvalidate();

public://private:
     CoreSurfaceAccessorID                   accessor;
     std::vector<SurfaceAllocationAccess>    accesses;
};



class DisplayTask : public SurfaceTask
{
public:
     DisplayTask( CoreLayerRegion       *region,
                  const DFBRegion       *update,
                  DFBSurfaceFlipFlags    flip_flags,
                  CoreSurfaceAllocation *allocation );

     ~DisplayTask();

     static DFBResult Generate( CoreLayerRegion      *region,
                                const DFBRegion      *update,
                                DFBSurfaceFlipFlags   flags,
                                DisplayTask         **ret_task );

protected:
     virtual DFBResult Setup();
     virtual DFBResult Run();
     virtual void      Finalise();
     virtual void      Describe( Direct::String &string );

private:
     CoreLayerRegion       *region;
     DFBRegion             *update;
     DFBRegion              update_region;
     DFBSurfaceFlipFlags    flip_flags;
     CoreSurfaceAllocation *allocation;
     CoreLayer             *layer;
     CoreLayerContext      *context;
     int                    index;
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


#endif // __cplusplus

#endif


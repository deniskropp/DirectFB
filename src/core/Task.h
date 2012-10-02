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

#include <core/surface.h>

#include <directfb.h>


DFBResult    TaskManager_Initialise( void );
void         TaskManager_Shutdown( void );
void         TaskManager_Sync( void );


DFB_SurfaceTask *SurfaceTask_New      ( CoreSurfaceAccessorID   accessor );
DFBResult        SurfaceTask_AddAccess( DFB_SurfaceTask        *task,
                                        CoreSurfaceAllocation  *allocation,
                                        CoreSurfaceAccessFlags  flags );
void             SurfaceTask_Flush    ( DFB_SurfaceTask        *task );
void             SurfaceTask_Done     ( DFB_SurfaceTask        *task );


#ifdef __cplusplus
}


extern "C" {
#include <direct/thread.h>

#include <core/surface.h>

#include <directfb.h>
}


#include <list>
#include <queue>
#include <string>
#include <vector>


namespace DirectFB {


template <typename T>
class FIFO
{
public:
     FIFO()
     {
          direct_mutex_init( &lock );
          direct_waitqueue_init( &wq );
          direct_waitqueue_init( &wq_empty );
     }

     ~FIFO()
     {
          direct_mutex_deinit( &lock );
          direct_waitqueue_deinit( &wq );
          direct_waitqueue_deinit( &wq_empty );
     }

     void
     push( T e )
     {
          direct_mutex_lock( &lock );

          queue.push( e );

          direct_waitqueue_signal( &wq );

          direct_mutex_unlock( &lock );
     }

     T
     pull()
     {
          T e;

          direct_mutex_lock( &lock );

          while (queue.empty())
               direct_waitqueue_wait( &wq, &lock );

          e = queue.front();
          queue.pop();

//          if (queue.empty())
               direct_waitqueue_broadcast( &wq_empty );

          direct_mutex_unlock( &lock );

          return e;
     }

     bool
     empty()
     {
          bool val;

          direct_mutex_lock( &lock );

          val = queue.empty();

          direct_mutex_unlock( &lock );

          return val;
     }

     void
     waitEmpty()
     {
          direct_mutex_lock( &lock );

          while (!queue.empty())
               direct_waitqueue_wait( &wq_empty, &lock );

          direct_mutex_unlock( &lock );
     }

     void
     waitMost( size_t count )
     {
          direct_mutex_lock( &lock );

          while (queue.size() > count)
               direct_waitqueue_wait( &wq_empty, &lock );

          direct_mutex_unlock( &lock );
     }

     size_t
     count()
     {
          size_t count;

          direct_mutex_lock( &lock );

          count = queue.size();

          direct_mutex_unlock( &lock );

          return count;
     }

private:
     DirectMutex     lock;
     DirectWaitQueue wq;
     DirectWaitQueue wq_empty;

     std::queue<T>   queue;
};


template <typename T>
class FastFIFO
{
class Element {
public:
     DirectFifoItem item;
     T              val;
};

public:
     FastFIFO()
     {
          direct_fifo_init( &fifo );
     }

     ~FastFIFO()
     {
          direct_fifo_destroy( &fifo );
     }

     void
     push( T e )
     {
          Element *element = new Element;

          element->item.magic = 0;
          element->val        = e;

          direct_fifo_push( &fifo, &element->item );
     }

     T
     pull()
     {
          Element *element;
          T        val;

          do {
               element = (Element*) direct_fifo_pull( &fifo );
               if (!element)
                    direct_fifo_wait( &fifo );
          } while (!element);

          val = element->val;

          delete element;

          return val;
     }

private:
     DirectFifo      fifo;
};





typedef enum {
     TASK_NEW,
     TASK_FLUSHED,
     TASK_READY,
     TASK_RUNNING,
     TASK_DONE
} TaskState;

typedef enum {
     TASK_FLAG_NONE         = 0x00000000,

     TASK_FLAG_NOSYNC       = 0x00000001,     /* Task is not accounted when TaskManager::Sync() is called */
     TASK_FLAG_EMITNOTIFIES = 0x00000002,     /* Task runs notifyAll(0 already on emit(), not on finish() */

     TASK_FLAG_ALL          = 0x00000003,
} TaskFlags;


class Task;
class TaskManager;

class DisplayTask;

typedef std::pair<Task*,bool> TaskNotify;

class Task
{
public:
     Task();
     virtual ~Task();

     void      AddSlave( Task *slave );

     void      Flush();
     void      Done();

protected:
     virtual DFBResult   Setup();
     virtual DFBResult   Push();
     virtual DFBResult   Run();
     virtual void        Finalise();
     virtual std::string Describe();

protected:
     TaskState state;
     TaskFlags flags;

     DFBResult emit( bool following );
     DFBResult finish();

     void addNotify( Task *task,
                     bool  follow );
     void notifyAll();
     void handleNotify( bool following );

private:
     friend class TaskManager;
     friend class DisplayTask;

     std::vector<TaskNotify>  notifies;
     unsigned int             block_count;

     unsigned int             slaves;
     Task                    *master;
     Task                    *next_slave;

     bool                     finished;
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

     static std::list<Task*>   tasks;

     static void       pushTask  ( Task *task );
     static Task      *pullTask  ();
     static DFBResult  handleTask( Task *task );

     static void      *managerLoop( DirectThread *thread,
                                    void         *arg );

public:
     static void       dumpTasks();
};



typedef std::pair<CoreSurfaceAllocation*,CoreSurfaceAccessFlags> SurfaceAllocationAccess;

class SurfaceTask : public Task
{
public:
     SurfaceTask( CoreSurfaceAccessorID accessor );

     DFBResult AddAccess( CoreSurfaceAllocation  *allocation,
                          CoreSurfaceAccessFlags  flags );

     // FIXME: make private if possible
     CoreSurfaceAccessorID                   accessor;

protected:
     virtual DFBResult   Setup();
     virtual void        Finalise();
     virtual std::string Describe();

private:
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

     static DFBResult Generate( CoreLayerRegion     *region,
                                const DFBRegion     *update,
                                DFBSurfaceFlipFlags  flags );

protected:
     virtual DFBResult Setup();
     virtual void Finalise();
     virtual DFBResult Run();
     virtual std::string Describe();

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

Tasks
- flag if task is blocking subsequent tasks


Run()
 - cache invalidate when flag is set

Done()
 - cache flush when flag is set




Cache
- add cache groups to avoid flushing between SW threads or multi core GPUs

Tiles / SLI
- allow parallel tasks, splitting allocations into parts
- dynamic splitting to avoid overhead with normal access
- may need pushTasks( task[] ) function to atomically push more than one task

Siblings
- parallel tasks can be connected as siblings
- master task reponsible for notifies, but slaves need to signal master
- could make multiple readers work like this



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


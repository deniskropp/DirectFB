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


#ifdef __cplusplus
}


extern "C" {
#include <direct/thread.h>

#include <core/surface.h>

#include <directfb.h>
}


#include <queue>
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
     }

     ~FIFO()
     {
          direct_mutex_deinit( &lock );
          direct_waitqueue_deinit( &wq );
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

          direct_mutex_unlock( &lock );

          return e;
     }

     bool
     empty() const
     {
          return queue.empty();
     }

private:
     DirectMutex     lock;
     DirectWaitQueue wq;

     std::queue<T>   queue;
};





typedef enum {
     TASK_NEW,
     TASK_FLUSHED,
     TASK_READY,
     TASK_RUNNING,
     TASK_DONE
} TaskState;


class TaskManager;

class Task
{
public:
     Task();
     virtual ~Task();

     void      AddSlave( Task *slave );

     void      Flush();
     void      Done();

protected:
     virtual DFBResult Setup();
     virtual DFBResult Push();
     virtual DFBResult Run();
     virtual void      Finalise();

protected:
     TaskState state;

     DFBResult emit();
     DFBResult finish();

     void addNotify( Task *task );
     void notifyAll();
     void handleNotify();

private:
     friend class TaskManager;

     TaskManager        *manager;

     std::vector<Task*>  notifies;
     unsigned int        block_count;

     unsigned int        slaves;
     Task               *master;
     Task               *next_slave;

     bool                finished;
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

     static void       pushTask  ( Task *task );
     static Task      *pullTask  ();
     static DFBResult  handleTask( Task *task );

     static void      *managerLoop( DirectThread *thread,
                                    void         *arg );
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
     virtual DFBResult Setup();
     virtual void      Finalise();

private:
     std::vector<SurfaceAllocationAccess>    accesses;
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


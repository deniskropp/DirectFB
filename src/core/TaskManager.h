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



#ifndef ___DirectFB__TaskManager__H___
#define ___DirectFB__TaskManager__H___


#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <gfx/util.h>

#include <directfb.h>


DFBResult    TaskManager_Initialise( void );
void         TaskManager_Shutdown( void );
void         TaskManager_SyncAll( void );
void         TaskManager_DumpTasks( void );


#ifdef __cplusplus
}


#include <direct/Magic.h>
#include <direct/Mutex.h>
#include <direct/Performer.h>
#include <direct/String.h>

#include <core/Fifo.h>
#include <core/Task.h>
#include <core/Util.h>

#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>


namespace DirectFB {


class Task;
class TaskThreads;


class TaskManager
{
public:
     static DFBResult Initialise();
     static void      Shutdown();
     static void      SyncAll();

private:
     friend class Task;

     static bool               running;

     static DirectThread      *thread;
     static FIFO<Task*>        fifo;

     static TaskThreads       *threads;

#if DFB_TASK_DEBUG_TASKS
     static std::list<Task*>   tasks;
     static DirectMutex        tasks_lock;
#endif

     static long long                        pull_timeout;
     static std::set<Task*,TaskManager>      timed_emits;


     static void       pushTask  ( Task *task );
     static Task      *pullTask  ();
     static DFBResult  handleTask( Task *task );

     static void      *managerLoop( DirectThread *thread,
                                    void         *arg );
     static void       handleTimedEmits();

public:
     static void       dumpTasks();


     bool operator()( Task *t1, Task *t2 )
     {
          return t1->ts_emit < t2->ts_emit;
     }
};


}

#endif // __cplusplus


#endif


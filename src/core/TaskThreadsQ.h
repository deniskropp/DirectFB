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



#ifndef ___DirectFB__TaskThreadsQ__H___
#define ___DirectFB__TaskThreadsQ__H___


#include <directfb.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <gfx/util.h>


// C wrapper?


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


class Task;
class TaskManager;
class TaskThreads;


class TaskThreadsQ : public Direct::Magic<TaskThreadsQ> {
private:
     class Runner : public Direct::Magic<Runner> {
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
     DirectFB::FIFO<Task*>              fifo;   // TODO: try to find better structure without lock, maybe futex
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



}


#endif // __cplusplus


#endif


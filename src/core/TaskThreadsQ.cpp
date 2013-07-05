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

#include <directfb.h>
#include <directfb_util.h>


extern "C" {
#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <misc/conf.h>
}

#include <direct/String.h>

#include <core/TaskManager.h>
#include <core/TaskThreadsQ.h>
#include <core/Util.h>

D_DEBUG_DOMAIN( DirectFB_TaskThreadsQ, "DirectFB/TaskThreadsQ", "DirectFB TaskThreadsQ" );

/*********************************************************************************************************************/

namespace DirectFB {


extern "C" {




}

/*********************************************************************************************************************/

TaskThreadsQ::Runner::Runner( TaskThreadsQ         *threads,
                              unsigned int          index,
                              DirectThreadType      type,
                              const Direct::String &name )
     :
     threads( threads ),
     index( index )
{
     thread = direct_thread_create( type, taskLoop, this, name.buffer() );
}

TaskThreadsQ::Runner::~Runner()
{
     direct_thread_join( thread );
     direct_thread_destroy( thread );
}

/*********************************************************************************************************************/

TaskThreadsQ::TaskThreadsQ( const std::string &name, size_t num, DirectThreadType type )
{
     D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s( '%s', num %zu, type %d )\n", __FUNCTION__, name.c_str(), num, type );

     for (size_t i=0; i<num; i++) {
          runners.push_back( new Runner( this, i, type, (num > 1) ?
                                                            Direct::String::F( "%s/%zu", name.c_str(), i ) :
                                                            Direct::String::F( "%s", name.c_str() ) ) );
     }

     D_ASSUME( runners.size() == num );
}

TaskThreadsQ::~TaskThreadsQ()
{
     for (size_t i=0; i<runners.size(); i++)
          fifo.push( NULL );

     for (std::vector<Runner*>::const_iterator it = runners.begin(); it != runners.end(); it++)
          delete *it;
}


void TaskThreadsQ::Push( Task *task )
{
     static D_PERF_COUNTER( TaskThreadsQ__Push, "TaskThreadsQ::Push" );

     D_PERF_COUNT( TaskThreadsQ__Push );

     D_MAGIC_ASSERT( task, Task );

     D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s( task [%s] )\n", __FUNCTION__, task->Description().buffer() );

     D_ASSERT( task->qid != 0 );

     D_PERF_COUNT_N( perfs[task->qid].counter, +1 );


     Task *last = queues[task->qid];

     D_DEBUG_AT( DirectFB_TaskThreadsQ, "  -> last task %p\n", last );

     D_MAGIC_ASSERT_IF( last, Task );

     queues[task->qid] = task;

     if (last)
          last->append( task );
     else {
          D_DEBUG_AT( DirectFB_TaskThreadsQ, "  -> pushing task %p\n", task );

          fifo.push( task );
     }
}

void TaskThreadsQ::Finalise( Task *task )
{
     static D_PERF_COUNTER( TaskThreadsQ__Finalise, "TaskThreadsQ::Finalise" );

     D_PERF_COUNT( TaskThreadsQ__Finalise );

     D_MAGIC_ASSERT( task, Task );

     D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s( task [%s] ) <- next %p\n", __FUNCTION__, task->Description().buffer(), task->next );

     D_ASSERT( task->qid != 0 );
     D_MAGIC_ASSERT_IF( task->next, Task );

     if (D_FLAGS_ARE_SET( task->flags, TASK_FLAG_LAST_IN_QUEUE )) {
          static D_PERF_COUNTER( FLAG_LAST, "TASK_FLAG_LAST_IN_QUEUE" );

          D_PERF_COUNT( FLAG_LAST );

          D_DEBUG_AT( DirectFB_TaskThreadsQ, "  -> TASK_FLAG_LAST_IN_QUEUE\n" );

          if (task->next) {
               D_ASSERT( queues[task->qid] != task );

               D_DEBUG_AT( DirectFB_TaskThreadsQ, "  -> pushing task %p to resume operation\n", task->next );

               fifo.push( task->next );
          }
          else {
               D_ASSERT( queues[task->qid] == task );

               queues.erase( task->qid );
          }
     }

     D_ASSERT( queues[task->qid] != task );
}

void *
TaskThreadsQ::taskLoop( DirectThread *thread,
                        void         *arg )
{
     DFBResult     ret;
     Runner       *runner = (Runner *)arg;
     TaskThreadsQ *thiz   = runner->threads;
     Task         *task, *next;

     D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()\n", __FUNCTION__ );

     while (true) {
          task = thiz->fifo.pull();
          if (!task) {
               D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> got NULL task (exit signal)\n", __FUNCTION__ );
               return NULL;
          }

          D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> got task [%s]\n", __FUNCTION__, task->Description().buffer() );

          task->hwid = runner->index;

          //bool foo=false;
next:
          D_MAGIC_ASSERT( task, Task );

          next = task->next;

          if (!next) {
               D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> NO NEXT\n", __FUNCTION__ );
               //if (foo) {
                    D_FLAGS_SET( task->flags, TASK_FLAG_LAST_IN_QUEUE );
               //}
               //else {
               //     foo = true;
               //     direct_thread_sleep( 1000 );
               //     goto next;
               //}
          }
          else
               D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> next will be [%s]\n", __FUNCTION__, next->Description().buffer() );

          D_MAGIC_ASSERT_IF( next, Task );

          D_PERF_COUNT_N( thiz->perfs[task->qid].counter, -1 );  // not fully thread safe

          ret = task->Run();
          if (ret)
               D_DERROR( ret, "TaskThreadsQ: Task::Run() failed! [%s]\n", task->Description().buffer() );

          if (next) {
               if (0) {
                    D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> running next (%p)...\n", __FUNCTION__, next );
                    task = next;
                    goto next;
               }

               D_DEBUG_AT( DirectFB_TaskThreadsQ, "TaskThreadsQ::%s()  -> pushing next [%s]...\n", __FUNCTION__, next->Description().buffer() );

               D_MAGIC_ASSERT( next, Task );

               thiz->fifo.push( next );
          }
     }

     return NULL;
}


}

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

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/processor.h>
#include <direct/thread.h>

D_LOG_DOMAIN( Direct_Processor,         "Direct/Processor",         "Direct Processor" );
D_LOG_DOMAIN( Direct_Processor_Alloc,   "Direct/Processor/Alloc",   "Direct Processor Allocation" );
D_LOG_DOMAIN( Direct_Processor_Post,    "Direct/Processor/Post",    "Direct Processor Posting" );
D_LOG_DOMAIN( Direct_Processor_Recycle, "Direct/Processor/Recycle", "Direct Processor Recycling" );

/**********************************************************************************************************************/

typedef struct {
     DirectFifoItem     item;

     int                magic;
} ProcessorCommand;

/**********************************************************************************************************************/

static void *
processor_thread( DirectThread *thread,
                  void         *ctx )
{
     DirectResult                ret;
     bool                        started   = false;
     DirectProcessor            *processor = ctx;
     const DirectProcessorFuncs *funcs;

     D_DEBUG_AT( Direct_Processor, "%s( %p, %p )...\n", __FUNCTION__, thread, ctx );

     D_MAGIC_ASSERT( processor, DirectProcessor );

     funcs = processor->funcs;
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->Process != NULL );

     while (!processor->stop) {
          ProcessorCommand *command = direct_fifo_pull( &processor->commands );

          if (command) {
               D_DEBUG_AT( Direct_Processor, "=---### %p - %p (%s)\n", command, command + 1, processor->name );

               D_MAGIC_ASSERT( command, ProcessorCommand );

               if (!started) {
                    if (funcs->Start)
                         funcs->Start( processor, processor->context );

                    started = true;
               }

               ret = funcs->Process( processor, command + 1, processor->context );
               if (ret)
                    D_DERROR( ret, "Direct/Processor: Processing failed! (%s)\n", processor->name );
          }
          else {
               if (started) {
                    if (funcs->Stop)
                         funcs->Stop( processor, processor->context );

                    started = false;
               }

#if 0
               while (processor->lock) {
                    direct_mutex_lock( &processor->lock_mutex );

                    processor->locked = true;

                    direct_waitqueue_signal( &processor->lock_cond );

                    while (processor->lock)
                         direct_waitqueue_wait( &processor->lock_cond, &processor->lock_mutex );

                    processor->locked = false;

                    direct_waitqueue_signal( &processor->lock_cond );

                    direct_mutex_unlock( &processor->lock_mutex );
               }
#endif

               if (processor->idle_ms) {
                    while (direct_fifo_wait_timed( &processor->commands, processor->idle_ms ) == DR_TIMEOUT) {
                         if (funcs->Idle)
                              funcs->Idle( processor, processor->context );
                    }
               }
               else {
                    if (funcs->Idle)
                         funcs->Idle( processor, processor->context );

                    direct_fifo_wait( &processor->commands );
               }
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

DirectResult
direct_processor_init( DirectProcessor            *processor,
                       const char                 *name,
                       const DirectProcessorFuncs *funcs,
                       unsigned int                data_size,
                       void                       *context,
                       int                         idle_ms )
{
     D_ASSERT( processor != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->Process != NULL );
     D_ASSERT( data_size > 0 );

     D_DEBUG_AT( Direct_Processor, "%s( %p, %p, %p, %u, %p )\n", __FUNCTION__,
                 processor, name, funcs, data_size, context );

     processor->name = D_STRDUP( name );
     if (!processor->name)
          return D_OOM();

     processor->funcs        = funcs;
     processor->data_size    = data_size;
     processor->context      = context;
     processor->idle_ms      = idle_ms;
     processor->max_recycled = (8000 / data_size) + 1;

     direct_fifo_init( &processor->commands );
     direct_fifo_init( &processor->recycled );

#if 0
     direct_waitqueue_init( &processor->lock_cond, NULL );
     direct_mutex_init( &processor->lock_mutex, NULL );
#endif

     D_MAGIC_SET( processor, DirectProcessor );

     processor->thread = direct_thread_create( DTT_DEFAULT, processor_thread, processor, name );
     if (!processor->thread) {
          D_MAGIC_CLEAR( processor );
          direct_fifo_destroy( &processor->commands );
          direct_fifo_destroy( &processor->recycled );
          D_FREE( processor->name );
          return DR_INIT;
     }

     return DR_OK;
}

DirectResult
direct_processor_destroy( DirectProcessor *processor )
{
     void *item;

     D_MAGIC_ASSERT( processor, DirectProcessor );

     D_DEBUG_AT( Direct_Processor, "%s( %p '%s' )\n", __FUNCTION__, processor, processor->name );

     processor->stop = true;

     direct_thread_terminate( processor->thread );

     direct_fifo_wakeup( &processor->commands );
     direct_fifo_wakeup( &processor->recycled );

     direct_thread_join( processor->thread );
     direct_thread_destroy( processor->thread );


     while ((item = direct_fifo_pull( &processor->commands )) != NULL)
          D_FREE( item );
          
     direct_fifo_destroy( &processor->commands );


     while ((item = direct_fifo_pull( &processor->recycled )) != NULL)
          D_FREE( item );

     direct_fifo_destroy( &processor->recycled );


#if 0
     direct_waitqueue_deinit( &processor->lock_cond );
     direct_mutex_deinit( &processor->lock_mutex );
#endif

     D_FREE( processor->name );

     D_MAGIC_CLEAR( processor );

     return DR_OK;
}

void *
direct_processor_allocate( DirectProcessor *processor )
{
     ProcessorCommand *command;

     D_MAGIC_ASSERT( processor, DirectProcessor );

     D_DEBUG_AT( Direct_Processor_Alloc, "%s( %p '%s' )\n", __FUNCTION__, processor, processor->name );

     command = direct_fifo_pop( &processor->recycled );
     if (command) {
          D_MAGIC_ASSERT( command, ProcessorCommand );
     }
     else {
          command = D_CALLOC( 1, sizeof(ProcessorCommand) + processor->data_size );
          if (!command) {
               D_OOM();
               return NULL;
          }

          D_MAGIC_SET( command, ProcessorCommand );
     }

     D_DEBUG_AT( Direct_Processor_Alloc, "  -> %p - %p\n", command, command + 1 );

     return command + 1;
}

void
direct_processor_post( DirectProcessor *processor,
                       void            *data )
{
     ProcessorCommand *command = (ProcessorCommand*) data - 1;

     D_MAGIC_ASSERT( processor, DirectProcessor );
     D_MAGIC_ASSERT( command, ProcessorCommand );

     D_DEBUG_AT( Direct_Processor_Post, "%s( %p '%s' <---= %p )\n", __FUNCTION__, processor, processor->name, data );

     if (processor->direct) {
          DirectResult                ret;
          const DirectProcessorFuncs *funcs;

//          D_ASSUME( !processor->commands.count );

          funcs = processor->funcs;
          D_ASSERT( funcs != NULL );
          D_ASSERT( funcs->Process != NULL );
          
          ret = funcs->Process( processor, data, processor->context );
          if (ret)
               D_DERROR( ret, "Direct/Processor: Processing directly failed! (%s)\n", processor->name );
     }
     else
          direct_fifo_push( &processor->commands, &command->item );
}

void
direct_processor_recycle( DirectProcessor *processor,
                          void            *data )
{
     ProcessorCommand *command = (ProcessorCommand*) data - 1;

     D_MAGIC_ASSERT( processor, DirectProcessor );
     D_MAGIC_ASSERT( command, ProcessorCommand );

     D_DEBUG_AT( Direct_Processor_Recycle, "%s( %p '%s' <- %p )\n", __FUNCTION__, processor, processor->name, data );

//     if (processor->recycled.count < processor->max_recycled)
          direct_fifo_push( &processor->recycled, &command->item );
/*     else {
          D_MAGIC_CLEAR( command );

          D_FREE( command );
     }*/
}

#if 0
void
direct_processor_lock( DirectProcessor *processor )
{
     direct_mutex_lock( &processor->lock_mutex );

     processor->lock++;

     while (!processor->locked) {
          direct_fifo_wakeup( &processor->commands );

          direct_waitqueue_wait( &processor->lock_cond, &processor->lock_mutex );
     }

     direct_mutex_unlock( &processor->lock_mutex );
}

void
direct_processor_unlock( DirectProcessor *processor )
{
     direct_mutex_lock( &processor->lock_mutex );

     if (! --processor->lock && processor->locked)
          direct_waitqueue_signal( &processor->lock_cond );

     direct_mutex_unlock( &processor->lock_mutex );
}
#endif


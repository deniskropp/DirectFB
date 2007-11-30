/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <stdlib.h>

#include <pthread.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/signals.h>
#include <direct/thread.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Main, "Direct/Main", "Initialization and shutdown of libdirect" );

/**************************************************************************************************/

struct __D_DirectCleanupHandler {
     DirectLink                link;
     
     int                       magic;
     
     DirectCleanupHandlerFunc  func;
     void                     *ctx;
};

/**************************************************************************************************/

static int              refs      = 0;
static DirectLink      *handlers  = NULL; 
static pthread_mutex_t  main_lock = PTHREAD_MUTEX_INITIALIZER;

static DirectLog       *default_log;

/**************************************************************************************************/

static void
direct_cleanup( void )
{
     DirectCleanupHandler *handler, *temp;
     
     if (!refs)
          return;
          
     direct_list_foreach_safe (handler, temp, handlers) {
          D_DEBUG_AT( Direct_Main, "Calling cleanup func %p...\n", handler->func );

          handler->func( handler->ctx );
          
          /*direct_list_remove( &handlers, &handler->link );

          D_MAGIC_CLEAR( handler );

          D_FREE( handler );*/
     }
     
     direct_print_memleaks();

     direct_print_interface_leaks();
}

/**************************************************************************************************/

DirectResult 
direct_cleanup_handler_add( DirectCleanupHandlerFunc   func,
                            void                      *ctx,
                            DirectCleanupHandler     **ret_handler )
{
     DirectCleanupHandler *handler;
     
     D_ASSERT( func != NULL );
     D_ASSERT( ret_handler != NULL );
     
     D_DEBUG_AT( Direct_Main,
                 "Adding cleanup handler %p with context %p...\n", func, ctx );
     
     handler = D_CALLOC( 1, sizeof(DirectCleanupHandler) );
     if (!handler) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }
     
     handler->func = func;
     handler->ctx  = ctx;
     
     D_MAGIC_SET( handler, DirectCleanupHandler );

     pthread_mutex_lock( &main_lock );
     
     if (handlers == NULL)
          atexit( direct_cleanup );
     
     direct_list_append( &handlers, &handler->link );
     
     pthread_mutex_unlock( &main_lock );
     
     *ret_handler = handler;
     
     return DFB_OK;
}

DirectResult
direct_cleanup_handler_remove( DirectCleanupHandler *handler )
{
     D_ASSERT( handler != NULL );
     
     D_MAGIC_ASSERT( handler, DirectCleanupHandler );

     D_DEBUG_AT( Direct_Main, "Removing cleanup handler %p with context %p...\n", 
                 handler->func, handler->ctx );

     pthread_mutex_lock( &main_lock );
     direct_list_remove( &handlers, &handler->link );
     pthread_mutex_unlock( &main_lock );

     D_MAGIC_CLEAR( handler );

     D_FREE( handler );

     return DFB_OK;
}

DirectResult
direct_initialize()
{
     pthread_mutex_lock( &main_lock );

     direct_log_create( DLT_STDERR, NULL, &default_log );
     direct_log_set_default( default_log );

     D_DEBUG_AT( Direct_Main, "direct_initialize() called...\n" );

     if (refs++) {
          D_DEBUG_AT( Direct_Main, "...%d references now.\n", refs );
          pthread_mutex_unlock( &main_lock );
          return DFB_OK;
     }
     else if (!direct_thread_self_name())
          direct_thread_set_name( "Main Thread" );

     D_DEBUG_AT( Direct_Main, "...initializing now.\n" );

     direct_signals_initialize();

     pthread_mutex_unlock( &main_lock );

     return DFB_OK;
}

DirectResult
direct_shutdown()
{
     pthread_mutex_lock( &main_lock );

     D_DEBUG_AT( Direct_Main, "direct_shutdown() called...\n" );

     if (--refs) {
          D_DEBUG_AT( Direct_Main, "...%d references left.\n", refs );
          pthread_mutex_unlock( &main_lock );
          return DFB_OK;
     }

     D_DEBUG_AT( Direct_Main, "...shutting down now.\n" );

     direct_signals_shutdown();

     direct_log_destroy( default_log );

     pthread_mutex_unlock( &main_lock );

     return DFB_OK;
}


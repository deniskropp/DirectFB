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

#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/log.h>
#include <direct/print.h>
#include <direct/thread.h>
#include <direct/util.h>


/**********************************************************************************************************************/
/**********************************************************************************************************************/

/* Statically allocated to avoid endless loops between D_CALLOC() and D_DEBUG(), while the latter would only
 * call the allocation once, if there wouldn't be the loopback...
 */
static DirectLog  fallback_log;
static DirectLog *default_log;

void
__D_log_init()
{
     DirectLog *fb = &fallback_log;

     fb->type = DLT_STDERR;

     direct_recursive_mutex_init( &fb->lock );

     direct_log_init( fb, NULL );

     D_MAGIC_SET( fb, DirectLog );
}

void
__D_log_deinit()
{
     DirectLog *fb = &fallback_log;

     direct_log_deinit( fb );

     direct_mutex_deinit( &fb->lock );

     D_MAGIC_CLEAR( fb );

     default_log = NULL;
}

/**********************************************************************************************************************/

DirectResult
direct_log_create( DirectLogType   type,
                   const char     *param,
                   DirectLog     **ret_log )
{
     DirectResult  ret;
     DirectLog    *log;

     log = D_CALLOC( 1, sizeof(DirectLog) );
     if (!log)
          return D_OOM();

     log->type = type;

     direct_recursive_mutex_init( &log->lock );

     ret = direct_log_init( log, param );
     if (ret) {
          direct_mutex_deinit( &log->lock );
          D_FREE( log );
          return ret;
     }

     D_ASSERT( log->write != NULL );

     D_MAGIC_SET( log, DirectLog );

     *ret_log = log;

     return DR_OK;
}

DirectResult
direct_log_destroy( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     D_ASSERT( &fallback_log != log );

     if (log == default_log)
          default_log = NULL;

     direct_log_deinit( log );

     direct_mutex_deinit( &log->lock );

     D_MAGIC_CLEAR( log );

     D_FREE( log );

     return DR_OK;
}

__no_instrument_function__
DirectResult
direct_log_printf( DirectLog  *log,
                   const char *format, ... )
{
     DirectResult  ret = 0;
     va_list       args;
     int           len;
     char          buf[2000];
     char         *ptr = buf;

     /*
      * Don't use D_MAGIC_ASSERT or any other
      * macros/functions that might cause an endless loop.
      */

     /* Use the default log if passed log is invalid. */
     if (!D_MAGIC_CHECK( log, DirectLog ))
          log = direct_log_default();

     if (!D_MAGIC_CHECK( log, DirectLog ))
          return DR_BUG;


     va_start( args, format );
     len = direct_vsnprintf( buf, sizeof(buf), format, args );
     va_end( args );

     if (len < 0)
          return DR_FAILURE;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len+1 );
          if (!ptr)
               return DR_NOLOCALMEMORY;
          
          va_start( args, format );
          len = direct_vsnprintf( ptr, len+1, format, args );
          va_end( args );

          if (len < 0) {
               direct_free( ptr );
               return DR_FAILURE;
          }
     }


     direct_mutex_lock( &log->lock );

     ret = log->write( log, ptr, len );

     direct_mutex_unlock( &log->lock );

     if (ptr != buf)
          direct_free( ptr );

     return ret;
}

DirectResult
direct_log_set_default( DirectLog *log )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     default_log = log;

     return DR_OK;
}

__no_instrument_function__
void
direct_log_lock( DirectLog *log )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     if (!log)
          log = direct_log_default();

     D_MAGIC_ASSERT( log, DirectLog );

     direct_mutex_lock( &log->lock );
}

__no_instrument_function__
void
direct_log_unlock( DirectLog *log )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     if (!log)
          log = direct_log_default();

     D_MAGIC_ASSERT( log, DirectLog );

     direct_mutex_unlock( &log->lock );
}

DirectResult
direct_log_set_buffer( DirectLog *log,
                       char      *buffer,
                       size_t     bytes )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     if (!log)
          log = direct_log_default();

     D_MAGIC_ASSERT( log, DirectLog );

     if (!log->set_buffer)
          return DR_UNSUPPORTED;

     return log->set_buffer( log, buffer, bytes );
}

DirectResult
direct_log_flush( DirectLog *log,
                  bool       sync )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     if (!log)
          log = direct_log_default();

     D_MAGIC_ASSERT( log, DirectLog );

     if (!log->flush)
          return DR_UNSUPPORTED;

     return log->flush( log, sync );
}

/**********************************************************************************************************************/

__no_instrument_function__
DirectLog *
direct_log_default( void )
{
     return default_log ? default_log : &fallback_log;
}


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



#include <config.h>

#include <direct/build.h>


#ifndef DIRECT_DISABLE_DEPRECATED

#include <pthread.h>

#include <direct/clock.h>
#include <direct/filesystem.h>
#include <direct/util.h>

/**********************************************************************************************************************/

// @deprecated
void
direct_clock_set_start( const struct timeval *start )
{
     long long start_micros;

     start_micros = (long long) start->tv_sec * 1000000LL + (long long) start->tv_usec;

     direct_clock_set_time( DIRECT_CLOCK_SESSION, direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) - start_micros );
}

// @deprecated
int
direct_try_open( const char *name1, const char *name2, int flags, bool error_msg )
{
     DirectResult ret;
     DirectFile   file;

     ret = direct_file_open( &file, name1, flags, 660 );
     if (ret) {
          if (ret != DR_FILENOTFOUND) {
               if (error_msg)
                    D_DERROR( ret, "Direct/Util: Opening '%s' failed!\n", name1 );
               return -1;
          }

          ret = direct_file_open( &file, name2, flags, 660 );
          if (ret && error_msg) {
               if (ret == DR_FILENOTFOUND)
                    D_DERROR( ret, "Direct/Util: Opening '%s' and '%s' failed!\n", name1, name2 );
               else
                    D_DERROR( ret, "Direct/Util: Opening '%s' failed!\n", name2 );
          }
     }

     return ret ? -1 : file.fd;
}

// @deprecated
int
direct_util_recursive_pthread_mutex_init( pthread_mutex_t *mutex )
{
     int                 ret;
     pthread_mutexattr_t attr;

     pthread_mutexattr_init( &attr );
#if HAVE_DECL_PTHREAD_MUTEX_RECURSIVE
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
#endif
     ret = pthread_mutex_init( mutex, &attr );
     if (ret)
          D_PERROR( "Direct/Lock: Could not initialize recursive mutex!\n" );

     pthread_mutexattr_destroy( &attr );

     return ret;
}

#endif


/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/mutex.h>
#include <direct/util.h>

/**********************************************************************************************************************/

DirectResult
direct_mutex_init( DirectMutex *mutex )
{
     if (pthread_mutex_init( &mutex->lock, NULL ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_recursive_mutex_init( DirectMutex *mutex )
{
     DirectResult        ret;
     int                 result;
     pthread_mutexattr_t attr;

     pthread_mutexattr_init( &attr );
#if HAVE_DECL_PTHREAD_MUTEX_RECURSIVE
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
#endif
     result = pthread_mutex_init( &mutex->lock, &attr );
     if (result) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Mutex: Could not initialize recursive mutex!\n" );
     }

     pthread_mutexattr_destroy( &attr );

     return (DirectResult) ret;
}

__attribute__((no_instrument_function))
DirectResult
direct_mutex_lock( DirectMutex *mutex )
{
     if (pthread_mutex_lock( &mutex->lock ))
          return errno2result( errno );

     return DR_OK;
}

__attribute__((no_instrument_function))
DirectResult
direct_mutex_unlock( DirectMutex *mutex )
{
     if (pthread_mutex_unlock( &mutex->lock ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_mutex_trylock( DirectMutex *mutex )
{
     if (pthread_mutex_trylock( &mutex->lock ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_mutex_deinit( DirectMutex *mutex )
{
     if (pthread_mutex_destroy( &mutex->lock ))
          return errno2result( errno );

     return DR_OK;
}


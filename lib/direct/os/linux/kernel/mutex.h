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

#ifndef __DIRECT__OS__LINUX__KERNEL__MUTEX_H__
#define __DIRECT__OS__LINUX__KERNEL__MUTEX_H__

#include <direct/types.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION( 2, 6, 24 )    // guess
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#define __SEMAPHORE_INITIALIZER __SEMAPHORE_INIT
#endif

/**********************************************************************************************************************/

struct __D_DirectMutex {
     struct semaphore    sema;
};

/**********************************************************************************************************************/

#define DIRECT_MUTEX_INITIALIZER(name)                 { .sema = __SEMAPHORE_INITIALIZER( name.sema, 1 ) }
#define DIRECT_RECURSIVE_MUTEX_INITIALIZER(name)       { .sema = __SEMAPHORE_INITIALIZER( name.sema, 1 ) }

/**********************************************************************************************************************/

static inline DirectResult
direct_mutex_init( DirectMutex *mutex )
{
     init_MUTEX( &mutex->sema );

     return DR_OK;
}

static inline DirectResult
direct_recursive_mutex_init( DirectMutex *mutex )
{
     init_MUTEX( &mutex->sema );

     return DR_OK;
}

__attribute__((no_instrument_function))
static inline DirectResult
direct_mutex_lock( DirectMutex *mutex )
{
     int ret;

     ret = down_interruptible( &mutex->sema );
     if (ret)
          return DR_SIGNALLED;

     return DR_OK;
}

__attribute__((no_instrument_function))
static inline DirectResult
direct_mutex_unlock( DirectMutex *mutex )
{
     up( &mutex->sema );

     return DR_OK;
}

static inline DirectResult
direct_mutex_trylock( DirectMutex *mutex )
{
     if (down_trylock( &mutex->sema ))
          return DR_LOCKED;

     return DR_OK;
}

static inline DirectResult
direct_mutex_deinit( DirectMutex *mutex )
{
     return DR_OK;
}

#endif


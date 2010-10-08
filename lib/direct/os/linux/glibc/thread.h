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

#ifndef __DIRECT__OS__LINUX__GLIBC__THREAD_H__
#define __DIRECT__OS__LINUX__GLIBC__THREAD_H__

#include <pthread.h>

#include <direct/util.h>

/**********************************************************************************************************************/

struct __D_DirectThreadHandle {
     pthread_t           thread;
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/

struct __D_DirectOnce {
     pthread_once_t once;
};

/**********************************************************************************************************************/

#define DIRECT_ONCE_INIT      { PTHREAD_ONCE_INIT }

/**********************************************************************************************************************/

typedef void (*DirectOnceInitHandler)( void );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static inline DirectResult direct_once( DirectOnce            *once,
                                        DirectOnceInitHandler  handler );

static inline DirectResult
direct_once( DirectOnce            *once,
             DirectOnceInitHandler  handler )
{
     if (pthread_once( &once->once, handler ))
          return errno2result( errno );

     return DR_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

struct __D_DirectTLS {
     pthread_key_t key;
};

/**********************************************************************************************************************/

#define DIRECT_TLS_DATA( name )    \
     static DirectTLS name = { (pthread_key_t) -1 }

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static inline void *direct_tls_get__( DirectTLS *tls );

__attribute__((no_instrument_function))
static inline DirectResult direct_tls_set__( DirectTLS *tls,
                                             void      *value );

__attribute__((no_instrument_function))
static inline DirectResult direct_tls_register( DirectTLS  *tls,
                                                void      (*destructor)( void* ) );

__attribute__((no_instrument_function))
static inline DirectResult direct_tls_unregister( DirectTLS *tls );

/**********************************************************************************************************************/

#define direct_tls_get( name )                    direct_tls_get__( &name )
#define direct_tls_set( name, v )                 direct_tls_set__( &name, v )

/**********************************************************************************************************************/

static inline void *
direct_tls_get__( DirectTLS *tls )
{
     void *value;

     value = pthread_getspecific( tls->key );

     return value;
}

static inline DirectResult
direct_tls_set__( DirectTLS *tls,
                  void      *value )
{
     if (pthread_setspecific( tls->key, value ))
          return errno2result( errno );

     return DR_OK;
}

static inline DirectResult
direct_tls_register( DirectTLS *tls, void (*destructor)( void* ) )
{
     if (pthread_key_create( &tls->key, destructor ))
          return errno2result( errno );

     return DR_OK;
}

static inline DirectResult
direct_tls_unregister( DirectTLS *tls )
{
     if (pthread_key_delete( tls->key ))
          return errno2result( errno );

     tls->key = (pthread_key_t) -1;

     return DR_OK;
}


#endif


/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifndef FUSION_FAKE
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif


#include "fusion_types.h"
#include "lock.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

FusionResult
fusion_skirmish_init (FusionSkirmish *skirmish)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( skirmish != NULL );

     while (ioctl (_fusion_fd, FUSION_SKIRMISH_NEW, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          FPERROR ("FUSION_SKIRMISH_NEW");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_skirmish_prevail (FusionSkirmish *skirmish)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( skirmish != NULL );

     while (ioctl (_fusion_fd, FUSION_SKIRMISH_PREVAIL, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid skirmish\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_SKIRMISH_PREVAIL");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_skirmish_swoop (FusionSkirmish *skirmish)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( skirmish != NULL );

     while (ioctl (_fusion_fd, FUSION_SKIRMISH_SWOOP, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("invalid skirmish\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_SKIRMISH_SWOOP");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_skirmish_dismiss (FusionSkirmish *skirmish)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( skirmish != NULL );

     while (ioctl (_fusion_fd, FUSION_SKIRMISH_DISMISS, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid skirmish\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_SKIRMISH_DISMISS");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
fusion_skirmish_destroy (FusionSkirmish *skirmish)
{
     DFB_ASSERT( _fusion_fd != -1 );
     DFB_ASSERT( skirmish != NULL );

     while (ioctl (_fusion_fd, FUSION_SKIRMISH_DESTROY, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("invalid skirmish\n");
                    return FUSION_DESTROYED;
               default:
                    break;
          }

          FPERROR ("FUSION_SKIRMISH_DESTROY");

          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

#else  /* !FUSION_FAKE */

FusionResult
fusion_skirmish_init (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );

     fusion_pthread_recursive_mutex_init( &skirmish->fake.lock );

     return FUSION_SUCCESS;
}

FusionResult
fusion_skirmish_prevail (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );

     return pthread_mutex_lock( &skirmish->fake.lock );
}

FusionResult
fusion_skirmish_swoop (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );

     return pthread_mutex_trylock( &skirmish->fake.lock );
}

FusionResult
fusion_skirmish_dismiss (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );

     return pthread_mutex_unlock( &skirmish->fake.lock );
}

FusionResult
fusion_skirmish_destroy (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );

     return pthread_mutex_destroy( &skirmish->fake.lock );
}

#endif


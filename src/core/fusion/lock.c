/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#define _GNU_SOURCE

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifndef FUSION_FAKE

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>

#include <sys/ioctl.h>
#include <linux/fusion.h>

#endif


#include "fusion_types.h"
#include "lock.h"

#include "fusion_internal.h"


/***************************
 *  Internal declarations  *
 ***************************/


/*******************
 *  Internal data  *
 *******************/



/****************
 *  Public API  *
 ****************/

#ifndef FUSION_FAKE

FusionResult
skirmish_init (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );
     
     if (ioctl (fusion_fd, FUSION_SKIRMISH_NEW, &skirmish->id)) {
          FPERROR ("FUSION_SKIRMISH_NEW");
          return FUSION_FAILURE;
     }

     return FUSION_SUCCESS;
}

FusionResult
skirmish_prevail (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );
     
     while (ioctl (fusion_fd, FUSION_SKIRMISH_PREVAIL, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("skirmish already destroyed\n");
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
skirmish_swoop (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );
     
     while (ioctl (fusion_fd, FUSION_SKIRMISH_SWOOP, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EAGAIN:
                    return FUSION_INUSE;
               case EINVAL:
                    FERROR ("skirmish already destroyed\n");
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
skirmish_dismiss (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );
     
     while (ioctl (fusion_fd, FUSION_SKIRMISH_DISMISS, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("skirmish already destroyed\n");
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
skirmish_destroy (FusionSkirmish *skirmish)
{
     DFB_ASSERT( skirmish != NULL );
     
     while (ioctl (fusion_fd, FUSION_SKIRMISH_DESTROY, &skirmish->id)) {
          switch (errno) {
               case EINTR:
                    continue;
               case EINVAL:
                    FERROR ("skirmish already destroyed\n");
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
skirmish_init (FusionSkirmish *skirmish)
{
     pthread_mutexattr_t attr;

     DFB_ASSERT( skirmish != NULL );
     
     pthread_mutexattr_init( &attr );
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
     
     pthread_mutex_init( skirmish, &attr );
     
     pthread_mutexattr_destroy( &attr );

     return FUSION_SUCCESS;
}

#endif

/*******************************
 *  Fusion internal functions  *
 *******************************/



/*****************************
 *  File internal functions  *
 *****************************/



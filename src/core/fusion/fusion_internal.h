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

#ifndef __FUSION_INTERNAL_H__
#define __FUSION_INTERNAL_H__

#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>

#ifndef FUSION_FAKE
#include <sys/ioctl.h>
#include <linux/fusion.h>
#endif

#include <string.h>

#include <config.h>

#include <core/fusion/fusion.h>
#include <core/fusion/list.h>
#include <core/fusion/lock.h>

#include <misc/conf.h>
#include <misc/util.h>


#ifdef DFB_DEBUG
#  define FUSION_DEBUG
#endif

#if !defined(FUSION_DEBUG) || defined(DFB_NOTEXT)
#  define FDEBUG(x...)   do {} while (0)
#else
#  define FDEBUG(x...)   do { if (dfb_config->debug) {                         \
                                 fprintf( stderr, "(-) [%d: %5lld] DirectFB/"  \
                                          "core/fusion: (%s) ", getpid(),      \
                                          fusion_get_millis(), __FUNCTION__ ); \
                                 fprintf( stderr, x );                         \
                                 fflush( stderr );                             \
                            } } while (0)
#endif

#if defined(DFB_NOTEXT)
#  define FERROR(x...)  do {} while (0)
#  define FPERROR(x...) do {} while (0)
#else
#  define FERROR(x...) do \
{ \
     fprintf( stderr, "(!) [%d: %5lld] DirectFB/core/fusion: (%s) ",           \
              getpid(), fusion_get_millis(), __FUNCTION__ );                   \
     fprintf( stderr, x );                                                     \
     fflush( stderr );                                                         \
} while (0)

#  define FPERROR(x...) do \
{ \
     fprintf( stderr, "(!) [%d: %5lld] DirectFB/core/fusion: (%s) ",           \
              getpid(), fusion_get_millis(), __FUNCTION__ );                   \
     fprintf( stderr, x );                                                     \
     fprintf( stderr, "    --> " );                                            \
     perror("");                                                               \
     fflush( stderr );                                                         \
} while (0)
#endif


/***************************************
 *  Fusion internal type declarations  *
 ***************************************/

typedef struct {
     struct timeval  start_time;

     FusionLink     *arenas;
     FusionSkirmish  arenas_lock;
} FusionShared;

/*******************************************
 *  Fusion internal function declarations  *
 *******************************************/

/*
 * from fusion.c
 */
extern int _fusion_id;
extern int _fusion_fd;

extern FusionShared *_fusion_shared;

/*
 * from reactor.c
 */
void _reactor_free_all();
void _reactor_process_message( int reactor_id, const void *msg_data );

/*
 * from call.c
 */
#ifndef FUSION_FAKE
void _fusion_call_process( int call_id, FusionCallMessage *call );
#endif

#endif /* __FUSION_INTERNAL_H__ */


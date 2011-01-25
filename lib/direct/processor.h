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

#ifndef __DIRECT__PROCESSOR_H__
#define __DIRECT__PROCESSOR_H__

#include <direct/fifo.h>
#include <direct/types.h>

/**********************************************************************************************************************/

typedef struct {
     DirectResult (*Start)( DirectProcessor *processor,
                            void            *context );

     DirectResult (*Process)( DirectProcessor *processor,
                              void            *data,
                              void            *context );

     DirectResult (*Stop)( DirectProcessor *processor,
                           void            *context );

     DirectResult (*Idle)( DirectProcessor *processor,
                           void            *context );
} DirectProcessorFuncs;

/**********************************************************************************************************************/

struct __D_DirectProcessor {
     int                          magic;

     DirectThread                *thread;

     DirectFifo                   commands;
     DirectFifo                   recycled;

     int                          max_recycled;

     bool                         direct;
     bool                         stop;

#if 0
     bool                         locked;

     int                          lock;
     DirectWaitQueue              lock_cond;
     DirectMutex                  lock_mutex;
#endif

     char                        *name;

     const DirectProcessorFuncs  *funcs;
     unsigned int                 data_size;
     void                        *context;
     int                          idle_ms;
};

/**********************************************************************************************************************/

DirectResult DIRECT_API  direct_processor_init    ( DirectProcessor            *processor,
                                                    const char                 *name,
                                                    const DirectProcessorFuncs *funcs,
                                                    unsigned int                data_size,
                                                    void                       *context,
                                                    int                         idle_ms );

DirectResult DIRECT_API  direct_processor_destroy ( DirectProcessor *processor );

void         DIRECT_API *direct_processor_allocate( DirectProcessor *processor );

void         DIRECT_API  direct_processor_post    ( DirectProcessor *processor,
                                                    void            *data );

void         DIRECT_API  direct_processor_recycle ( DirectProcessor *processor,
                                                    void            *data );

#if 0
void         DIRECT_API  direct_processor_lock    ( DirectProcessor *processor );
void         DIRECT_API  direct_processor_unlock  ( DirectProcessor *processor );
#endif

#endif


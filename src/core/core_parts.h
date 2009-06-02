/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE_PARTS_H__
#define __CORE_PARTS_H__

#include <fusion/types.h>
#include <fusion/lock.h>

#include <directfb.h>

#include <core/coretypes.h>
#include <core/coredefs.h>


typedef DFBResult (*CoreInitialize)( CoreDFB *core,
                                     void    *data_local,
                                     void    *data_shared );

typedef DFBResult (*CoreJoin)      ( CoreDFB *core,
                                     void    *data_local,
                                     void    *data_shared );

typedef DFBResult (*CoreShutdown)  ( void    *data_local,
                                     bool     emergency );

typedef DFBResult (*CoreLeave)     ( void    *data_local,
                                     bool     emergency );

typedef DFBResult (*CoreSuspend)   ( void    *data_local );

typedef DFBResult (*CoreResume)    ( void    *data_local );


typedef struct {
     const char     *name;

     int             size_local;
     int             size_shared;

     CoreInitialize  Initialize;
     CoreJoin        Join;
     CoreShutdown    Shutdown;
     CoreLeave       Leave;
     CoreSuspend     Suspend;
     CoreResume      Resume;

     void           *data_local;
     void           *data_shared;

     bool            initialized;
} CorePart;


DFBResult dfb_core_part_initialize( CoreDFB  *core,
                                    CorePart *core_part );

DFBResult dfb_core_part_join      ( CoreDFB  *core,
                                    CorePart *core_part );

DFBResult dfb_core_part_shutdown  ( CoreDFB  *core,
                                    CorePart *core_part,
                                    bool      emergency );

DFBResult dfb_core_part_leave     ( CoreDFB  *core,
                                    CorePart *core_part,
                                    bool      emergency );


#define DFB_CORE_PART(part,Type)                                               \
                                                                               \
static DFBResult dfb_##part##_initialize( CoreDFB           *core,             \
                                          DFB##Type         *local,            \
                                          DFB##Type##Shared *shared );         \
                                                                               \
static DFBResult dfb_##part##_join      ( CoreDFB           *core,             \
                                          DFB##Type         *local,            \
                                          DFB##Type##Shared *shared );         \
                                                                               \
static DFBResult dfb_##part##_shutdown  ( DFB##Type         *local,            \
                                          bool               emergency );      \
                                                                               \
static DFBResult dfb_##part##_leave     ( DFB##Type         *local,            \
                                          bool               emergency );      \
                                                                               \
static DFBResult dfb_##part##_suspend   ( DFB##Type         *local );          \
                                                                               \
static DFBResult dfb_##part##_resume    ( DFB##Type         *local );          \
                                                                               \
CorePart dfb_##part = {                                                        \
     .name        = #part,                                                     \
                                                                               \
     .size_local  = sizeof(DFB##Type),                                         \
     .size_shared = sizeof(DFB##Type##Shared),                                 \
                                                                               \
     .Initialize  = (void*)dfb_##part##_initialize,                            \
     .Join        = (void*)dfb_##part##_join,                                  \
     .Shutdown    = (void*)dfb_##part##_shutdown,                              \
     .Leave       = (void*)dfb_##part##_leave,                                 \
     .Suspend     = (void*)dfb_##part##_suspend,                               \
     .Resume      = (void*)dfb_##part##_resume,                                \
}


#endif


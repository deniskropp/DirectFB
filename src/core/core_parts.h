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

#ifndef __CORE_PARTS_H__
#define __CORE_PARTS_H__

#include <core/fusion/fusion_types.h>
#include <core/fusion/lock.h>

#include <directfb.h>

#include <core/coretypes.h>
#include <core/coredefs.h>

typedef DFBResult (*CoreInitialize)( void *data_local,
                                     void *data_shared );

typedef DFBResult (*CoreJoin)      ( void *data_local,
                                     void *data_shared );

typedef DFBResult (*CoreShutdown)  ( bool  emergency );

typedef DFBResult (*CoreLeave)     ( bool  emergency );

typedef DFBResult (*CoreSuspend)   (  );

typedef DFBResult (*CoreResume)    (  );

typedef struct {
     const char     *name;

     int             size_local;
     int             size_shared;

     void           *data_local;
     void           *data_shared;

     bool            initialized;
     
     CoreInitialize  Initialize;
     CoreJoin        Join;
     CoreShutdown    Shutdown;
     CoreLeave       Leave;
     CoreSuspend     Suspend;
     CoreResume      Resume;
} CorePart;

extern CorePart dfb_core_clipboard;
extern CorePart dfb_core_colorhash;
extern CorePart dfb_core_system;
extern CorePart dfb_core_input;
extern CorePart dfb_core_gfxcard;
extern CorePart dfb_core_layers;

DFBResult dfb_core_part_initialize( CorePart *core_part );
DFBResult dfb_core_part_join      ( CorePart *core_part );
DFBResult dfb_core_part_shutdown  ( CorePart *core_part,
                                    bool      emergency );
DFBResult dfb_core_part_leave     ( CorePart *core_part,
                                    bool      emergency );


#define DFB_CORE_PART(part,sl,ss)                                              \
                                                                               \
static DFBResult dfb_##part##_initialize( void *data_local,                    \
                                          void *data_shared );                 \
                                                                               \
static DFBResult dfb_##part##_join      ( void *data_local,                    \
                                          void *data_shared );                 \
                                                                               \
static DFBResult dfb_##part##_shutdown  ( bool  emergency );                   \
                                                                               \
static DFBResult dfb_##part##_leave     ( bool  emergency );                   \
                                                                               \
static DFBResult dfb_##part##_suspend   (  );                                  \
                                                                               \
static DFBResult dfb_##part##_resume    (  );                                  \
                                                                               \
CorePart dfb_core_##part = {                                                   \
     #part,                                                                    \
     sl, ss, NULL, NULL, false,                                                \
                                                                               \
     dfb_##part##_initialize,                                                  \
     dfb_##part##_join,                                                        \
     dfb_##part##_shutdown,                                                    \
     dfb_##part##_leave,                                                       \
     dfb_##part##_suspend,                                                     \
     dfb_##part##_resume                                                       \
};


#endif


/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef __DIRECTFB_INTERNALS_H__
#define __DIRECTFB_INTERNALS_H__

typedef struct
{
     char *filename;
     char *type;
     char *implementation;

     int   references;
     
     DFBResult (*Probe)( void *data, ... );
     DFBResult (*Construct)( void *interface, ... );
} DFBInterfaceImplementation;

DFBResult DFBGetInterface( DFBInterfaceImplementation **iimpl,
                           char *type,
                           char *implementation,
                           int (*probe)( DFBInterfaceImplementation *impl, void *ctx ),
                           void *probe_ctx );

#define DFB_ALLOCATE_INTERFACE(p,i)     \
     (p) = (i*)calloc( 1, sizeof(i) );
     
     
extern IDirectFB *idirectfb_singleton;

typedef void (*DFBSuspendResumeFunc)( int suspend, void *ctx );

void DFBAddSuspendResumeFunc( DFBSuspendResumeFunc func, void *ctx );
void DFBRemoveSuspendResumeFunc( DFBSuspendResumeFunc func, void *ctx );

#endif /* __DIRECTFB_INTERNALS_H__ */

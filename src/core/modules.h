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

#ifndef __MODULES_H__
#define __MODULES_H__

#include <core/coretypes.h>

#include <core/fusion/fusion_types.h>
#include <core/fusion/list.h>

struct _ModuleEntry {
     FusionLink       link;

     ModuleDirectory *directory;

     bool             loaded;
     bool             dynamic;
     bool             disabled;

     char            *name;
     const void      *funcs;
     
     int              refs;
     char            *file;
     void            *handle;
};

struct _ModuleDirectory {
     const char   *path;
     unsigned int  abi_version;

     FusionLink   *entries;

     ModuleEntry  *loading;
};

#define DECLARE_MODULE_DIRECTORY(d)  \
     extern ModuleDirectory d

#define DEFINE_MODULE_DIRECTORY(d,p,n) \
     ModuleDirectory d = {             \
          path:        MODULEDIR"/"p,  \
          abi_version: n,              \
          entries:     NULL,           \
          loading:     NULL            \
     }

int   dfb_modules_explore_directory( ModuleDirectory *directory );

void  dfb_modules_register( ModuleDirectory *directory,
                            unsigned int     abi_version,
                            const char      *name,
                            const void      *funcs );

const void *dfb_module_ref  ( ModuleEntry *module );
void        dfb_module_unref( ModuleEntry *module );

#endif


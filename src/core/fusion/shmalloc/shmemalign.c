/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   Fusion shmalloc is based on GNU malloc. Please see below.

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

/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include "../shmalloc.h"
#include "shmalloc_internal.h"

void *
_fusion_shmemalign (size_t alignment, size_t size)
{
     void          *result;
     unsigned long  adj;

     result = _fusion_shmalloc (size + alignment - 1);
     if (result == NULL)
          return NULL;

     adj = (unsigned long) result % alignment;
     if (adj != 0) {
          struct alignlist *l;

          for (l = _sheap->aligned_blocks; l != NULL; l = l->next)
               if (l->aligned == NULL)
                    /* This slot is free.  Use it.  */
                    break;

          if (l == NULL) {
               l = (struct alignlist *) _fusion_shmalloc (sizeof (struct alignlist));
               if (l == NULL) {
                    _fusion_shfree (result);
                    return NULL;
               }

               l->next = _sheap->aligned_blocks;
               _sheap->aligned_blocks = l;
          }

          l->exact = result;
          result = l->aligned = (char *) result + alignment - adj;
     }

     return result;
}

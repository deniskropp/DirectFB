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

#ifndef __FUSION__TYPES_H__
#define __FUSION__TYPES_H__

#ifdef __cplusplus
extern "C"
{
#else

typedef enum {
  false = 0,
  true = 1
} bool;

#endif


  typedef enum {
    FUSION_SUCCESS   = 0,
    FUSION_FAILURE,
    FUSION_BUG,
    FUSION_UNIMPLEMENTED,
    FUSION_INVARG,
    FUSION_DESTROYED,
    FUSION_ACCESSDENIED,
    FUSION_PERMISSIONDENIED,
    FUSION_NOTEXISTENT,
    FUSION_LIMITREACHED,
    FUSION_TOOHIGH,
    FUSION_TOOLONG,
    FUSION_INUSE
  } FusionResult;

  typedef struct _FusionReactor      FusionReactor;
  typedef struct _FusionArena        FusionArena;
  
  typedef struct _FusionObject       FusionObject;
  typedef struct _FusionObjectPool   FusionObjectPool;


  union semun {
    int val;                    /* value for SETVAL */
    struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
    unsigned short int *array;  /* array for GETALL, SETALL */
    struct seminfo *__buf;      /* buffer for IPC_INFO */
  };
				
				
#ifdef __cplusplus
}
#endif

#endif /* __FUSION__TYPES_H__ */


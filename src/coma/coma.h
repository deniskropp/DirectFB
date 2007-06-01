/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __COMA__COMA_H__
#define __COMA__COMA_H__

#include <fusiondale.h>

#include <fusion/types.h>

#include "coma_types.h"



/*
 * Coma initialization and deinitialization
 */
DirectResult coma_enter( FusionWorld  *world,
                         const char   *name,
                         Coma        **ret_coma );

DirectResult coma_exit ( Coma         *coma,
                         bool          emergency );

/*
 * Components
 */
DirectResult coma_create_component( Coma            *coma,
                                    const char      *name,
                                    ComaMethodFunc   func,
                                    int              num_notifications,
                                    void            *ctx,
                                    ComaComponent  **ret_component );

DirectResult coma_get_component   ( Coma            *coma,
                                    const char      *name,
                                    unsigned int     timeout,
                                    ComaComponent  **ret_component );
/*
 * Thread local SHM
 */
DirectResult coma_get_local       ( Coma            *coma,
                                    unsigned int     bytes,
                                    void           **ret_ptr );

DirectResult coma_free_local      ( Coma            *coma );

/* 
 * Misc
 */
FusionWorld         *coma_world  ( const Coma *coma );
FusionSHMPoolShared *coma_shmpool( const Coma *coma );


/*
 * Internal
 */
void _coma_internal_remove_component( Coma          *coma,
                                      ComaComponent *component );

#endif

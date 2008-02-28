/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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

#include <config.h>

#include <fusionsound.h>

#include <ifusionsound.h>

#include <misc/sound_conf.h>

#include <direct/interface.h>


static DirectResult Probe( void *arg );

static DirectResult Construct( IFusionSound *thiz, void *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSound, default )


/* exported symbols */

static DirectResult
Probe( void *arg )
{
     return DR_OK;
}

static DirectResult
Construct( IFusionSound *thiz,
           void         *arg )
{
     DirectResult ret;
     
     ret = FusionSoundInit( NULL, NULL );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return ret;
     }

     if (fs_config->remote.host) {
          DirectInterfaceFuncs *funcs;

          ret = DirectGetInterface( &funcs, "IFusionSound", "Requestor", NULL, NULL );
          if (ret) {
               DIRECT_DEALLOCATE_INTERFACE(thiz);
               return ret;
          }

          return funcs->Construct( thiz, fs_config->remote.host, fs_config->remote.session );
     }          

     return IFusionSound_Construct( thiz );
}

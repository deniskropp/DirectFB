/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdio.h>

#include <fusionsound.h>
#include <fusionsound_version.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <ifusionsound.h>

#include <misc/conf.h>

#include <misc/fs_config.h>


IFusionSound *ifusionsound_singleton = NULL;

/**************************************************************************************************/

static DFBResult CreateRemote( const char    *host,
                               int            session,
                               IFusionSound **ret_interface );

/**************************************************************************************************/

DFBResult
FusionSoundInit( int *argc, char **argv[] )
{
     DFBResult ret;
     
     ret = dfb_config_init( argc, argv );
     if (ret == DFB_OK)
          ret = fs_config_init( argc, argv );
          
     return ret;
}

DFBResult
FusionSoundSetOption( const char *name, const char *value )
{
     if (fs_config == NULL) {
          D_ERROR( "FusionSoundSetOption: FusionSoundInit has to be called first!\n" );
          return DFB_INIT;
     }

     if (ifusionsound_singleton) {
          D_ERROR( "FusionSoundSetOption: FusionSoundCreate has already been called!\n" );
          return DFB_INIT;
     }

     if (!name)
          return DFB_INVARG;

     return fs_config_set( name, value );
}

DirectResult
FusionSoundCreate( IFusionSound **ret_interface )
{
     DFBResult ret;
     
     if (!dfb_config || !fs_config) {
          D_ERROR( "FusionSoundCreate: FusionSoundInit has to be called first!\n" );
          return DFB_INIT;
     }

     if (!ret_interface)
          return DFB_INVARG;
          
     if (ifusionsound_singleton) {
          ifusionsound_singleton->AddRef( ifusionsound_singleton );
          *ret_interface = ifusionsound_singleton;
          return DFB_OK;
     }

     if (!direct_config->quiet && dfb_config->banner) {
          fprintf( stderr, "\n" );
          fprintf( stderr, "       --------------------- FusionSound v%d.%d.%d -------------------\n",
                          FUSIONSOUND_MAJOR_VERSION, FUSIONSOUND_MINOR_VERSION, FUSIONSOUND_MICRO_VERSION );
          fprintf( stderr, "             (c) 2000-2002  convergence integrated media GmbH  \n" );
          fprintf( stderr, "             (c) 2002-2005  convergence GmbH                   \n" );
          fprintf( stderr, "        -----------------------------------------------------------\n" );
          fprintf( stderr, "\n" );
     }

     if (dfb_config->remote.host)
          return CreateRemote( dfb_config->remote.host, dfb_config->remote.session, ret_interface );
          
     DIRECT_ALLOCATE_INTERFACE( ifusionsound_singleton, IFusionSound );
     
     ret = IFusionSound_Construct( ifusionsound_singleton );
     if (ret != DFB_OK)
          ifusionsound_singleton = NULL;
          
     *ret_interface = ifusionsound_singleton;

     return ret;
}

DirectResult
FusionSoundError( const char *msg, DirectResult error )
{
     if (msg)
          fprintf( stderr, "(#) FusionSound Error [%s]: %s\n", msg, DirectResultString( error ) );
     else
          fprintf( stderr, "(#) FusionSound Error: %s\n", DirectResultString( error ) );

     return error;
}

const char *
FusionSoundErrorString( DirectResult error )
{
     return DirectResultString( error );
}

/**************************************************************************************************/

static DFBResult
CreateRemote( const char *host, int session, IFusionSound **ret_interface )
{
     DFBResult             ret;
     DirectInterfaceFuncs *funcs;
     void                 *interface;

     D_ASSERT( host != NULL );
     D_ASSERT( ret_interface != NULL );

     ret = DirectGetInterface( &funcs, "IFusionSound", "Requestor", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface );
     if (ret)
          return ret;

     ret = funcs->Construct( interface, host, session );
     if (ret)
          return ret;

     *ret_interface = interface;

     return DFB_OK;
}

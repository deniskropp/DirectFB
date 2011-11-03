/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __VOODOO__PLAY_SERVER_H__
#define __VOODOO__PLAY_SERVER_H__

#include <voodoo/app.h>
#include <voodoo/play.h>


typedef DirectResult (*VoodooPlayerLaunchFunc)( VoodooPlayer                *player,
                                                void                        *ctx,
                                                const VoodooAppDescription  *app,
                                                const VoodooPlayInfo        *player_info,
                                                const char                  *player_addr,
                                                void                       **ret_data );

typedef DirectResult (*VoodooPlayerStopFunc)  ( VoodooPlayer               *player,
                                                void                       *ctx,
                                                void                       *data );


DirectResult voodoo_player_run_server   ( VoodooPlayer                 *player,
                                          const VoodooAppDescription   *apps,
                                          unsigned int                  num_apps,
                                          VoodooPlayerLaunchFunc        launch_func,
                                          VoodooPlayerStopFunc          stop_func,
                                          void                         *ctx );
                                                                       
DirectResult voodoo_player_get_apps     ( VoodooPlayer                 *player,
                                          unsigned int                  max_num,
                                          unsigned int                 *ret_num,
                                          VoodooAppDescription         *ret_apps );
                                                                       
DirectResult voodoo_player_launch_app   ( VoodooPlayer                 *player,
                                          const u8                      app_uuid[16],
                                          const u8                      player_uuid[16],
                                          u8                            ret_instance_uuid[16] );
                                                                       
DirectResult voodoo_player_stop_instance( VoodooPlayer                 *player,
                                          const u8                      instance_uuid[16] );
                                                                       
DirectResult voodoo_player_wait_instance( VoodooPlayer                 *player,
                                          const u8                      instance_uuid[16] );

DirectResult voodoo_player_get_instances( VoodooPlayer                 *player,
                                          unsigned int                  max_num,
                                          unsigned int                 *ret_num,
                                          VoodooAppInstanceDescription *ret_instances );


extern VoodooPlayer *voodoo_player;

#endif

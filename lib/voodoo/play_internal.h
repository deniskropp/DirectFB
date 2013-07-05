/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


#ifndef __VOODOO__PLAY_INTERNAL_H__
#define __VOODOO__PLAY_INTERNAL_H__

#include <voodoo/app.h>
#include <voodoo/play.h>
#include <voodoo/play_server.h>

#include <direct/list.h>
#include <direct/thread.h>


struct __V_VoodooPlayer {
     int                 magic;

     int                 fd;

     DirectMutex         lock;

     bool                quit;

     VoodooPlayVersion   version;
     VoodooPlayInfo      info;

     DirectThread       *thread;

     DirectLink         *nodes;

     long long           broadcast;


     VoodooServer               *server;

     const VoodooAppDescription *apps;
     unsigned int                num_apps;

     VoodooPlayerLaunchFunc      launch_func;
     VoodooPlayerStopFunc        stop_func;
     void                       *ctx;

     DirectLink                 *instances;
     DirectMutex                 instances_lock;
     DirectWaitQueue             instances_cond;
};


typedef struct {
     DirectLink            link;

     u8                    uuid[16];

     void                 *data;

     VoodooAppDescription  app;
     u8                    player_uuid[16];
} VoodooAppInstance;


extern VoodooPlayVersion g_VoodooPlay_version;
extern VoodooPlayInfo    g_VoodooPlay_info;

#endif

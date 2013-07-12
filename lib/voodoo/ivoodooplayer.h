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


#ifndef __IVOODOOPLAYER_H__
#define __IVOODOOPLAYER_H__

#include <voodoo/app.h>


D_DECLARE_INTERFACE( IVoodooPlayer );

D_DEFINE_INTERFACE( IVoodooPlayer,

     DirectResult (*GetApps)(
          IVoodooPlayer                 *thiz,
          unsigned int                   max_num,
          unsigned int                  *ret_num,
          VoodooAppDescription          *ret_applications
     );

     DirectResult (*LaunchApp)(
          IVoodooPlayer                 *thiz,
          const u8                       app_uuid[16],
          const u8                       player_uuid[16],
          u8                             ret_instance_uuid[16]
     );

     DirectResult (*StopInstance)(
          IVoodooPlayer                 *thiz,
          const u8                       instance_uuid[16]
     );

     DirectResult (*WaitInstance)(
          IVoodooPlayer                 *thiz,
          const u8                       instance_uuid[16]
     );

     DirectResult (*GetInstances)(
          IVoodooPlayer                 *thiz,
          unsigned int                   max_num,
          unsigned int                  *ret_num,
          VoodooAppInstanceDescription  *ret_instances
     );
);


DirectResult VOODOO_API VoodooPlayerCreate( const char     *host,     // FIXME: these will be a unified specifier, e.g. player UUID
                                            int             port,
                                            IVoodooPlayer **ret_interface );

#endif

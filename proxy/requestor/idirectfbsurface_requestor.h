/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __IDIRECTFBSURFACE_REQUESTOR_H__
#define __IDIRECTFBSURFACE_REQUESTOR_H__

#include <directfb.h>

#include <direct/thread.h>

#include <voodoo/manager.h>

#define IDIRECTFBSURFACE_REQUESTOR_METHOD_ID_FlipNotify     1

/*
 * private data struct of IDirectFBSurface_Requestor
 */
typedef struct {
     int                    ref;      /* reference counter */

     IDirectFB             *idirectfb;

     VoodooManager         *manager;
     VoodooInstanceID       instance;
     VoodooInstanceID       local;

     IDirectFBFont         *font;

     DFBSurfacePixelFormat  format;

     struct {
          bool                   use_notify;

          DirectMutex            lock;
          DirectWaitQueue        queue;

          unsigned int           requested;
          unsigned int           returned;

          long long              fps_stamp;
          unsigned int           fps_count;
          unsigned int           fps_old;



          bool                   use_buffer;

          IDirectFBEventBuffer  *buffer;
          IDirectFBWindow       *window;
     } flip;
} IDirectFBSurface_Requestor_data;

#endif


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

#ifndef __IFUSIONDALEMESSENGER_H__
#define __IFUSIONDALEMESSENGER_H__

#include <fusiondale.h>

#include <core/dale_types.h>

#include <direct/types.h>

/*
 * private data struct of IFusionDaleMessenger
 */
typedef struct {
     int                ref;       /* reference counter */

     CoreDale          *core;
     CoreMessenger     *messenger;
     CoreMessengerPort *port;
} IFusionDaleMessenger_data;

DFBResult IFusionDaleMessenger_Construct( IFusionDaleMessenger *thiz,
                                          CoreDale             *core,
                                          CoreMessenger        *messenger );

#endif

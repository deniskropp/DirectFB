/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef __V4L_H__
#define __V4L_H__

#include "core.h"
#include "surfaces.h"

/*
 * check for availability of video4linux
 */
DFBResult v4l_probe();

/*
 * open video4linux device and prepare for capturing
 */
DFBResult v4l_open();

/*
 * capture to the given surface and start bogus thread for callback
 * generation if callback is supplied
 */
DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                          DVFrameCallback callback, void *ctx );

/*
 * stop capturing, stop bogus thread
 */
DFBResult v4l_stop();

#endif

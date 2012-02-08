/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#ifndef __PVR2D_GFXDRIVER_H__
#define __PVR2D_GFXDRIVER_H__

#include <pvr2d.h>


typedef struct {
     int dummy;
} PVR2DDeviceData;

typedef struct {
     int                 v_flags;      // validation flags

     PVR2DBLTINFO        bltinfo;

     int                 nDevices;
     PVR2DDEVICEINFO    *pDevInfo;
     PVR2DCONTEXTHANDLE  hPVR2DContext;
     int                 nDeviceNum;
} PVR2DDriverData;


#endif

/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__OS__LINUX__GLIBC__MUTEX_H__
#define __DIRECT__OS__LINUX__GLIBC__MUTEX_H__

#include <pthread.h>

#include <direct/util.h>

/**********************************************************************************************************************/

struct __D_DirectMutex {
     pthread_mutex_t     lock;
};

/**********************************************************************************************************************/

#define DIRECT_MUTEX_INITIALIZER(name)            { PTHREAD_MUTEX_INITIALIZER }
#define DIRECT_RECURSIVE_MUTEX_INITIALIZER(name)  { PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP }

#endif


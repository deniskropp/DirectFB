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



#ifndef ___SaWMan__Debug__H___
#define ___SaWMan__Debug__H___


#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#ifdef __cplusplus
extern "C" {
#endif

#define SaWMan SaWMan_C
#include <sawman.h>
#undef SaWMan

const char *ToString_SaWManProcessFlags( SaWManProcessFlags flags );
const char *ToString_SaWManProcess( const SaWManProcess *process );


#ifdef __cplusplus
}


#include <direct/String.hxx>
#include <direct/ToString.h>


#endif // __cplusplus

#endif


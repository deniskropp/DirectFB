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



#ifndef __FUSIONDALE_COMA__POLICY_H__
#define __FUSIONDALE_COMA__POLICY_H__

#include <direct/types.h>


typedef struct {
     unsigned int   age;
     bool           allowed;
     const char    *name;
} ComaPolicy;

#define COMA_POLICY_INIT( __name )        (ComaPolicy){ .age = 0, .name = (__name) }

void coma_policy_config( const char *name, bool allow );
bool coma_policy_check ( ComaPolicy *policy );

bool coma_policy_check_manager     ( const char *manager );
bool coma_policy_check_component   ( const char *manager, const char *component );
bool coma_policy_check_method      ( const char *manager, const char *component, unsigned int method );
bool coma_policy_check_notification( const char *manager, const char *component, unsigned int notification );


#endif


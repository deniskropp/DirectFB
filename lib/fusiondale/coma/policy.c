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



#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/util.h>

#include <coma/policy.h>

#include <misc/dale_config.h>

/**********************************************************************************************************************/

typedef struct {
     DirectLink  link;
     char       *name;
     bool        allowed;
} PolicyEntry;

/**********************************************************************************************************************/

static pthread_mutex_t  policies_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int     policies_age  = 1;
static DirectLink      *policies      = NULL;

/**********************************************************************************************************************/

static PolicyEntry *
lookup_policy( const char *name, bool sub )
{
     PolicyEntry *entry;

     direct_list_foreach (entry, policies) {
          if (! strcasecmp( entry->name, name ))
               return entry;
     }

     /*
      * If the policy being registered contains a slash, but didn't exactly match an entry
      * in fusiondalerc, check to see if the policy is descended from an entry in fusiondalerc
      * (e.g. 'ui/field/messages' matches 'ui' or 'ui/field')
      */
     if (sub && strchr(name, '/')) {
          int passed_name_len = strlen( name );

          direct_list_foreach (entry, policies) {
               int entry_len = strlen( entry->name );
               if ((passed_name_len > entry_len) &&
                   (name[entry_len] == '/') &&
                   (! strncasecmp( entry->name, name, entry_len))) {
                    return entry;
               }
          }
     }

     return NULL;
}

static bool
check_policy( ComaPolicy *policy )
{
     if (policy->age != policies_age) {
          PolicyEntry *entry = lookup_policy( policy->name, true );

          policy->allowed = entry ? entry->allowed : fusiondale_config->coma_policy;
          policy->age     = policies_age;
     }

     return policy->allowed;
}

/**********************************************************************************************************************/

void
coma_policy_config( const char *name, bool allow )
{
     PolicyEntry *entry;

     pthread_mutex_lock( &policies_lock );

     entry = lookup_policy( name, false );
     if (!entry) {
          entry = calloc( 1, sizeof(PolicyEntry) );
          if (!entry) {
               D_WARN( "out of memory" );
               pthread_mutex_unlock( &policies_lock );
               return;
          }

          entry->name = strdup( name );

          direct_list_prepend( &policies, &entry->link );
     }

     entry->allowed = allow;

     if (! ++policies_age)
          policies_age++;

     pthread_mutex_unlock( &policies_lock );
}

bool
coma_policy_check( ComaPolicy *policy )
{
     bool allowed;

     pthread_mutex_lock( &policies_lock );

     allowed = check_policy( policy );

     pthread_mutex_unlock( &policies_lock );

     return allowed;
}

/**********************************************************************************************************************/

bool
coma_policy_check_manager( const char *manager )
{
     ComaPolicy policy = COMA_POLICY_INIT( manager );

     return coma_policy_check( &policy );
}

bool
coma_policy_check_component( const char *manager, const char *component )
{
     char *policy_name;

     if (asprintf( &policy_name, "%s/%s", manager, component ) < 0)
          return DR_FAILURE;

     ComaPolicy policy  = COMA_POLICY_INIT( policy_name );
     bool       allowed = coma_policy_check( &policy );

     free( policy_name );

     return allowed;
}

bool
coma_policy_check_method( const char *manager, const char *component, unsigned int method )
{
     char *policy_name;

     if (asprintf( &policy_name, "%s/%s/m%u", manager, component, method ) < 0)
          return DR_FAILURE;

     ComaPolicy policy  = COMA_POLICY_INIT( policy_name );
     bool       allowed = coma_policy_check( &policy );

     free( policy_name );

     return allowed;
}

bool
coma_policy_check_notification( const char *manager, const char *component, unsigned int notification )
{
     char *policy_name;

     if (asprintf( &policy_name, "%s/%s/n%u", manager, component, notification ) < 0)
          return DR_FAILURE;

     ComaPolicy policy  = COMA_POLICY_INIT( policy_name );
     bool       allowed = coma_policy_check( &policy );

     free( policy_name );

     return allowed;
}


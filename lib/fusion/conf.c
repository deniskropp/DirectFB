/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <stddef.h>
#include <string.h>
#include <grp.h>

#include <direct/conf.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/conf.h>


static FusionConfig config = {
     tmpfs:       NULL,
     shmfile_gid: -1
};

FusionConfig *fusion_config       = &config;
const char   *fusion_config_usage =
     "libfusion options:\n"
     "  force-slave                    Always enter as a slave, waiting for the master, if not there\n"
     "  tmpfs=<directory>              Location of shared memory file\n"
     "  shmfile-group=<groupname>      Group that owns shared memory files\n"
     "  [no-]debugshm                  Enable shared memory allocation tracking\n"
     "  [no-]madv-remove               Enable usage of MADV_REMOVE (default = auto)\n"
     "\n";

/**********************************************************************************************************************/

DirectResult
fusion_config_set( const char *name, const char *value )
{
     if (strcmp (name, "tmpfs" ) == 0) {
          if (value) {
               if (fusion_config->tmpfs)
                    D_FREE( fusion_config->tmpfs );
               fusion_config->tmpfs = D_STRDUP( value );
          }
          else {
               D_ERROR("Fusion/Config 'tmpfs': No directory specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "shmfile-group" ) == 0) {
          if (value) {
               struct group *group_info;
               
               group_info = getgrnam( value );
               if (group_info)
                    fusion_config->shmfile_gid = group_info->gr_gid;
               else
                    D_PERROR("Fusion/Config 'shmfile-group': Group '%s' not found!\n", value);
          }
          else {
               D_ERROR("Fusion/Config 'shmfile-group': No file group name specified!\n");
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "force-slave" ) == 0) {
          fusion_config->force_slave = true;
     } else
     if (strcmp (name, "no-force-slave" ) == 0) {
          fusion_config->force_slave = false;
     } else
     if (strcmp (name, "debugshm" ) == 0) {
          fusion_config->debugshm = true;
     } else
     if (strcmp (name, "no-debugshm" ) == 0) {
          fusion_config->debugshm = false;
     } else
     if (strcmp (name, "madv-remove" ) == 0) {
          fusion_config->madv_remove       = true;
          fusion_config->madv_remove_force = true;
     } else
     if (strcmp (name, "no-madv-remove" ) == 0) {
          fusion_config->madv_remove       = false;
          fusion_config->madv_remove_force = true;
     } else
     if (direct_config_set( name, value ))
          return DFB_UNSUPPORTED;

     return DFB_OK;
}


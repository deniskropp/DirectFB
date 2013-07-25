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

#include <direct/Base.h>
#include <direct/direct.h>
#include <direct/log.h>
#include <direct/log_domain.h>
#include <direct/init.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/perf.h>
#include <direct/result.h>
#include <direct/thread.h>
#include <direct/util.h>


void DirectResult__init( void );
void DirectResult__deinit( void );

D_DEBUG_DOMAIN( Direct_Init, "Direct/Init", "Direct Init" );

/**********************************************************************************************************************/

typedef void (*Func)( void );


static Func init_funcs[] = {
     __D_conf_init,
     __D_direct_init,
     __D_util_init,
     __D_result_init,
     DirectResult__init,
     __D_mem_init,
     __D_thread_init,
     __D_log_init,
     __D_log_domain_init,
     __D_perf_init,
     __D_interface_init,
     __D_interface_dbg_init,
     __D_base_init,
};

static Func deinit_funcs[] = {
     __D_base_deinit,
     __D_interface_dbg_deinit,
     __D_interface_deinit,
     __D_log_domain_deinit,
     __D_perf_deinit,
     __D_thread_deinit,
     __D_mem_deinit,
     DirectResult__deinit,
     __D_result_deinit,
     __D_util_deinit,
     __D_direct_deinit,
     __D_log_deinit,
     __D_conf_deinit,
};

/**********************************************************************************************************************/

void
__D_init_all()
{
     size_t i;

     D_DEBUG_AT( Direct_Init, "%s()\n", __FUNCTION__ );

     for (i=0; i<D_ARRAY_SIZE(init_funcs); i++)
          init_funcs[i]();
}

void
__D_deinit_all()
{
     size_t i;

     D_DEBUG_AT( Direct_Init, "%s()\n", __FUNCTION__ );

     for (i=0; i<D_ARRAY_SIZE(deinit_funcs); i++)
          deinit_funcs[i]();
}

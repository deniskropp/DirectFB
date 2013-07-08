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



#ifndef __VOODOO__LINK_H__
#define __VOODOO__LINK_H__

#include <voodoo/types.h>


typedef struct {
     void   *ptr;
     size_t  length;
     size_t  done;
} VoodooChunk;


struct __V_VoodooLink {
     void *priv;
     u32   code;

     void    (*Close)( VoodooLink *link );

     /* See 'read(2)', blocking */
     ssize_t (*Read) ( VoodooLink *link,
                       void       *buffer,
                       size_t      count );

     /* See 'write(2)', blocking */
     ssize_t (*Write)( VoodooLink *link,
                       const void *buffer,
                       size_t      count );


     /* For later... */
     DirectResult (*SendReceive)( VoodooLink  *link,
                                  VoodooChunk *send,
                                  size_t       num_send,
                                  VoodooChunk *recv,
                                  size_t       num_recv );

     DirectResult (*WakeUp)     ( VoodooLink  *link );

     DirectResult (*WaitForData)( VoodooLink  *link,
                                  int          timeout_ms );
};


DirectResult VOODOO_API voodoo_link_init_connect( VoodooLink *link,
                                                  const char *hostname,
                                                  int         port,
                                                  bool        raw );

DirectResult VOODOO_API voodoo_link_init_local  ( VoodooLink *link,
                                                  const char *path,
                                                  bool        raw );

DirectResult VOODOO_API voodoo_link_init_fd     ( VoodooLink *link,
                                                  int         fd[2] );

#endif

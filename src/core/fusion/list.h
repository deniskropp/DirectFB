/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
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

#ifndef __FUSION__LIST_H__
#define __FUSION__LIST_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "fusion_types.h"

  struct _FusionLink {
    FusionLink *next;
    FusionLink *prev;
  };

  void fusion_list_prepend (FusionLink **list, FusionLink *link);
  void fusion_list_remove  (FusionLink **list, FusionLink *link);

#define fusion_list_foreach(link, list)  for (link = list; link; link = link->next)

#ifdef __cplusplus
}
#endif

#endif /* __FUSION__LIST_H__ */


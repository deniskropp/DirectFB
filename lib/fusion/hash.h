/*
    GLIB - Library of useful routines for C programming
    Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.
 
   All rights reserved.
 
   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.
              Michael Emmel <memmel@gmail.com>.
 
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __FUSION_HASH_H__
#define __FUSION_HASH_H__

#include <fusion/types.h>
#include <fusion/shmalloc.h>
#include <string.h>

#define FUSION_HASH_MIN_SIZE 11
#define FUSION_HASH_MAX_SIZE 13845163

#define HASH_STR(h,p) \
{\
    h = *p;\
    if (h)\
        for (p += 1; *p != '\0'; p++)\
            h = (h << 5) - h + *p;\
}\

typedef enum {
HASH_PTR,
HASH_STRING,
HASH_INT
}
FusionHashType;

typedef struct _FusionHashNode  FusionHashNode;

struct _FusionHashNode
{
    void      *key;
    void      *value;
    FusionHashNode *next;
};

struct __Fusion_FusionHash
{
    int       magic;
    bool      local;
    FusionHashType key_type;
    FusionHashType value_type;
    int             size;
    int             nnodes;
    FusionHashNode      **nodes;
    FusionSHMPoolShared *pool;
};

typedef bool (*FusionHashIteratorFunc)( FusionHash *hash,
                                        void       *key,
                                        void       *value,
                                        void       *ctx );


DirectResult
fusion_hash_resize (FusionHash  *hash);

DirectResult
fusion_hash_create (FusionSHMPoolShared *pool,
                    FusionHashType key_type,
                    FusionHashType value_type,
                    int  size, FusionHash **ret_hash );

DirectResult
fusion_hash_create_local (FusionHashType key_type, FusionHashType value_type,
                          int  size, FusionHash **ret_hash );

DirectResult
fusion_hash_remove (FusionHash    *hash,
                    const void *  key,
                    void **old_key,
                    void **old_value);

DirectResult
fusion_hash_insert( FusionHash *hash, void  *key, void  *value );

DirectResult
fusion_hash_replace (FusionHash *hash,
                     void *   key, 
                     void *   value,
                     void **old_key,
                     void **old_value);
void
fusion_hash_destroy( FusionHash *hash );

void *
fusion_hash_lookup (FusionHash *hash, const void * key);

void
fusion_hash_iterate( FusionHash             *hash,
                     FusionHashIteratorFunc  func,
                     void                   *ctx );

bool fusion_hash_should_resize ( FusionHash    *hash);


static inline FusionHashNode**
fusion_hash_lookup_node (FusionHash *hash,
              const void *   key)
{
  FusionHashNode **node;

  /*TODO We could also optimize pointer hashing*/
  if (hash->key_type == HASH_STRING )
  {
    unsigned int h;
    const signed char *p = key;
    HASH_STR(h,p)
    node = &hash->nodes[h % hash->size];
  }
  else
    node = &hash->nodes[((unsigned int)key) % hash->size];

    /* Hash table lookup needs to be fast.
     *  We therefore remove the extra conditional of testing
     *  whether to call the key_equal_func or not from
     *  the inner loop.
     */
    if (hash->key_type == HASH_STRING ) {
        while(*node && strcmp((const char *)(*node)->key,(const char*)key))
            node = &(*node)->next;
    }
    else
        while (*node && (*node)->key != key)
            node = &(*node)->next;

  return node;

}



#endif /*__FUSION_HASH_H__*/


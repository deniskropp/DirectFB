/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__DEBUG_H__
#define __ONE__DEBUG_H__

#include <linux/kernel.h>

#define ONE_ASSERT(exp)    do { if (!(exp)) { printk( KERN_ERR "linux-one: Assertion [ " #exp " ] failed! (%s:%d)\n", __FILE__, __LINE__ ); *(char*) 0 = 0; } } while (0)
#define ONE_ASSUME(exp)    do { if (!(exp)) printk( KERN_ERR "linux-one: Assumption [ " #exp " ] failed! (%s:%d)\n", __FILE__, __LINE__ ); } while (0)

#ifdef ONE_ENABLE_DEBUG
#define ONE_DEBUG(x...)  one_debug_printk (x)
#else
#define ONE_DEBUG(x...)  do {} while (0)
#endif

void one_debug_printk( const char *format, ... );


#define D_ASSERT ONE_ASSERT
#define D_ASSUME ONE_ASSUME


/*
 * Magic Assertions & Utilities
 */

#define D_MAGIC(spell)             ( (((spell)[sizeof(spell)*8/9] << 24) | \
                                      ((spell)[sizeof(spell)*7/9] << 16) | \
                                      ((spell)[sizeof(spell)*6/9] <<  8) | \
                                      ((spell)[sizeof(spell)*5/9]      )) ^  \
                                     (((spell)[sizeof(spell)*4/9] << 24) | \
                                      ((spell)[sizeof(spell)*3/9] << 16) | \
                                      ((spell)[sizeof(spell)*2/9] <<  8) | \
                                      ((spell)[sizeof(spell)*1/9]      )) )


#define D_MAGIC_CHECK(o,m)         ((o) != NULL && (o)->magic == D_MAGIC(#m))

#define D_MAGIC_SET(o,m)           do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSUME( (o)->magic != D_MAGIC(#m) );       \
                                                                                     \
                                        (o)->magic = D_MAGIC(#m);                    \
                                   } while (0)

#define D_MAGIC_SET_ONLY(o,m)      do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                                                                     \
                                        (o)->magic = D_MAGIC(#m);                    \
                                   } while (0)

#define D_MAGIC_ASSERT(o,m)        do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSERT( (o)->magic == D_MAGIC(#m) );       \
                                   } while (0)

#define D_MAGIC_ASSUME(o,m)        do {                                              \
                                        D_ASSUME( (o) != NULL );                     \
                                        if (o)                                       \
                                             D_ASSUME( (o)->magic == D_MAGIC(#m) );  \
                                   } while (0)

#define D_MAGIC_ASSERT_IF(o,m)     do {                                              \
                                        if (o)                                       \
                                             D_ASSERT( (o)->magic == D_MAGIC(#m) );  \
                                   } while (0)

#define D_MAGIC_CLEAR(o)           do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSUME( (o)->magic != 0 );                 \
                                                                                     \
                                        (o)->magic = 0;                              \
                                   } while (0)

#endif

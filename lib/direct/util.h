/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __DIRECT__UTIL_H__
#define __DIRECT__UTIL_H__

#include <direct/types.h>
#include <direct/messages.h>

#include <pthread.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef SIGN
#define SIGN(x)  (((x) < 0) ?  -1  :  (((x) > 0) ? 1 : 0))
#endif

#ifndef ABS
#define ABS(x)   ((x) > 0 ? (x) : -(x))
#endif

#ifndef CLAMP
#define CLAMP(x,min,max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#endif


#define D_FLAGS_SET(flags,f)       do { (flags) |= (f); } while (0)
#define D_FLAGS_CLEAR(flags,f)     do { (flags) &= ~(f); } while (0)
#define D_FLAGS_IS_SET(flags,f)    (((flags) & (f)) != 0)
#define D_FLAGS_ARE_SET(flags,f)   (((flags) & (f)) == (f))
#define D_FLAGS_ARE_IN(flags,f)    (((flags) & ~(f)) == 0)
#define D_FLAGS_INVALID(flags,f)   (((flags) & ~(f)) != 0)

#define D_ARRAY_SIZE(array)        ((int)(sizeof(array) / sizeof((array)[0])))


#if __GNUC__ >= 3
#define D_CONST_FUNC               __attribute__((const))
#else
#define D_CONST_FUNC
#endif


/*
 * translates errno to DirectResult
 */
DirectResult errno2result( int erno );

const char *DirectResultString( DirectResult result );

int direct_try_open( const char *name1, const char *name2, int flags, bool error_msg );

void direct_trim( char **s );

/*
 * Slow implementation, but quite fast if only low bits are set.
 */
static inline int
direct_util_count_bits( unsigned int mask )
{
     register int ret = 0;

     while (mask) {
          ret += mask & 1;
          mask >>= 1;
     }

     return ret;
}

/*
 * Utility function to initialize recursive mutexes.
 */
static inline int
direct_util_recursive_pthread_mutex_init( pthread_mutex_t *mutex )
{
     int                 ret;
     pthread_mutexattr_t attr;

     pthread_mutexattr_init( &attr );
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

     ret = pthread_mutex_init( mutex, &attr );
     if (ret)
          D_PERROR( "Fusion/Lock: Could not initialize recursive mutex!\n" );

     pthread_mutexattr_destroy( &attr );

     return ret;
}

#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#else
#warning PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP is not defined, be aware of dead locks
#define DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER
#endif

/* floor and ceil implementation to get rid of libm */

/*
 IEEE floor for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static inline int D_IFLOOR(float f)
{
        int ai, bi;
        double af, bf;

        af = (3 << 22) + 0.5 + (double)f;
        bf = (3 << 22) + 0.5 - (double)f;

#if defined(__GNUC__) && defined(__i386__)
        /*
         GCC generates an extra fstp/fld without this.
        */
        __asm__ __volatile__ ("fstps %0" : "=m" (ai) : "t" (af) : "st");
        __asm__ __volatile__ ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
#else
        {
                union { int i; float f; } u;
                u.f = af; ai = u.i;
                u.f = bf; bi = u.i;
        }
#endif

        return (ai - bi) >> 1;
}


/*
 IEEE ceil for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This ceil operation is done by "(iround(f + .5) + iround(f - .5) + 1) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static inline int D_ICEIL(float f)
{
        int ai, bi;
        double af, bf;

        af = (3 << 22) + 0.5 + (double)f;
        bf = (3 << 22) + 0.5 - (double)f;

#if defined(__GNUC__) && defined(__i386__)
        /*
         GCC generates an extra fstp/fld without this.
        */
        __asm__ __volatile__ ("fstps %0" : "=m" (ai) : "t" (af) : "st");
        __asm__ __volatile__ ("fstps %0" : "=m" (bi) : "t" (bf) : "st");
#else
        {
                union { int i; float f; } u;
                u.f = af; ai = u.i;
                u.f = bf; bi = u.i;
        }
#endif

        return (ai - bi + 1) >> 1;
}

static inline int direct_log2( int val )
{
     register int ret = 0;

     while (val >> ++ret);

     if ((1 << --ret) < val)
          ret++;

     return ret;
}

#endif

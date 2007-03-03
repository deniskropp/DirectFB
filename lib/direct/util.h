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

#ifndef __DIRECT__UTIL_H__
#define __DIRECT__UTIL_H__

#include <direct/debug.h>
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

#ifndef BSWAP16
#define BSWAP16(x) (((u16)(x)>>8) | ((u16)(x)<<8))
#endif

#ifndef BSWAP32
#define BSWAP32(x) ((((u32)(x)>>24) & 0x000000ff) | (((u32)(x)>> 8) & 0x0000ff00) | \
                    (((u32)(x)<< 8) & 0x00ff0000) | (((u32)(x)<<24) & 0xff000000))
#endif


#define D_FLAGS_SET(flags,f)       do { (flags) |= (f); } while (0)
#define D_FLAGS_CLEAR(flags,f)     do { (flags) &= ~(f); } while (0)
#define D_FLAGS_IS_SET(flags,f)    (((flags) & (f)) != 0)
#define D_FLAGS_ARE_SET(flags,f)   (((flags) & (f)) == (f))
#define D_FLAGS_ARE_IN(flags,f)    (((flags) & ~(f)) == 0)
#define D_FLAGS_INVALID(flags,f)   (((flags) & ~(f)) != 0)

#define D_FLAGS_ASSERT(flags,f)    D_ASSERT( D_FLAGS_ARE_IN(flags,f) )

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

/*
 * duplicates a file descriptor as needed to ensure it's not stdin, stdout, or stderr
 */
int direct_safe_dup( int fd );

int direct_try_open( const char *name1, const char *name2, int flags, bool error_msg );

void direct_trim( char **s );

/*
 * Set a string with a maximum size including the zero termination.
 *
 * This acts like a strncpy(d,s,n), but always terminates the string like snprintf(d,n,"%s",s).
 *
 * Returns dest or NULL if n is zero.
 */
static __inline__ char *
direct_snputs( char       *dest,
               const char *src,
               size_t      n )
{
     char *start = dest;

     D_ASSERT( dest != NULL );
     D_ASSERT( src != NULL );

     if (!n)
          return NULL;

     for (; n>1 && *src; n--)
          *dest++ = *src++;

     *dest = 0;

     return start;
}

/*
 * Encode/Decode Base-64 strings.
 */
char *direct_base64_encode( const void *data, int size );
void *direct_base64_decode( const char *string, int *ret_size );

/*
 * Compute MD5 sum (store 16-bytes long result in "dst").
 */
void  direct_md5_sum( void *dst, const void *src, const int len );

/*
 * Slow implementation, but quite fast if only low bits are set.
 */
static __inline__ int
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
 * Generic alignment routine.
 */
static __inline__ int
direct_util_align( int value,
                   int alignment )
{
     if (alignment > 1) {
          int tail = value % alignment;

          if (tail)
               value += alignment - tail;
     }

     return value;
}

/*
 * Utility function to initialize recursive mutexes.
 */
int direct_util_recursive_pthread_mutex_init( pthread_mutex_t *mutex );


/* floor and ceil implementation to get rid of libm */

/*
 IEEE floor for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static __inline__ int D_IFLOOR(float f)
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
static __inline__ int D_ICEIL(float f)
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

static __inline__ int direct_log2( int val )
{
     register int ret = 0;

     while (val >> ++ret);

     if ((1 << --ret) < val)
          ret++;

     return ret;
}

#endif

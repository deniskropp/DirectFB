/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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


#include <direct/clock.h>
#include <direct/messages.h>
#include <direct/print.h>

#ifndef DIRECT_DISABLE_DEPRECATED
#include <pthread.h>
#endif

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

#ifdef __GNUC__
#define D_FLAGS_SET(flags,f)       do { (flags) = (__typeof__(flags))((flags) |  (f)); } while (0)
#define D_FLAGS_CLEAR(flags,f)     do { (flags) = (__typeof__(flags))((flags) & ~(f)); } while (0)
#else
#define D_FLAGS_SET(flags,f)       do { (flags) = (unsigned int)((flags) |  (f)); } while (0)
#define D_FLAGS_CLEAR(flags,f)     do { (flags) = (unsigned int)((flags) & ~(f)); } while (0)
#endif

#define D_FLAGS_IS_SET(flags,f)    (((flags) & (f)) != 0)
#define D_FLAGS_ARE_SET(flags,f)   (((flags) & (f)) == (f))
#define D_FLAGS_ARE_IN(flags,f)    (((flags) & ~(f)) == 0)
#define D_FLAGS_INVALID(flags,f)   (((flags) & ~(f)) != 0)

#define D_ARRAY_SIZE(array)        ((int)(sizeof(array) / sizeof((array)[0])))


#define D_UTIL_SWAP(a,b)                                                   \
     do {                                                                  \
          const typeof(a) __swap_x = (a); (a) = (b); (b) = __swap_x;       \
     } while (0)


#if __GNUC__ >= 3
#define D_CONST_FUNC               __attribute__((const))
#else
#define D_CONST_FUNC
#endif

#if __GNUC__ >= 3
#define D_FORMAT_PRINTF(n)         __attribute__((__format__ (__printf__, n, n+1)))
#define D_FORMAT_VPRINTF(n)        __attribute__((__format__ (__printf__, n, 0)))
#else
#define D_FORMAT_PRINTF(n)
#define D_FORMAT_VPRINTF(n)
#endif


#define D_BITn32(f)  (((f) & 0x00000001) ?  0 : \
                      ((f) & 0x00000002) ?  1 : \
                      ((f) & 0x00000004) ?  2 : \
                      ((f) & 0x00000008) ?  3 : \
                      ((f) & 0x00000010) ?  4 : \
                      ((f) & 0x00000020) ?  5 : \
                      ((f) & 0x00000040) ?  6 : \
                      ((f) & 0x00000080) ?  7 : \
                      ((f) & 0x00000100) ?  8 : \
                      ((f) & 0x00000200) ?  9 : \
                      ((f) & 0x00000400) ? 10 : \
                      ((f) & 0x00000800) ? 11 : \
                      ((f) & 0x00001000) ? 12 : \
                      ((f) & 0x00002000) ? 13 : \
                      ((f) & 0x00004000) ? 14 : \
                      ((f) & 0x00008000) ? 15 : \
                      ((f) & 0x00010000) ? 16 : \
                      ((f) & 0x00020000) ? 17 : \
                      ((f) & 0x00040000) ? 18 : \
                      ((f) & 0x00080000) ? 19 : \
                      ((f) & 0x00100000) ? 20 : \
                      ((f) & 0x00200000) ? 21 : \
                      ((f) & 0x00400000) ? 22 : \
                      ((f) & 0x00800000) ? 23 : \
                      ((f) & 0x01000000) ? 24 : \
                      ((f) & 0x02000000) ? 25 : \
                      ((f) & 0x04000000) ? 26 : \
                      ((f) & 0x08000000) ? 27 : \
                      ((f) & 0x10000000) ? 28 : \
                      ((f) & 0x20000000) ? 29 : \
                      ((f) & 0x40000000) ? 30 : \
                      ((f) & 0x80000000) ? 31 : -1)

#define D_UNUSED_P(x) (void)x

/*
 * portable sched_yield() implementation
 */
#ifdef _POSIX_PRIORITY_SCHEDULING
#define direct_sched_yield()  sched_yield()
#else
#define direct_sched_yield()  usleep(1)
#endif

/*
 * translates errno to DirectResult
 */
DirectResult DIRECT_API errno2result( int erno );

/*
 * duplicates a file descriptor as needed to ensure it's not stdin, stdout, or stderr
 */
int DIRECT_API direct_safe_dup( int fd );


#ifndef DIRECT_DISABLE_DEPRECATED

// @deprecated
int DIRECT_API direct_try_open( const char *name1, const char *name2, int flags, bool error_msg );

// @deprecated
int DIRECT_API direct_util_recursive_pthread_mutex_init( pthread_mutex_t *mutex );

#endif


const char DIRECT_API *direct_inet_ntop( int af, const void* src, char* dst, int cnt );

int DIRECT_API direct_sscanf( const char *str, const char *format, ... );
int DIRECT_API direct_vsscanf( const char *str, const char *format, va_list args );

size_t DIRECT_API direct_strlen( const char *s );

void DIRECT_API direct_trim( char **s );

int DIRECT_API direct_strcmp( const char *a, const char *b );
int DIRECT_API direct_strcasecmp( const char *a, const char *b );
int DIRECT_API direct_strncasecmp( const char *a, const char *b, size_t bytes );

unsigned long int DIRECT_API direct_strtoul( const char *nptr, char **endptr, int base );

char DIRECT_API *direct_strtok_r( char *str, const char *delim, char **saveptr );

const char DIRECT_API *direct_strerror( int erno );

/*
 * Set a string with a maximum size including the zero termination.
 *
 * This acts like a strncpy(d,s,n), but always terminates the string like direct_snprintf(d,n,"%s",s).
 *
 * Returns dest or NULL if n is zero.
 */
char DIRECT_API *direct_snputs( char       *dest,
                                const char *src,
                                size_t      n );

/*
 * Encode/Decode Base-64 strings.
 */
char DIRECT_API *direct_base64_encode( const void *data, int size );
void DIRECT_API *direct_base64_decode( const char *string, int *ret_size );

/*
 * Compute MD5 sum (store 16-bytes long result in "dst").
 */
void DIRECT_API direct_md5_sum( void *dst, const void *src, const int len );

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

void DIRECT_API *
direct_bsearch( const void *key, const void *base,
		      size_t nmemb, size_t size, void *func );

/* floor and ceil implementation to get rid of libm */

/*
 IEEE floor for computers that round to nearest or even.

 'f' must be between -4194304 and 4194303.

 This floor operation is done by "(iround(f + .5) + iround(f - .5)) >> 1",
 but uses some IEEE specific tricks for better speed.
*/
static __inline__ int
D_IFLOOR(float f)
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
                u.f = (float) af; ai = u.i;
                u.f = (float) bf; bi = u.i;
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
static __inline__ int
D_ICEIL(float f)
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
                u.f = (float) af; ai = u.i;
                u.f = (float) bf; bi = u.i;
        }
#endif

        return (ai - bi + 1) >> 1;
}

static __inline__ int
direct_log2( int val )
{
     register int ret = 0;

     while (val >> ++ret);

     if ((1 << --ret) < val)
          ret++;

     return ret;
}


typedef struct {
     long long start;
     long long stop;
} DirectClock;

static __inline__ void direct_clock_start( DirectClock *clock ) {
     clock->start = direct_clock_get_micros();
}

static __inline__ void direct_clock_stop( DirectClock *clock ) {
     clock->stop = direct_clock_get_micros();
}

static __inline__ long long direct_clock_diff( DirectClock *clock ) {
     return clock->stop - clock->start;
}

#define DIRECT_CLOCK_DIFF_SEC_MS( clock ) \
     (direct_clock_diff( clock ) / 1000000), (direct_clock_diff( clock ) / 1000 % 1000)

#define DIRECT_CLOCK_ITEMS_Mk_SEC( clock, n ) \
     ((n) / direct_clock_diff( clock )), ((long)(((n) * 1000LL / direct_clock_diff( clock ) % 1000)))


void __D_util_init( void );
void __D_util_deinit( void );

#endif

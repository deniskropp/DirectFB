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

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

/*
 * translates errno to DirectResult
 */
DirectResult
errno2result( int erno )
{
     switch (erno) {
          case 0:
               return DFB_OK;
          case ENOENT:
               return DFB_FILENOTFOUND;
          case EACCES:
          case EPERM:
               return DFB_ACCESSDENIED;
          case EBUSY:
          case EAGAIN:
               return DFB_BUSY;
          case ECONNREFUSED:
               return DFB_ACCESSDENIED;
          case ENODEV:
          case ENXIO:
#ifdef ENOTSUP
          /* ENOTSUP is not defined on NetBSD */
          case ENOTSUP:
#endif
               return DFB_UNSUPPORTED;
     }

     return DFB_FAILURE;
}

const char *
DirectResultString( DirectResult result )
{
     switch (result) {
          case DFB_OK:
               return "OK";
          case DFB_FAILURE:
               return "General failure!";
          case DFB_INIT:
               return "Initialization error!";
          case DFB_BUG:
               return "Internal bug!";
          case DFB_DEAD:
               return "Interface was released!";
          case DFB_UNSUPPORTED:
               return "Not supported!";
          case DFB_UNIMPLEMENTED:
               return "Not implemented!";
          case DFB_ACCESSDENIED:
               return "Access denied!";
          case DFB_INVARG:
               return "Invalid argument!";
          case DFB_NOSYSTEMMEMORY:
               return "Out of memory!";
          case DFB_NOVIDEOMEMORY:
               return "Out of video memory!";
          case DFB_LOCKED:
               return "Resource is locked!";
          case DFB_BUFFEREMPTY:
               return "Buffer is empty!";
          case DFB_FILENOTFOUND:
               return "File not found!";
          case DFB_IO:
               return "General I/O error!";
          case DFB_NOIMPL:
               return "No (suitable) implementation found!";
          case DFB_MISSINGFONT:
               return "No font has been set!";
          case DFB_TIMEOUT:
               return "Operation timed out!";
          case DFB_MISSINGIMAGE:
               return "No image has been set!";
          case DFB_BUSY:
               return "Resource is busy!";
          case DFB_THIZNULL:
               return "'thiz' argument is NULL!";
          case DFB_IDNOTFOUND:
               return "Requested ID not found!";
          case DFB_INVAREA:
               return "Invalid area present!";
          case DFB_DESTROYED:
               return "Resource was destroyed!";
          case DFB_FUSION:
               return "Fusion IPC error detected!";
          case DFB_BUFFERTOOLARGE:
               return "Buffer is too large!";
          case DFB_INTERRUPTED:
               return "Operation has been interrupted!";
          case DFB_NOCONTEXT:
               return "No context available!";
          case DFB_TEMPUNAVAIL:
               return "Resource temporarily unavailable!";
          case DFB_LIMITEXCEEDED:
               return "Limit has been exceeded!";
          case DFB_NOSUCHMETHOD:
               return "No such (remote) method!";
          case DFB_NOSUCHINSTANCE:
               return "No such (remote) instance!";
          case DFB_ITEMNOTFOUND:
               return "Appropriate item not found!";
          case DFB_VERSIONMISMATCH:
               return "Some versions didn't match!";
          case DFB_NOSHAREDMEMORY:
               return "Out of shared memory!";
          case DFB_EOF:
               return "End of file!";
          case DFB_SUSPENDED:
               return "Object is suspended!";
          case DFB_INCOMPLETE:
               return "Operation incomplete!";
          case DFB_NOCORE:
               return "No core (loaded)!";
     }

     return "UNKNOWN RESULT CODE!";
}

int
direct_safe_dup( int fd )
{
    int n = 0;
    int fc[3];

    while (fd >= 0 && fd <= 2) {
        fc[n++] = fd;
        fd = dup (fd);
    }

    while (n)
        close (fc[--n]);

    return fd;
}

int
direct_try_open( const char *name1, const char *name2, int flags, bool error_msg )
{
     int fd;

     fd = open (name1, flags);
     if (fd >= 0)
          return direct_safe_dup (fd);

     if (errno != ENOENT) {
          if (error_msg)
               D_PERROR( "Direct/Util: opening '%s' failed\n", name1 );
          return -1;
     }

     fd = open (name2, flags);
     if (fd >= 0)
          return direct_safe_dup (fd);

     if (error_msg) {
          if (errno == ENOENT)
               D_PERROR( "Direct/Util: opening '%s' and '%s' failed\n", name1, name2 );
          else
               D_PERROR( "Direct/Util: opening '%s' failed\n", name2 );
     }

     return -1;
}

void
direct_trim( char **s )
{
     int i;
     int len = strlen( *s );

     for (i = len-1; i >= 0; i--)
          if ((*s)[i] <= ' ')
               (*s)[i] = 0;
          else
               break;

     while (**s)
          if (**s <= ' ')
               (*s)++;
          else
               return;
}

/*
 * Utility function to initialize recursive mutexes.
 */
int
direct_util_recursive_pthread_mutex_init( pthread_mutex_t *mutex )
{
     int                 ret;
     pthread_mutexattr_t attr;

     pthread_mutexattr_init( &attr );
#if HAVE_DECL_PTHREAD_MUTEX_RECURSIVE
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
#endif
     ret = pthread_mutex_init( mutex, &attr );
     if (ret)
          D_PERROR( "Direct/Lock: Could not initialize recursive mutex!\n" );

     pthread_mutexattr_destroy( &attr );

     return ret;
}

/*
 * Encode/Decode Base-64.
 */
char*
direct_base64_encode( const void *data, int size )
{
     static char   *enc = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "abcdefghijklmnopqrstuvwxyz"
                          "0123456789+/=";
     unsigned char *src = (unsigned char*)data;
     char          *ret;
     char          *buf;
     
     D_ASSERT( data != NULL );

     buf = ret = D_MALLOC( (size + 2) / 3 * 4 + 1 );
     if (!ret)
          return NULL;
     
     for (; size >= 3; size -= 3) {
          buf[0] = enc[((src[0] & 0xfc) >> 2)];
          buf[1] = enc[((src[0] & 0x03) << 4) | ((src[1] & 0xf0) >> 4)];
          buf[2] = enc[((src[1] & 0x0f) << 2) | ((src[2] & 0xc0) >> 6)];
          buf[3] = enc[((src[2] & 0x3f))];
          buf += 4;
          src += 3;
     }

     if (size > 0) {
          buf[0] = enc[(src[0] & 0xfc) >> 2];
          
          if (size > 1) {
               buf[1] = enc[((src[0] & 0x03) << 4) | ((src[1] & 0xf0) >> 4)];
               buf[2] = enc[((src[1] & 0x0f) << 2)];
          } else {
               buf[1] = enc[(src[0] & 0x03) << 4];
               buf[2] = '=';
          }

          buf[3] = '=';
          buf += 4;
     }

     *buf = '\0';

     return ret;
}

void*
direct_base64_decode( const char *string, int *ret_size )
{
     unsigned char  dec[256];
     unsigned char *ret;
     unsigned char *buf;
     int            len;
     int            i, j;
     
     D_ASSERT( string != NULL );
     
     len = strlen( string );
     buf = ret = D_MALLOC( len * 3 / 4 + 3 );
     if (!ret)
          return NULL;

     /* generate decode table */
     for (i = 0; i < 255; i++)
          dec[i] = 0x80;
     for (i = 'A'; i <= 'Z'; i++)
          dec[i] = 0  + (i - 'A');
     for (i = 'a'; i <= 'z'; i++)
          dec[i] = 26 + (i - 'a');
     for (i = '0'; i <= '9'; i++)
          dec[i] = 52 + (i - '0');
     dec['+'] = 62;
     dec['/'] = 63;
     dec['='] = 0;
  
     /* decode */
     for (j = 0; j < len; j += 4) {
          unsigned char a[4], b[4];

          for (i = 0; i < 4; i++) {
               int c = string[i+j];
               a[i] = c;
               b[i] = dec[c];
          }
    
          *buf++ = (b[0] << 2) | (b[1] >> 4);
          *buf++ = (b[1] << 4) | (b[2] >> 2);
          *buf++ = (b[2] << 6) | (b[3]     );
          if (a[2] == '=' || a[3] == '=')
               break;
     }

     *buf = '\0';
     
     if (ret_size)
          *ret_size = buf - ret;

     return ret;
}

/*
 * Compute MD5 sum.
 */
static const u8 S[4][4] = {
     { 7, 12, 17, 22 },  /* Round 1 */
     { 5,  9, 14, 20 },  /* Round 2 */
     { 4, 11, 16, 23 },  /* Round 3 */
     { 6, 10, 15, 21 }   /* Round 4 */
};

static const u32 T[64] = {
     0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,   /* Round 1 */
     0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
     0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
     0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,

     0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,   /* Round 2 */
     0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
     0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
     0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,

     0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,   /* Round 3 */
     0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
     0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
     0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,

     0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,   /* Round 4 */
     0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
     0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
     0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
                
static void
md5_hash( u32 ABCD[4], u32 X[16] )
{
     u32 a = ABCD[3];
     u32 b = ABCD[2];
     u32 c = ABCD[1];
     u32 d = ABCD[0];
     int   t;
     int   i;
    
#ifdef WORDS_BIGENDIAN
     for (i = 0; i < 16; i++)
          X[i] = BSWAP32(X[i]);
#endif

     for (i = 0; i < 64; i++) {
          t = S[i>>4][i&3];
          a += T[i];
          switch (i>>4) {
               case 0: a += (d ^ (b&(c^d))) + X[     i &15]; break;
               case 1: a += (c ^ (d&(c^b))) + X[(1+5*i)&15]; break;
               case 2: a += (b^c^d)         + X[(5+3*i)&15]; break;
               case 3: a += (c^(b|~d))      + X[(  7*i)&15]; break;
          }
          a = b + ((a << t) | (a >> (32 - t)));
          t = d; d = c; c = b; b = a; a = t;
     }
     
     ABCD[0] += d;
     ABCD[1] += c;
     ABCD[2] += b;
     ABCD[3] += a;
}         

void 
direct_md5_sum( void *dst, const void *src, const int len )
{
     u8    block[64];
     u32 ABCD[4];
     int   i, j;
     
     D_ASSERT( dst != NULL );
     D_ASSERT( src != NULL );
     
     ABCD[0] = 0x10325476;
     ABCD[1] = 0x98badcfe;
     ABCD[2] = 0xefcdab89;
     ABCD[3] = 0x67452301;
     
     for (i = 0, j = 0; i < len; i++) {
          block[j++] = ((u8*)src)[i];
          if (j == 64) {
               md5_hash( ABCD, (u32*)block );
               j = 0;
          }
     }
     
     block[j++] = 0x80;
     memset( &block[j], 0, 64-j );
     
     if (j > 56) {
          md5_hash( ABCD, (u32*)block );
          memset( block, 0, 64 );
     }
     
     for (i = 0; i < 8; i++)
          block[56+i] = ((u64)len << 3) >> (i << 3);
          
     md5_hash( ABCD, (u32*)block );
     
     for (i = 0; i < 4; i++)
#ifdef WORDS_BIGENDIAN
          ((u32*)dst)[i] = BSWAP32(ABCD[3-i]);
#else
          ((u32*)dst)[i] = ABCD[3-i];
#endif
}

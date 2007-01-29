/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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

#define GEN_FUNC_NAME( format, mode, dir )  mix_from_##format##_##mode##_##dir
#define FUNC_NAME( format, mode, dir )      GEN_FUNC_NAME( format, mode, dir )


#ifndef FORMAT
# warning FORMAT is not defined!!
#endif

#ifndef TYPE
# warning TYPE is not defined!!
#endif

#ifndef FSF_FROM_SRC
# warning FSF_FROM_SRC() is not defined!!
#endif


static int
FUNC_NAME(FORMAT,mono,fw) ( CoreSoundBuffer *buffer,
                            __fsf           *dest,
                            long             pos,
                            long             inc,
                            long             max,
                            __fsf            left,
                            __fsf            right )

{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < 0x1000) {
          /* upsample */
          for (i = 0; i < max; i += inc) {
               long  p = (i >> 12) + pos;
               __fsf s;
               __fsf t;
          
               if (p >= buffer->length)
                    p %= buffer->length;
                    
               s = FSF_FROM_SRC( src, p );
               
               if (i & 0xfff) {
                    __fsf l, r, w;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    t = FSF_FROM_SRC( src, q );
                    
                    w = fsf_from_int_scaled( 0x1000-(i&0xfff), 12 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
                    
                    dst[0] += (l == FSF_ONE) ? s :
                              (fsf_mul( s, l ) + fsf_mul( t, left-l ));
                    dst[1] += (r == FSF_ONE) ? s :
                              (fsf_mul( s, r ) + fsf_mul( t, right-r ));
               }
               else {
                    dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
                    dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               }
               
               dst += 2;
          }
          
          return (int)(dst - dest);
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (i = 0; i < max; i += inc) {
          long  p = (i >> 12) + pos;
          __fsf s;
          
          if (p >= buffer->length)
               p %= buffer->length;
                    
          s = FSF_FROM_SRC( src, p );
               
          dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
          dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               
          dst += 2;
     }
          
     return (int)(dst - dest);
}

static int
FUNC_NAME(FORMAT,mono,rw) ( CoreSoundBuffer *buffer,
                            __fsf           *dest,
                            long             pos,
                            long             inc,
                            long             max,
                            __fsf            left,
                            __fsf            right )

{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < 0x1000) {
          /* upsample */
          for (i = 0; i > max; i += inc) {
               long  p = (i >> 12) + pos;
               __fsf s;
               __fsf t;
          
               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               s = FSF_FROM_SRC( src, p );
               
               if (-i & 0xfff) {
                    __fsf l, r, w;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    t = FSF_FROM_SRC( src, q );
                    
                    w = fsf_from_int_scaled( 0x1000-(-i&0xfff), 12 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
                    
                    dst[0] += (l == FSF_ONE) ? s :
                              (fsf_mul( s, l ) + fsf_mul( t, left-l ));
                    dst[1] += (r == FSF_ONE) ? s :
                              (fsf_mul( s, r ) + fsf_mul( t, right-r ));
               }
               else {
                    dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
                    dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               }
               
               dst += 2;
          }
          
          return (int)(dst - dest);
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (i = 0; i > max; i += inc) {
          long  p = (i >> 12) + pos;
          __fsf s;
          
          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;
                    
          s = FSF_FROM_SRC( src, p );
               
          dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
          dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               
          dst += 2;
     }
          
     return (int)(dst - dest);
}


static int
FUNC_NAME(FORMAT,stereo,fw) ( CoreSoundBuffer *buffer,
                              __fsf           *dest,
                              long             pos,
                              long             inc,
                              long             max,
                              __fsf            left,
                              __fsf            right )
{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < 0x1000) {
          /* upsample */
          for (i = 0; i < max; i += inc) {
               long p = (i >> 12) + pos;

               if (p >= buffer->length)
                    p %= buffer->length;
                    
               if (i & 0xfff) {
                    __fsf l, r, w;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    p <<= 1;
                    q <<= 1;
                         
                    w = fsf_from_int_scaled( 0x1000-(i&0xfff), 12 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
                    
                    dst[0] += (l == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : (fsf_mul( FSF_FROM_SRC( src, p ), l ) + 
                                 fsf_mul( FSF_FROM_SRC( src, q ), left-l ));
                    p++;
                    q++;
                    dst[1] += (r == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : (fsf_mul( FSF_FROM_SRC( src, p ), r ) +
                                 fsf_mul( FSF_FROM_SRC( src, q ), right-r ));
               }
               else {
                    p <<= 1;
                    dst[0] += (left == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), left  );
                    p++;
                    dst[1] += (right == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), right );
               }
               
               dst += 2;
          } 
          
          return (int)(dst - dest);
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (i = 0; i < max; i += inc) {
          long p = (i >> 12) + pos;

          if (p >= buffer->length)
               p %= buffer->length;
                    
          p <<= 1;
          dst[0] += (left == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), left  );
          p++;
          dst[1] += (right == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), right );
              
          dst += 2;
     }     

     return (int)(dst - dest);
}

static int
FUNC_NAME(FORMAT,stereo,rw) ( CoreSoundBuffer *buffer,
                              __fsf           *dest,
                              long             pos,
                              long             inc,
                              long             max,
                              __fsf            left,
                              __fsf            right )
{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < 0x1000) {
          /* upsample */
          for (i = 0; i > max; i += inc) {
               long p = (i >> 12) + pos;

               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               if (-i & 0xfff) {
                    __fsf l, r, w;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    p <<= 1;
                    q <<= 1;
                         
                    w = fsf_from_int_scaled( 0x1000-(-i&0xfff), 12 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
                    
                    dst[0] += (l == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : (fsf_mul( FSF_FROM_SRC( src, p ), l ) + 
                                 fsf_mul( FSF_FROM_SRC( src, q ), left-l ));
                    p++;
                    q++;
                    dst[1] += (r == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : (fsf_mul( FSF_FROM_SRC( src, p ), r ) +
                                 fsf_mul( FSF_FROM_SRC( src, q ), right-r ));
               }
               else {
                    p <<= 1;
                    dst[0] += (left == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), left  );
                    p++;
                    dst[1] += (right == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), right );
               }
               
               dst += 2;
          } 
          
          return (int)(dst - dest);
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (i = 0; i > max; i += inc) {
          long p = (i >> 12) + pos;

          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;
                    
          p <<= 1;
          dst[0] += (left == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), left  );
          p++;
          dst[1] += (right == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), right );
              
          dst += 2;
     }     

     return (int)(dst - dest);
}


#undef FUNC_NAME
#undef GEN_FUNC_NAME

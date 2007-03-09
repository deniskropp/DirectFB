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

#define FSF_INTERP( a, b, w ) ( \
 __extension__({                \
     register __fsf _a = (a);   \
     register __fsf _b = (b);   \
     _a + fsf_mul( _b-_a, w );  \
 })                             \
)

static int
FUNC_NAME(FORMAT,mono,fw) ( CoreSoundBuffer *buffer,
                            __fsf           *dest,
                            long             pos,
                            long             inc,
                            long             max,
                            __fsf            left,
                            __fsf            right,
                            bool             last )

{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i   = 0;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max -= FS_PITCH_ONE;

          for (; i < max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf s;
          
               if (p >= buffer->length)
                    p %= buffer->length;
                    
               s = FSF_FROM_SRC( src, p );
               
               if (i & (FS_PITCH_ONE-1)) {
                    __fsf w;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    
                    w = fsf_from_int_scaled( i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    s = FSF_INTERP( s, FSF_FROM_SRC( src, q ), w );
               }

               dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               
               dst += 2;
          }
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i < max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
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
                            __fsf            right,
                            bool             last )

{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i   = 0;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max += FS_PITCH_ONE;

          for (; i > max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf s;
          
               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               s = FSF_FROM_SRC( src, p );
               
               if (-i & (FS_PITCH_ONE-1)) {
                    __fsf w;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    
                    w = fsf_from_int_scaled( -i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    s = FSF_INTERP( s, FSF_FROM_SRC( src, q ), w );
               }
               
               dst[0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dst[1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               
               dst += 2;
          }
          
          if (last)
               max -= FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i > max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
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
                              __fsf            right,
                              bool             last )
{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i   = 0;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max -= FS_PITCH_ONE;

          for (; i < max; i += inc) {
               long p = (i >> FS_PITCH_BITS) + pos;

               if (p >= buffer->length)
                    p %= buffer->length;
                    
               if (i & (FS_PITCH_ONE-1)) {
                    __fsf w, sl, sr;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    p <<= 1;
                    q <<= 1;
                         
                    w  = fsf_from_int_scaled( i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    sl = FSF_INTERP( FSF_FROM_SRC( src, p+0 ), 
                                     FSF_FROM_SRC( src, q+0 ), w );
                    sr = FSF_INTERP( FSF_FROM_SRC( src, p+1 ),
                                     FSF_FROM_SRC( src, q+1 ), w );
                    
                    dst[0] += (left  == FSF_ONE) ? sl : fsf_mul( sl, left  );
                    dst[1] += (right == FSF_ONE) ? sr : fsf_mul( sr, right );
               }
               else {
                    p <<= 1;
                    dst[0] += (left == FSF_ONE)
                              ? FSF_FROM_SRC( src, p+0 )
                              : fsf_mul( FSF_FROM_SRC( src, p+0 ), left  );
                    dst[1] += (right == FSF_ONE)
                              ? FSF_FROM_SRC( src, p+1 )
                              : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               }
               
               dst += 2;
          } 
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i < max; i += inc) {
          long p = (i >> FS_PITCH_BITS) + pos;

          if (p >= buffer->length)
               p %= buffer->length;         
          p <<= 1;
          
          dst[0] += (left == FSF_ONE)
                    ? FSF_FROM_SRC( src, p+0 )
                    : fsf_mul( FSF_FROM_SRC( src, p+0), left  );
          dst[1] += (right == FSF_ONE)
                    ? FSF_FROM_SRC( src, p+1 )
                    : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
              
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
                              __fsf            right,
                              bool             last )
{
     TYPE  *src = buffer->data;
     __fsf *dst = dest;
     long   i   = 0;

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max += FS_PITCH_ONE;

          for (; i > max; i += inc) {
               long p = (i >> FS_PITCH_BITS) + pos;

               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               if (-i & (FS_PITCH_ONE-1)) {
                    __fsf w, sl, sr;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    p <<= 1;
                    q <<= 1;
                         
                    w  = fsf_from_int_scaled( -i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    sl = FSF_INTERP( FSF_FROM_SRC( src, p+0 ), 
                                     FSF_FROM_SRC( src, q+0 ), w );
                    sr = FSF_INTERP( FSF_FROM_SRC( src, p+1 ),
                                     FSF_FROM_SRC( src, q+1 ), w );
                    
                    dst[0] += (left  == FSF_ONE) ? sl : fsf_mul( sl, left  );
                    dst[1] += (right == FSF_ONE) ? sr : fsf_mul( sr, right );
               }
               else {
                    p <<= 1;
                    dst[0] += (left == FSF_ONE)
                              ? FSF_FROM_SRC( src, p+0 )
                              : fsf_mul( FSF_FROM_SRC( src, p+0 ), left  );
                    dst[1] += (right == FSF_ONE)
                              ? FSF_FROM_SRC( src, p+1 )
                              : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               }
               
               dst += 2;
          } 
          
          if (last)
               max -= FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i > max; i += inc) {
          long p = (i >> FS_PITCH_BITS) + pos;

          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;            
          p <<= 1;
          
          dst[0] += (left == FSF_ONE)
                    ? FSF_FROM_SRC( src, p+0 )
                    : fsf_mul( FSF_FROM_SRC( src, p+0 ), left  );
          dst[1] += (right == FSF_ONE)
                    ? FSF_FROM_SRC( src, p+1 )
                    : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
              
          dst += 2;
     }     

     return (int)(dst - dest);
}

#undef FSF_INTERP

#undef FUNC_NAME
#undef GEN_FUNC_NAME

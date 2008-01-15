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
                            FSChannelMode    mode,
                            long             pos,
                            long             inc,
                            long             max,
                            __fsf            levels[6],
                            bool             last )

{
     TYPE  *src   = buffer->data;
     __fsf *dst   = dest;
     long   i     = 0;   
     __fsf  left  = levels[0];
     __fsf  right = levels[1];

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max -= FS_PITCH_ONE;

          for (; i < max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf s, sl, sr;
          
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
               
               sl = (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               sr = (right == FSF_ONE) ? s : fsf_mul( s, right );
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );
               
               dst += FS_MAX_CHANNELS;
          }
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i < max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
          __fsf s, sl, sr;
          
          if (p >= buffer->length)
               p %= buffer->length;
                    
          s = FSF_FROM_SRC( src, p );
               
          sl = (left  == FSF_ONE) ? s : fsf_mul( s, left  );
          sr = (right == FSF_ONE) ? s : fsf_mul( s, right );
               
          dst[0] += sl;
          dst[1] += sr;
          if (FS_MODE_HAS_CENTER(mode))
               dst[2] += fsf_shr( sl+sr, 1 );
               
          dst += FS_MAX_CHANNELS;
     }
          
     return (int)(dst - dest)/FS_MAX_CHANNELS;
}

static int
FUNC_NAME(FORMAT,mono,rw) ( CoreSoundBuffer *buffer,
                            __fsf           *dest,
                            FSChannelMode    mode,
                            long             pos,
                            long             inc,
                            long             max,
                            __fsf            levels[6],
                            bool             last )

{
     TYPE  *src   = buffer->data;
     __fsf *dst   = dest;
     long   i     = 0;   
     __fsf  left  = levels[0];
     __fsf  right = levels[1];

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max += FS_PITCH_ONE;

          for (; i > max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf s, sl, sr;
          
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
               
               sl = (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               sr = (right == FSF_ONE) ? s : fsf_mul( s, right );
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );
               
               dst += FS_MAX_CHANNELS;
          }
          
          if (last)
               max -= FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i > max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
          __fsf s, sl, sr;
          
          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;
                    
          s = FSF_FROM_SRC( src, p );
               
          sl = (left  == FSF_ONE) ? s : fsf_mul( s, left  );
          sr = (right == FSF_ONE) ? s : fsf_mul( s, right );
               
          dst[0] += sl;
          dst[1] += sr;
          if (FS_MODE_HAS_CENTER(mode))
               dst[2] += fsf_shr( sl+sr, 1 );
               
          dst += FS_MAX_CHANNELS;
     }
          
     return (int)(dst - dest)/FS_MAX_CHANNELS;
}


static int
FUNC_NAME(FORMAT,stereo,fw) ( CoreSoundBuffer *buffer,
                              __fsf           *dest,
                              FSChannelMode    mode,
                              long             pos,
                              long             inc,
                              long             max,
                              __fsf            levels[6],
                              bool             last )
{
     TYPE  *src   = buffer->data;
     __fsf *dst   = dest;
     long   i     = 0;   
     __fsf  left  = levels[0];
     __fsf  right = levels[1];

#ifdef FS_ENABLE_LINEAR_FILTER
     if (inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max -= FS_PITCH_ONE;

          for (; i < max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf sl, sr;

               if (p >= buffer->length)
                    p %= buffer->length;
                    
               if (i & (FS_PITCH_ONE-1)) {
                    __fsf w;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    p <<= 1;
                    q <<= 1;
                         
                    w  = fsf_from_int_scaled( i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    
                    sl = FSF_INTERP( FSF_FROM_SRC( src, p+0 ), 
                                     FSF_FROM_SRC( src, q+0 ), w );
                    if (left != FSF_ONE)
                         sl = fsf_mul( sl, left );
                    sr = FSF_INTERP( FSF_FROM_SRC( src, p+1 ),
                                     FSF_FROM_SRC( src, q+1 ), w );
                    if (right != FSF_ONE) 
                         sr = fsf_mul( sr, right );
               }
               else {
                    p <<= 1;
                    sl = (left == FSF_ONE)
                         ? FSF_FROM_SRC( src, p+0 )
                         : fsf_mul( FSF_FROM_SRC( src, p+0 ), left );
                    sr = (right == FSF_ONE)
                         ? FSF_FROM_SRC( src, p+1 )
                         : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               }
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );
               
               dst += FS_MAX_CHANNELS;
          } 
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i < max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
          __fsf sl, sr;

          if (p >= buffer->length)
               p %= buffer->length;         
          p <<= 1;
          
          sl = (left == FSF_ONE)
               ? FSF_FROM_SRC( src, p+0 )
               : fsf_mul( FSF_FROM_SRC( src, p+0), left );
          sr = (right == FSF_ONE)
               ? FSF_FROM_SRC( src, p+1 )
               : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               
          dst[0] += sl;
          dst[1] += sr;
          if (FS_MODE_HAS_CENTER(mode))
               dst[2] += fsf_shr( sl+sr, 1 );
              
          dst += FS_MAX_CHANNELS;
     }     

     return (int)(dst - dest)/FS_MAX_CHANNELS;
}

static int
FUNC_NAME(FORMAT,stereo,rw) ( CoreSoundBuffer *buffer,
                              __fsf           *dest,
                              FSChannelMode    mode,
                              long             pos,
                              long             inc,
                              long             max,
                              __fsf            levels[6],
                              bool             last )
{
     TYPE  *src   = buffer->data;
     __fsf *dst   = dest;
     long   i     = 0;   
     __fsf  left  = levels[0];
     __fsf  right = levels[1];

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max += FS_PITCH_ONE;

          for (; i > max; i += inc) {
               long  p = (i >> FS_PITCH_BITS) + pos;
               __fsf sl, sr;

               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               if (-i & (FS_PITCH_ONE-1)) {
                    __fsf w;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    p <<= 1;
                    q <<= 1;
                         
                    w  = fsf_from_int_scaled( -i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    
                    sl = FSF_INTERP( FSF_FROM_SRC( src, p+0 ), 
                                     FSF_FROM_SRC( src, q+0 ), w );
                    if (left != FSF_ONE)
                         sl = fsf_mul( sl, left );
                    sr = FSF_INTERP( FSF_FROM_SRC( src, p+1 ),
                                     FSF_FROM_SRC( src, q+1 ), w );
                    if (right != FSF_ONE)
                         sr = fsf_mul( sr, right );
               }
               else {
                    p <<= 1;
                    sl = (left == FSF_ONE)
                         ? FSF_FROM_SRC( src, p+0 )
                         : fsf_mul( FSF_FROM_SRC( src, p+0 ), left );
                    sr = (right == FSF_ONE)
                         ? FSF_FROM_SRC( src, p+1 )
                         : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               }
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );
               
               dst += FS_MAX_CHANNELS;
          } 
          
          if (last)
               max -= FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i > max; i += inc) {
          long  p = (i >> FS_PITCH_BITS) + pos;
          __fsf sl, sr;

          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;            
          p <<= 1;
          
          sl = (left == FSF_ONE)
               ? FSF_FROM_SRC( src, p+0 )
               : fsf_mul( FSF_FROM_SRC( src, p+0 ), left );
          sr = (right == FSF_ONE)
               ? FSF_FROM_SRC( src, p+1 )
               : fsf_mul( FSF_FROM_SRC( src, p+1 ), right );
               
          dst[0] += sl;
          dst[1] += sr;
          if (FS_MODE_HAS_CENTER(mode))
               dst[2] += fsf_shr( sl+sr, 1 );
              
          dst += FS_MAX_CHANNELS;
     }     

     return (int)(dst - dest)/FS_MAX_CHANNELS;
}

#if FS_MAX_CHANNELS > 2
static int
FUNC_NAME(FORMAT,multi,fw) ( CoreSoundBuffer *buffer,
                             __fsf           *dest,
                             FSChannelMode    mode,
                             long             pos,
                             long             inc,
                             long             max,
                             __fsf            levels[6],
                             bool             last )
{
     TYPE  *src      = buffer->data;
     __fsf *dst      = dest;
     long   i        = 0;
     int    channels = FS_CHANNELS_FOR_MODE(buffer->mode);

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
                    __fsf w, s;
                    long  q = p + 1;
                    
                    if (q == buffer->length)
                         q = 0;
                    p *= channels;
                    q *= channels;
                    
                    w = fsf_from_int_scaled( i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    
                    if (!FS_MODE_HAS_CENTER(buffer->mode)) {
                         __fsf sl, sr;
                         
                         /* front left */
                         sl = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                          FSF_FROM_SRC( src, q ), w );
                         if (levels[0] != FSF_ONE)
                              sl = fsf_mul( sl, levels[0] );
                         p++; q++;
                         
                         /* front right */
                         sr = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                          FSF_FROM_SRC( src, q ), w );
                         if (levels[1] != FSF_ONE)
                              sr = fsf_mul( sr, levels[1] );
                         p++; q++;
                         
                         dst[0] += sl;
                         dst[1] += sr;
                         if (FS_MODE_HAS_CENTER(mode))
                              dst[2] += fsf_shr( sl+sr, 1 );
                    }
                    else {
                         /* front left */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[0] != FSF_ONE)
                              s = fsf_mul( s, levels[0] );
                         dst[0] += s;
                         p++; q++;
                    
                         /* front center */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[2] != FSF_ONE)
                              s = fsf_mul( s, levels[2] );
                         dst[2] += s;
                         p++; q++;
                    
                         /* front right */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[1] != FSF_ONE)
                              s = fsf_mul( s, levels[1] );
                         dst[1] += s;
                         p++; q++;
                    }
                    
                    if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
                         /* rear */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         p++; q++;
                         
                         dst[3] += (levels[3] == FSF_ONE) 
                                   ? s : fsf_mul( s, levels[3] );
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? s : fsf_mul( s, levels[4] );
                    }
                    else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {                        
                         /* rear left */     
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[3] != FSF_ONE)
                              s = fsf_mul( s, levels[3] );
                         dst[3] += s;
                         p++; q++;
                         
                         /* rear right */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[4] != FSF_ONE)
                              s = fsf_mul( s, levels[4] );
                         dst[4] += s;
                         p++; q++;
                    }
                    
                    if (FS_MODE_HAS_LFE(buffer->mode)) {
                         /* subwoofer */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[5] != FSF_ONE)
                              s = fsf_mul( s, levels[5] );
                         dst[5] += s;
                    }
               }
               else {
                    p *= channels;
                    
                    if (!FS_MODE_HAS_CENTER(buffer->mode)) {
                         __fsf sl, sr;
                         
                         sl = (levels[0] == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
                         p++;
                         
                         sr = (levels[1] == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
                         p++;
                         
                         dst[0] += sl;
                         dst[1] += sr;
                         if (FS_MODE_HAS_CENTER(mode))
                              dst[2] += fsf_shr( sl+sr, 1 );
                         
                    }
                    else {
                         dst[0] += (levels[0] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
                         p++;
                    
                         dst[2] += (levels[2] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[2] );
                         p++;
                    
                         dst[1] += (levels[1] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
                         p++;
                    }
                    
                    if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
                         __fsf s;
                         
                         s = FSF_FROM_SRC( src, p );
                         p++;
                         
                         dst[3] += (levels[3] == FSF_ONE) 
                                   ? s : fsf_mul( s, levels[3] );
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? s : fsf_mul( s, levels[4] );
                    }
                    else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {
                         dst[3] += (levels[3] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[3] );
                         p++;
                         
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[4] );
                         p++;
                    }
                    
                    if (FS_MODE_HAS_LFE(buffer->mode)) { 
                         dst[5] += (levels[5] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[5] );
                    }
               }
               
               dst += FS_MAX_CHANNELS;
          } 
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i < max; i += inc) {
          long p = (i >> FS_PITCH_BITS) + pos;

          if (p >= buffer->length)
               p %= buffer->length;         
          p *= channels;
          
          if (!FS_MODE_HAS_CENTER(buffer->mode)) {
               __fsf sl, sr;
               
               sl = (levels[0] == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
               p++;
               
               sr = (levels[1] == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
               p++;
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );               
          }
          else {
               dst[0] += (levels[0] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
               p++;
                    
               dst[2] += (levels[2] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[2] );
               p++;
                    
               dst[1] += (levels[1] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
               p++;
          }
          
          if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
               __fsf s;
                         
               s = FSF_FROM_SRC( src, p );
               p++;
                         
               dst[3] += (levels[3] == FSF_ONE) 
                         ? s : fsf_mul( s, levels[3] );
               dst[4] += (levels[4] == FSF_ONE)
                         ? s : fsf_mul( s, levels[4] );
          }
          else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {    
               dst[3] += (levels[3] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[3] );
               p++;
                         
               dst[4] += (levels[4] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[4] );
               p++;
          }
               
          if (FS_MODE_HAS_LFE(buffer->mode)) {
               dst[5] += (levels[5] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[5] );
          }
              
          dst += FS_MAX_CHANNELS;
     }     

     return (int)(dst - dest)/FS_MAX_CHANNELS;
}

static int
FUNC_NAME(FORMAT,multi,rw) ( CoreSoundBuffer *buffer,
                             __fsf           *dest,
                             FSChannelMode    mode,
                             long             pos,
                             long             inc,
                             long             max,
                             __fsf            levels[6],
                             bool             last )
{
     TYPE  *src      = buffer->data;
     __fsf *dst      = dest;
     long   i        = 0;
     int    channels = FS_CHANNELS_FOR_MODE(buffer->mode);

#ifdef FS_ENABLE_LINEAR_FILTER
     if (-inc < FS_PITCH_ONE) {
          /* upsample */
          if (last)
               max -= FS_PITCH_ONE;

          for (; i > max; i += inc) {
               long p = (i >> FS_PITCH_BITS) + pos;

               if (p <= -buffer->length)
                    p %= buffer->length;
               if (p < 0)
                    p += buffer->length;
                    
               if (i & (FS_PITCH_ONE-1)) {
                    __fsf w, s;
                    long  q = p - 1;
                    
                    if (q == -1)
                         q += buffer->length;
                    p *= channels;
                    q *= channels;
                    
                    w = fsf_from_int_scaled( -i & (FS_PITCH_ONE-1), FS_PITCH_BITS );
                    
                    if (!FS_MODE_HAS_CENTER(buffer->mode)) {
                         __fsf sl, sr;
                         
                         /* front left */
                         sl = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                          FSF_FROM_SRC( src, q ), w );
                         if (levels[0] != FSF_ONE)
                              sl = fsf_mul( sl, levels[0] );
                         p++; q++;
                         
                         /* front right */
                         sr = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                          FSF_FROM_SRC( src, q ), w );
                         if (levels[1] != FSF_ONE)
                              sr = fsf_mul( sr, levels[1] );
                         p++; q++;
                         
                         dst[0] += sl;
                         dst[1] += sr;
                         if (FS_MODE_HAS_CENTER(mode))
                              dst[2] += fsf_shr( sl+sr, 1 );
                    }
                    else {
                         /* front left */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[0] != FSF_ONE)
                              s = fsf_mul( s, levels[0] );
                         dst[0] += s;
                         p++; q++;
                    
                         /* front center */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[2] != FSF_ONE)
                              s = fsf_mul( s, levels[2] );
                         dst[2] += s;
                         p++; q++;
                    
                         /* front right */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[1] != FSF_ONE)
                              s = fsf_mul( s, levels[1] );
                         dst[1] += s;
                         p++; q++;
                    }
                    
                    if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
                         /* rear */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         p++; q++;
                         
                         dst[3] += (levels[3] == FSF_ONE) 
                                   ? s : fsf_mul( s, levels[3] );
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? s : fsf_mul( s, levels[4] );
                    }
                    else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {
                         /* rear left */     
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[3] != FSF_ONE)
                              s = fsf_mul( s, levels[3] );
                         dst[3] += s;
                         p++; q++;
                         
                         /* rear right */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[4] != FSF_ONE)
                              s = fsf_mul( s, levels[4] );
                         dst[4] += s;
                         p++; q++;
                    }
                    
                    if (FS_MODE_HAS_LFE(buffer->mode)) {
                         /* subwoofer */
                         s = FSF_INTERP( FSF_FROM_SRC( src, p ), 
                                         FSF_FROM_SRC( src, q ), w );
                         if (levels[5] != FSF_ONE)
                              s = fsf_mul( s, levels[5] );
                         dst[5] += s;
                    }
               }
               else {
                    p *= channels;
                    
                    if (!FS_MODE_HAS_CENTER(buffer->mode)) {
                         __fsf sl, sr;
                         
                         sl = (levels[0] == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
                         p++;
                         
                         sr = (levels[1] == FSF_ONE)
                              ? FSF_FROM_SRC( src, p )
                              : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
                         p++;
                         
                         dst[0] += sl;
                         dst[1] += sr;
                         if (FS_MODE_HAS_CENTER(mode))
                              dst[2] += fsf_shr( sl+sr, 1 );                         
                    }
                    else {
                         dst[0] += (levels[0] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
                         p++;
                    
                         dst[2] += (levels[2] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[2] );
                         p++;
                    
                         dst[1] += (levels[1] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
                         p++;
                    }
                    
                    if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
                         __fsf s;
                         
                         s = FSF_FROM_SRC( src, p );
                         p++;
                         
                         dst[3] += (levels[3] == FSF_ONE) 
                                   ? s : fsf_mul( s, levels[3] );
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? s : fsf_mul( s, levels[4] );
                    }
                    else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {
                         dst[3] += (levels[3] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[3] );
                         p++;
                         
                         dst[4] += (levels[4] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[4] );
                         p++;
                         
                    }
                    
                    if (FS_MODE_HAS_LFE(buffer->mode)) {
                         dst[5] += (levels[5] == FSF_ONE)
                                   ? FSF_FROM_SRC( src, p )
                                   : fsf_mul( FSF_FROM_SRC( src, p ), levels[5] );
                    }
               }
               
               dst += FS_MAX_CHANNELS;
          } 
          
          if (last)
               max += FS_PITCH_ONE;
     }
#endif /* FS_ENABLE_LINEAR_FILTER */

     for (; i > max; i += inc) {
          long p = (i >> FS_PITCH_BITS) + pos;

          if (p <= -buffer->length)
               p %= buffer->length;
          if (p < 0)
               p += buffer->length;
          p *= channels;
          
          if (!FS_MODE_HAS_CENTER(buffer->mode)) {
               __fsf sl, sr;
               
               sl = (levels[0] == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
               p++;
               
               sr = (levels[1] == FSF_ONE)
                    ? FSF_FROM_SRC( src, p )
                    : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
               p++;
               
               dst[0] += sl;
               dst[1] += sr;
               if (FS_MODE_HAS_CENTER(mode))
                    dst[2] += fsf_shr( sl+sr, 1 );
          }
          else {
               dst[0] += (levels[0] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[0] );
               p++;
                    
               dst[2] += (levels[2] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                        : fsf_mul( FSF_FROM_SRC( src, p ), levels[2] );
               p++;
                    
               dst[1] = (levels[1] == FSF_ONE)
                        ? FSF_FROM_SRC( src, p )
                        : fsf_mul( FSF_FROM_SRC( src, p ), levels[1] );
               p++;
          }
                   
          if (FS_MODE_NUM_REARS(buffer->mode) == 1) {
               __fsf s;
                         
               s = FSF_FROM_SRC( src, p );
               p++;
                         
               dst[3] += (levels[3] == FSF_ONE) 
                          ? s : fsf_mul( s, levels[3] );
               dst[4] += (levels[4] == FSF_ONE)
                          ? s : fsf_mul( s, levels[4] );
          }
          else if (FS_MODE_NUM_REARS(buffer->mode) == 2) {         
               dst[3] += (levels[3] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[3] );
               p++;
                         
               dst[4] += (levels[4] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[4] );
               p++;
          }
          
          if (FS_MODE_HAS_LFE(buffer->mode)) {
               dst[5] += (levels[5] == FSF_ONE)
                         ? FSF_FROM_SRC( src, p )
                         : fsf_mul( FSF_FROM_SRC( src, p ), levels[5] );
          }
              
          dst += FS_MAX_CHANNELS;
     }     

     return (int)(dst - dest)/FS_MAX_CHANNELS;
}
#endif /* FS_MAX_CHANNELS > 2 */

#undef FSF_INTERP

#undef FUNC_NAME
#undef GEN_FUNC_NAME

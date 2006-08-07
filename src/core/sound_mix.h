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

#define GEN_FUNC_NAME( format, mode )  mix_from_##format##_##mode
#define FUNC_NAME( format, mode )      GEN_FUNC_NAME( format, mode )


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
FUNC_NAME(FORMAT,mono) ( CoreSoundBuffer *buffer,
                         __fsf           *dest,
                         int              dest_rate,
                         int              max_samples,
                         int              pos,
                         int              stop,
                         __fsf            left,
                         __fsf            right,
                         int              pitch,
                         int             *written )

{
     int            i;
     unsigned long  n, inc = (buffer->rate > 0xffff)
                             ? ((long long) buffer->rate * pitch / dest_rate)
                             : ((unsigned)  buffer->rate * pitch / dest_rate);
     TYPE          *data   = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples >> 1 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          long  p = (n >> 10) + pos;
          __fsf s;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

#ifdef FS_ENABLE_PRECISION
          if (inc < 0x400) {
               /* upsample */
               __fsf l, r;
               long  q = p + 1;
               
               if (n & 0x3ff) {
                    __fsf w = fsf_from_int_scaled( 0x400-(n&0x3ff), 10 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
               } else {
                    l = left;
                    r = right;
               }
               
               if (q == buffer->length)
                    q = 0;
               
               s = FSF_FROM_SRC( data,p );

               dest[i+0] += (l == FSF_ONE) ? s :
                             (fsf_mul( s, l ) + 
                              fsf_mul( FSF_FROM_SRC( data,q ), left-l ));
               dest[i+1] += (r == FSF_ONE) ? s :
                             (fsf_mul( s, r ) +
                              fsf_mul( FSF_FROM_SRC( data,q ), right-r ));
          }
          else {
               /* no-resample/downsample */
#endif
               s = FSF_FROM_SRC( data,p );

               dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
#ifdef FS_ENABLE_PRECISION
          }
#endif
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %ld (%d/%d).\n", 
              __FUNCTION__, n >> 10, i >> 1, max_samples >> 1 );

     *written = i;
     
     return n >> 10;
}

static int
FUNC_NAME(FORMAT,stereo) ( CoreSoundBuffer *buffer,
                           __fsf           *dest,
                           int              dest_rate,
                           int              max_samples,
                           int              pos,
                           int              stop,
                           __fsf            left,
                           __fsf            right,
                           int              pitch,
                           int             *written )
{
     int            i;
     unsigned long  n, inc = (buffer->rate > 0xffff)
                             ? ((long long) buffer->rate * pitch / dest_rate)
                             : ((unsigned)  buffer->rate * pitch / dest_rate);
     TYPE          *data   = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples >> 1 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          long p = (n >> 10) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

#ifdef FS_ENABLE_PRECISION
          if (inc < 0x400) {
               /* upsample */
               __fsf l, r;
               long  q = p + 1;

               if (n & 0x3ff) {
                    __fsf w = fsf_from_int_scaled( 0x400-(n&0x3ff), 10 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
               } else {
                    l = left;
                    r = right;
               }
               
               if (q == buffer->length)
                    q = 0;
               p <<= 1;
               q <<= 1;
                    
               dest[i+0] += (l == FSF_ONE)
                            ? FSF_FROM_SRC( data,p )
                            : (fsf_mul( FSF_FROM_SRC( data,p ), l ) + 
                               fsf_mul( FSF_FROM_SRC( data,q ), left-l ));
               p++; 
               q++;
                    
               dest[i+1] += (r == FSF_ONE)
                             ? FSF_FROM_SRC( data,p )
                             : (fsf_mul( FSF_FROM_SRC( data,p ), r ) +
                                fsf_mul( FSF_FROM_SRC( data,q ), right-r ));
          }
          else {
               /* no-resample/downsample */
#endif
               p <<= 1;

               dest[i+0] += (left == FSF_ONE)
                            ? FSF_FROM_SRC( data,p )
                            : fsf_mul( FSF_FROM_SRC( data,p ), left  );
               p++;
                    
               dest[i+1] += (right == FSF_ONE)
                            ? FSF_FROM_SRC( data,p )
                            : fsf_mul( FSF_FROM_SRC( data,p ), right );
#ifdef FS_ENABLE_PRECISION
          }
#endif
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %ld (%d/%d).\n", 
              __FUNCTION__, n >> 10, i >> 1, max_samples >> 1 );

     *written = i;
     
     return n >> 10;
}


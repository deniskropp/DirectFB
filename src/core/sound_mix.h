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

#define GEN_FUNC_NAME( name )  mix_from_##name
#define FUNC_NAME( name )      GEN_FUNC_NAME( name )

#ifndef NAME
# warning NAME is not defined!!
#endif

#ifndef TYPE
# warning TYPE is not defined!!
#endif

#ifndef FSF_FROM_SRC
# warning FSF_FROM_SRC() is not defined!!
#endif

static int
FUNC_NAME(NAME) ( CoreSoundBuffer *buffer,
                  __fsf           *dest,
                  int              dest_rate,
                  int              max_samples,
                  int              pos,
                  int              stop,
                  __fsf            left,
                  __fsf            right,
                  int              pitch )
{
     unsigned long  i, n;
     unsigned long  inc  = (buffer->rate * pitch) / dest_rate;
     TYPE          *data = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples / 2 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

#ifdef FS_ENABLE_ACCURACY
          if (inc < 0x100) {
               /* upsample */
               __fsf l, r;
               int   q = p + 1;
               
               if (q == buffer->length)
                    q = 0;
               
               if (n & 0xff) {
                    __fsf w = fsf_shr( fsf_from_int( 0x100-(n&0xff) ), 8 );
                    l = fsf_mul( left, w );
                    r = fsf_mul( right, w );
               } else {
                    l = left;
                    r = right;
               }
               
               if (buffer->channels == 2) {
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
                    __fsf s = FSF_FROM_SRC( data,p );

                    dest[i+0] += (l == FSF_ONE) ? s :
                                  (fsf_mul( s, l ) + 
                                   fsf_mul( FSF_FROM_SRC( data,q ), left-l ));
                    dest[i+1] += (r == FSF_ONE) ? s :
                                  (fsf_mul( s, r ) +
                                   fsf_mul( FSF_FROM_SRC( data,q ), right-r ));
               }
          }
          else {
               /* no-resample/downsample */
#endif
               if (buffer->channels == 2) {
                    p <<= 1;

                    dest[i+0] += (left == FSF_ONE)
                                 ? FSF_FROM_SRC( data,p )
                                 : fsf_mul( FSF_FROM_SRC( data,p ), left  );
                    p++;
                    
                    dest[i+1] += (right == FSF_ONE)
                                 ? FSF_FROM_SRC( data,p )
                                 : fsf_mul( FSF_FROM_SRC( data,p ), right );
               }
               else { 
                    __fsf s = FSF_FROM_SRC( data,p );

                    dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
                    dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
               }
#ifdef FS_ENABLE_ACCURACY
          }
#endif
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %ld (%ld).\n", __FUNCTION__, n >> 8, i >> 1 );

     return n >> 8;
}



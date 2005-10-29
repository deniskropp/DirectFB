/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.

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

#include <config.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/shmalloc.h>
#include <fusion/object.h>

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

/******************************************************************************/

static void
buffer_destructor( FusionObject *object, bool zombie )
{
     CoreSoundBuffer *buffer = (CoreSoundBuffer*) object;

     D_DEBUG( "FusionSound/Core: %s (%p, len %d, ch %d, fmt %d, rate %d)%s\n",
              __FUNCTION__, buffer, buffer->length, buffer->channels,
              buffer->format, buffer->rate, zombie ? " ZOMBIE!" : "" );

     SHFREE( buffer->shmpool, buffer->data );

     fusion_object_destroy( object );
}

FusionObjectPool *
fs_buffer_pool_create( const FusionWorld *world )
{
     return fusion_object_pool_create( "Sound Buffers", sizeof(CoreSoundBuffer),
                                       sizeof(CoreSoundBufferNotification),
                                       buffer_destructor, world );
}

/******************************************************************************/

DFBResult
fs_buffer_create( CoreSound        *core,
                  int               length,
                  int               channels,
                  FSSampleFormat    format,
                  int               rate,
                  CoreSoundBuffer **ret_buffer )
{
     int                  bytes;
     CoreSoundBuffer     *buffer;
     FusionSHMPoolShared *pool;

     D_ASSERT( core != NULL );
     D_ASSERT( length > 0 );
     D_ASSERT( channels == 1 || channels == 2 );
     D_ASSERT( rate > 0 );
     D_ASSERT( ret_buffer != NULL );

     D_DEBUG( "FusionSound/Core: %s (len %d, ch %d, fmt %d, rate %d)\n",
              __FUNCTION__, length, channels, format, rate );

     switch (format) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
               bytes = FS_BYTES_PER_SAMPLE( format );
               break;

          default:
               D_BUG( "unknown format" );
               return DFB_BUG;
     }

     pool = fs_core_shmpool( core );

     buffer = fs_core_create_buffer( core );
     if (!buffer)
          return DFB_FUSION;

     buffer->data = SHMALLOC( pool, length * bytes * channels );
     if (!buffer->data) {
          fusion_object_destroy( &buffer->object );
          return DFB_NOSYSTEMMEMORY;
     }

     buffer->core      = core;
     buffer->length    = length;
     buffer->channels  = channels;
     buffer->format    = format;
     buffer->rate      = rate;
     buffer->bytes     = bytes * channels;
     buffer->shmpool   = pool;

     fusion_object_activate( &buffer->object );

     *ret_buffer = buffer;

     return DFB_OK;
}

DFBResult
fs_buffer_lock( CoreSoundBuffer  *buffer,
                int               pos,
                int               length,
                void            **ret_data,
                int              *ret_bytes )
{
     D_ASSERT( buffer != NULL );
     D_ASSERT( pos >= 0 );
     D_ASSERT( pos < buffer->length );
     D_ASSERT( length >= 0 );
     D_ASSERT( length + pos <= buffer->length );
     D_ASSERT( ret_data != NULL );
     D_ASSERT( ret_bytes != NULL );

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, length %d)\n",
              __FUNCTION__, buffer, pos, length );

     if (!length)
          length = buffer->length - pos;

     *ret_data  = buffer->data + buffer->bytes * pos;
     *ret_bytes = buffer->bytes * length;

     return DFB_OK;
}

DFBResult
fs_buffer_unlock( CoreSoundBuffer *buffer )
{
     D_ASSERT( buffer != NULL );

     D_DEBUG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, buffer );

     return DFB_OK;
}

/******************************************************************************/          

static int
mix_from_8bit( CoreSoundBuffer *buffer,
               __fsf           *dest,
               int              dest_rate,
               int              max_samples,
               int              pos,
               int              stop,
               __fsf            left,
               __fsf            right,
               int              pitch )
{
     int   i, n;
     __u8 *data = buffer->data;
     int   inc  = (buffer->rate * pitch) / dest_rate;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples / 2 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

          if (buffer->channels == 2) {
               p <<= 1;

               dest[i+0] += (left == FSF_ONE)
                            ? fsf_from_u8( data[p+0] )
                            : fsf_mul( fsf_from_u8( data[p+0] ), left  );
               dest[i+1] += (right == FSF_ONE)
                            ? fsf_from_u8( data[p+1] )
                            : fsf_mul( fsf_from_u8( data[p+1] ), right );
          }
          else {
               __fsf s = fsf_from_u8( data[p] );

               dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
          }
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %d (%d).\n", __FUNCTION__, n >> 8, i >> 1 );

     return n >> 8;
}

static int
mix_from_16bit( CoreSoundBuffer *buffer,
                __fsf           *dest,
                int              dest_rate,
                int              max_samples,
                int              pos,
                int              stop,
                __fsf            left,
                __fsf            right,
                int              pitch )
{
     unsigned int  i, n;
     unsigned int  inc  = (buffer->rate * pitch) / dest_rate;
     __s16        *data = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples / 2 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

          if (buffer->channels == 2) {
               p <<= 1;

               dest[i+0] += (left == FSF_ONE)
                            ? fsf_from_s16( data[p+0] )
                            : fsf_mul( fsf_from_s16( data[p+0] ), left  );
               dest[i+1] += (right == FSF_ONE)
                            ? fsf_from_s16( data[p+1] )
                            : fsf_mul( fsf_from_s16( data[p+1] ), right );
          }
          else {
               __fsf s = fsf_from_s16( data[p] );

               dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
          }
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %d (%d).\n", __FUNCTION__, n >> 8, i >> 1 );

     return n >> 8;
}

static int
mix_from_24bit( CoreSoundBuffer *buffer,
                __fsf           *dest,
                int              dest_rate,
                int              max_samples,
                int              pos,
                int              stop,
                __fsf            left,
                __fsf            right,
                int              pitch )
{
     unsigned int  i, n;
     unsigned int  inc  = (buffer->rate * pitch) / dest_rate;
     __u8         *data = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples / 2 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

          if (buffer->channels == 2) {
               int s0, s1;
               
               p <<= 1;
               p  *= 3;
               s0  = ((data[p+0]<<8) | (data[p+1]<<16) | (data[p+2]<<24)) >> 8;
               s1  = ((data[p+3]<<8) | (data[p+4]<<16) | (data[p+5]<<24)) >> 8;
               
               dest[i+0] += (left == FSF_ONE)
                            ? fsf_from_s24( s0 )
                            : fsf_mul( fsf_from_s24( s0 ), left  );
               dest[i+1] += (right == FSF_ONE)
                            ? fsf_from_s24( s1 )
                            : fsf_mul( fsf_from_s24( s1 ), right );
          }
          else {
               __fsf s;
               
               p *= 3;
               s = fsf_from_s24( (int)((data[p+0] <<  8) |
                                       (data[p+1] << 16) |
                                       (data[p+2] << 24)) >> 8 );

               dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
          }
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %d (%d).\n", __FUNCTION__, n >> 8, i >> 1 );

     return n >> 8;
}

static int
mix_from_32bit( CoreSoundBuffer *buffer,
                __fsf           *dest,
                int              dest_rate,
                int              max_samples,
                int              pos,
                int              stop,
                __fsf            left,
                __fsf            right,
                int              pitch )
{
     unsigned int  i, n;
     unsigned int  inc  = (buffer->rate * pitch) / dest_rate;
     __s32        *data = buffer->data;

     D_DEBUG( "FusionSound/Core: %s (%p, pos %d, stop %d, max %d) ...\n",
              __FUNCTION__, buffer, pos, stop, max_samples / 2 );

     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          if (p >= buffer->length)
               p %= buffer->length;

          if (buffer->channels == 2) {
               p <<= 1;

               dest[i+0] += (left == FSF_ONE)
                            ? fsf_from_s32( data[p+0] )
                            : fsf_mul( fsf_from_s32( data[p+0] ), left  );
               dest[i+1] += (right == FSF_ONE)
                            ? fsf_from_s32( data[p+1] )
                            : fsf_mul( fsf_from_s32( data[p+1] ), right );
          }
          else {
               __fsf s = fsf_from_s32( data[p] );

               dest[i+0] += (left  == FSF_ONE) ? s : fsf_mul( s, left  );
               dest[i+1] += (right == FSF_ONE) ? s : fsf_mul( s, right );
          }
     }

     D_DEBUG( "FusionSound/Core: %s ... mixed %d (%d).\n", __FUNCTION__, n >> 8, i >> 1 );

     return n >> 8;
}


DFBResult
fs_buffer_mixto( CoreSoundBuffer *buffer,
                 __fsf           *dest,
                 int              dest_rate,
                 int              max_samples,
                 int              pos,
                 int              stop,
                 __fsf            left,
                 __fsf            right,
                 int              pitch,
                 int             *ret_pos,
                 int             *ret_num )
{
     DFBResult ret = DFB_OK;
     int       num;

     D_ASSERT( buffer != NULL );
     D_ASSERT( buffer->data != NULL );
     D_ASSERT( pos >= 0 );
     D_ASSERT( pos < buffer->length );
     D_ASSERT( stop <= buffer->length );
     D_ASSERT( dest != NULL );
     D_ASSERT( max_samples >= 0 );

     /* Make sure stop position is greater than start position. */
     if (stop >= 0 && pos >= stop)
          stop += buffer->length;

     /* Mix the data into the buffer. */
     switch (buffer->format) {
          case FSSF_U8:
               num = mix_from_8bit( buffer, dest, dest_rate, max_samples,
                                    pos, stop, left, right, pitch );
               break;

          case FSSF_S16:
               num = mix_from_16bit( buffer, dest, dest_rate, max_samples,
                                     pos, stop, left, right, pitch );
               break;

          case FSSF_S24:
               num = mix_from_24bit( buffer, dest, dest_rate, max_samples,
                                     pos, stop, left, right, pitch );
               break;
               
          case FSSF_S32:
               num = mix_from_32bit( buffer, dest, dest_rate, max_samples,
                                     pos, stop, left, right, pitch );
               break;

          default:
               D_BUG( "unknown sample format" );
               return DFB_BUG;
     }

     /* Advance position. */
     pos += num;

     /* Check if playback stopped. */
     if (stop >= 0 && pos >= stop) {
          pos = stop;
          ret = DFB_BUFFEREMPTY;
     }

     /* Handle wrap around. */
     pos %= buffer->length;

     /* Return new position. */
     if (ret_pos)
          *ret_pos = pos;

     /* Return number of samples mixed in. */
     if (ret_num)
          *ret_num = num;

     return ret;
}


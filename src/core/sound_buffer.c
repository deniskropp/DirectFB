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
          case FSSF_FLOAT:
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

#define FORMAT u8
#define TYPE __u8
#define FSF_FROM_SRC(s,i) fsf_from_u8(s[i])
#include "sound_mix.h"
#undef FSF_FROM_SRC
#undef TYPE
#undef FORMAT

#define FORMAT s16
#define TYPE __s16
#define FSF_FROM_SRC(s,i) fsf_from_s16(s[i])
#include "sound_mix.h"
#undef FSF_FROM_SRC
#undef TYPE
#undef FORMAT

#define FORMAT s24
#define TYPE __u8
#ifdef WORDS_BIGENDIAN
# define FSF_FROM_SRC(s,i) fsf_from_s24(((int)((s[i*3+2]<< 8) | \
                                               (s[i*3+1]<<16) | \
                                               (s[i*3+0]<<24)) >> 8))
#else
# define FSF_FROM_SRC(s,i) fsf_from_s24(((int)((s[i*3+0]<< 8) | \
                                               (s[i*3+1]<<16) | \
                                               (s[i*3+2]<<24)) >> 8))
#endif
#include "sound_mix.h"
#undef FSF_FROM_SRC
#undef TYPE
#undef FORMAT

#define FORMAT s32
#define TYPE __s32
#define FSF_FROM_SRC(s,i) fsf_from_s32(s[i])
#include "sound_mix.h"
#undef FSF_FROM_SRC
#undef TYPE
#undef FORMAT

#define FORMAT float
#define TYPE float
#define FSF_FROM_SRC(s,i) fsf_from_float(s[i])
#include "sound_mix.h"
#undef FSF_FROM_SRC
#undef TYPE
#undef FORMAT


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
                 int             *ret_num,
                 int             *ret_written )
{
     DFBResult ret = DFB_OK;
     int       num;
     int       len;

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
               if (buffer->channels == 1)
                    num = mix_from_u8_mono( buffer, dest, dest_rate, max_samples,
                                            pos, stop, left, right, pitch, &len );
               else
                    num = mix_from_u8_stereo( buffer, dest, dest_rate, max_samples,
                                              pos, stop, left, right, pitch, &len );
               break;

          case FSSF_S16:
               if (buffer->channels == 1)
                    num = mix_from_s16_mono( buffer, dest, dest_rate, max_samples,
                                             pos, stop, left, right, pitch, &len );
               else
                    num = mix_from_s16_stereo( buffer, dest, dest_rate, max_samples,
                                               pos, stop, left, right, pitch, &len );
               break;

          case FSSF_S24:
               if (buffer->channels == 1)
                    num = mix_from_s24_mono( buffer, dest, dest_rate, max_samples,
                                             pos, stop, left, right, pitch, &len );
               else
                    num = mix_from_s24_stereo( buffer, dest, dest_rate, max_samples,
                                               pos, stop, left, right, pitch, &len );
               break;
               
          case FSSF_S32:
               if (buffer->channels == 1)
                    num = mix_from_s32_mono( buffer, dest, dest_rate, max_samples,
                                             pos, stop, left, right, pitch, &len );
               else
                    num = mix_from_s32_stereo( buffer, dest, dest_rate, max_samples,
                                               pos, stop, left, right, pitch, &len );
               break;
               
          case FSSF_FLOAT:
               if (buffer->channels == 1)
                    num = mix_from_float_mono( buffer, dest, dest_rate, max_samples,
                                               pos, stop, left, right, pitch, &len );
               else
                    num = mix_from_float_stereo( buffer, dest, dest_rate, max_samples,
                                                 pos, stop, left, right, pitch, &len );
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

     /* Return number of samples written in. */
     if (ret_written)
          *ret_written = len;

     return ret;
}


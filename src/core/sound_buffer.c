/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <core/coredefs.h>
#include <core/fusion/shmalloc.h>
#include <core/fusion/object.h>

#include <core/core_sound.h>
#include <core/playback.h>
#include <core/sound_buffer.h>

/******************************************************************************/

struct __FS_CoreSoundBuffer {
     FusionObject     object;

     CoreSound       *core;

     int              length;
     int              channels;
     FSSampleFormat   format;
     int              rate;
     int              bytes;

     bool             notify;
     int              break_pos;
     
     void            *data;
};

/******************************************************************************/

static void
buffer_destructor( FusionObject *object, bool zombie )
{
     CoreSoundBuffer *buffer = (CoreSoundBuffer*) object;

     DEBUGMSG( "FusionSound/Core: %s (%p, len %d, ch %d, fmt %d, rate %d)%s\n",
               __FUNCTION__, buffer, buffer->length, buffer->channels,
               buffer->format, buffer->rate, zombie ? " ZOMBIE!" : "" );

     SHFREE( buffer->data );

     fusion_object_destroy( object );
}

FusionObjectPool *
fs_buffer_pool_create()
{
     return fusion_object_pool_create( "Sound Buffers", sizeof(CoreSoundBuffer),
                                       sizeof(CoreSoundBufferNotification),
                                       buffer_destructor );
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
     int              bytes;
     CoreSoundBuffer *buffer;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( length > 0 );
     DFB_ASSERT( channels == 1 || channels == 2 );
     DFB_ASSERT( rate > 0 );
     DFB_ASSERT( ret_buffer != NULL );

     DEBUGMSG( "FusionSound/Core: %s (len %d, ch %d, fmt %d, rate %d)\n",
               __FUNCTION__, length, channels, format, rate );

     switch (format) {
          case FSSF_S16:
               bytes = 2;
               break;

          case FSSF_U8:
               bytes = 1;
               break;

          default:
               BUG( "unknown format" );
               return DFB_BUG;
     }

     buffer = fs_core_create_buffer( core );
     if (!buffer)
          return DFB_FUSION;

     buffer->data = SHMALLOC( length * bytes * channels );
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
     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( pos >= 0 );
     DFB_ASSERT( pos < buffer->length );
     DFB_ASSERT( length >= 0 );
     DFB_ASSERT( length + pos <= buffer->length );
     DFB_ASSERT( ret_data != NULL );
     DFB_ASSERT( ret_bytes != NULL );

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, length %d)\n",
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
     DFB_ASSERT( buffer != NULL );

     DEBUGMSG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, buffer );

     return DFB_OK;
}

static inline int
mix_from_8bit( CoreSoundBuffer *buffer,
               int             *dest,
               int              dest_rate,
               int              max_samples,
               int              pos,
               int              stop,
               int              left,
               int              right,
               int              pitch )
{
     int   i, n;
     __u8 *data = buffer->data;
     int   inc  = (buffer->rate * pitch) / dest_rate;

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, max %d) ...\n",
               __FUNCTION__, buffer, pos, max_samples / 2 );
     
     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          p %= buffer->length;

          if (buffer->channels == 2) {
               p <<= 1;
          
               dest[i]   += ((int) data[p]   - 128) * left;
               dest[i+1] += ((int) data[p+1] - 128) * right;
          }
          else {
               int sample = (int) data[p] - 128;

               dest[i]   += sample * left;
               dest[i+1] += sample * right;
          }
     }

     DEBUGMSG( "FusionSound/Core: %s ... mixed %d.\n", __FUNCTION__, n >> 8 );
     
     return n >> 8;
}

static inline int
mix_from_16bit( CoreSoundBuffer *buffer,
                int             *dest,
                int              dest_rate,
                int              max_samples,
                int              pos,
                int              stop,
                int              left,
                int              right,
                int              pitch )
{
     int    i, n;
     __s16 *data = buffer->data;
     int    inc  = (buffer->rate * pitch) / dest_rate;

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, max %d) ...\n",
               __FUNCTION__, buffer, pos, max_samples / 2 );
     
     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (stop >= 0 && p >= stop)
               break;

          p %= buffer->length;

          if (buffer->channels == 2) {
               p <<= 1;
               
               dest[i]   += (data[p]   * left) >> 8;
               dest[i+1] += (data[p+1] * right) >> 8;
          }
          else {
               int sample = data[p];

               dest[i]   += (sample * left) >> 8;
               dest[i+1] += (sample * right) >> 8;
          }
     }

     DEBUGMSG( "FusionSound/Core: %s ... mixed %d.\n", __FUNCTION__, n >> 8 );
     
     return n >> 8;
}

DFBResult
fs_buffer_mixto( CoreSoundBuffer *buffer,
                 int             *dest,
                 int              dest_rate,
                 int              max_samples,
                 int              pos,
                 int              stop,
                 int              left,
                 int              right,
                 int              pitch,
                 int             *ret_pos )
{
     DFBResult ret = DFB_OK;

     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( buffer->data != NULL );
     DFB_ASSERT( pos >= 0 );
     DFB_ASSERT( pos < buffer->length );
     DFB_ASSERT( stop < buffer->length );
     DFB_ASSERT( dest != NULL );
     DFB_ASSERT( max_samples >= 0 );

     /* Make sure stop position is greater than start position. */
     if (stop >= 0 && pos >= stop)
          stop += buffer->length;
     
     /* Mix the data into the buffer. */
     switch (buffer->format) {
          case FSSF_S16:
               pos += mix_from_16bit( buffer, dest, dest_rate, max_samples,
                                      pos, stop, left, right, pitch );
               break;
          
          case FSSF_U8:
               pos += mix_from_8bit( buffer, dest, dest_rate, max_samples,
                                     pos, stop, left, right, pitch );
               break;

          default:
               BUG( "unknown sample format" );
               return DFB_BUG;
     }

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

     return ret;
}


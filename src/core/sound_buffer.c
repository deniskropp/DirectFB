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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/soundcard.h>

#include <core/coredefs.h>
#include <core/core.h>
#include <core/fusion/arena.h>
#include <core/fusion/list.h>
#include <core/fusion/shmalloc.h>
#include <core/fusion/object.h>
#include <core/thread.h>
#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/util.h>

#include <core/core_sound.h>
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

     shfree( buffer->data );

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
                  bool              notify,
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

     buffer->data = shmalloc( length * bytes * channels );
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
     buffer->notify    = notify;
     buffer->break_pos = -1;

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

DFBResult
fs_buffer_playback( CoreSoundBuffer *buffer, int pos, __u16 pan, bool loop )
{
     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( buffer->core != NULL );
     DFB_ASSERT( pos >= 0 );
     DFB_ASSERT( pos < buffer->length );

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, pan 0x%04x, loop %d)\n",
               __FUNCTION__, buffer, pos, pan, loop );

     return fs_core_add_playback( buffer->core, buffer, pos, pan, loop );
}

DFBResult
fs_buffer_set_break( CoreSoundBuffer *buffer, int pos )
{
     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( pos < buffer->length );

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d)\n",
               __FUNCTION__, buffer, pos );

     buffer->break_pos = pos;

     return DFB_OK;
}

DFBResult
fs_buffer_stop_all( CoreSoundBuffer *buffer )
{
     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( buffer->core != NULL );

     DEBUGMSG( "FusionSound/Core: %s (%p)\n", __FUNCTION__, buffer );

     return fs_core_remove_playbacks( buffer->core, buffer );
}

static int
mix_from_16bit( CoreSoundBuffer *buffer,
                int              pos,
                int             *dest,
                int              max_samples,
                __u16            pan,
                bool             loop,
                int              break_pos )
{
     __s16 *data = buffer->data;
     int    inc  = (buffer->rate << 8) / 44100;
     int    sl   = (pan > 0x8000) ? 0x10000 - ((int)pan - 0x8000) * 2 : 0x10000;
     int    sr   = (pan < 0x8000) ? pan * 2 : 0x10000;
     int    i, n;

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, max %d) ...\n",
               __FUNCTION__, buffer, pos, max_samples / 2 );
     
     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (break_pos >= 0 && p >= break_pos)
               break;

          if (p >= buffer->length) {
               if (loop)
                    p %= buffer->length;
               else
                    break;
          }

          if (buffer->channels == 2) {
               p <<= 1;
               
               if (pan != 0x8000) {
                    dest[i]   += (data[p]   * sl) >> 16;
                    dest[i+1] += (data[p+1] * sr) >> 16;
               }
               else {
                    dest[i]   += data[p];
                    dest[i+1] += data[p+1];
               }
          }
          else {
               int sample = data[p];

               if (pan != 0x8000) {
                    dest[i]   += (sample * sl) >> 16;
                    dest[i+1] += (sample * sr) >> 16;
               }
               else {
                    dest[i]   += sample;
                    dest[i+1] += sample;
               }
          }
     }

     DEBUGMSG( "FusionSound/Core: %s ... mixed %d.\n", __FUNCTION__, n >> 8 );
     
     return n >> 8;
}

static int
mix_from_8bit( CoreSoundBuffer *buffer,
               int              pos,
               int             *dest,
               int              max_samples,
               __u16            pan,
               bool             loop,
               int              break_pos )
{
     __u8 *data = buffer->data;
     int   inc  = (buffer->rate << 8) / 44100;
     int   sl   = (pan > 0x8000) ? 0x10000 - ((int)pan - 0x8000) * 2 : 0x10000;
     int   sr   = (pan < 0x8000) ? pan * 2 : 0x10000;
     int   i, n;

     DEBUGMSG( "FusionSound/Core: %s (%p, pos %d, max %d) ...\n",
               __FUNCTION__, buffer, pos, max_samples / 2 );
     
     for (i = 0, n = 0; i < max_samples; i += 2, n += inc) {
          int p = (n >> 8) + pos;

          if (break_pos >= 0 && p >= break_pos)
               break;

          if (p >= buffer->length) {
               if (loop)
                    p %= buffer->length;
               else
                    break;
          }

          if (buffer->channels == 2) {
               p <<= 1;
          
               if (pan != 0x8000) {
                    dest[i]   += (((int) data[p]   - 128) * sl) >> 8;
                    dest[i+1] += (((int) data[p+1] - 128) * sr) >> 8;
               }
               else {
                    dest[i]   += ((int) data[p]   - 128) << 8;
                    dest[i+1] += ((int) data[p+1] - 128) << 8;
               }
          }
          else {
               int sample = ((int) data[p] - 128) << 8;

               if (pan != 0x8000) {
                    dest[i]   += (sample * sl) >> 16;
                    dest[i+1] += (sample * sr) >> 16;
               }
               else {
                    dest[i]   += sample;
                    dest[i+1] += sample;
               }
          }
     }

     DEBUGMSG( "FusionSound/Core: %s ... mixed %d.\n", __FUNCTION__, n >> 8 );
     
     return n >> 8;
}

DFBResult
fs_buffer_mixto( CoreSoundBuffer *buffer,
                 int              pos,
                 int             *dest,
                 int              max_samples,
                 __u16            pan,
                 bool             loop,
                 int             *ret_pos )
{
     int       break_pos;
     DFBResult ret = DFB_OK;

     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( buffer->data != NULL );
     DFB_ASSERT( pos >= 0 );
     DFB_ASSERT( pos < buffer->length );
     DFB_ASSERT( dest != NULL );
     DFB_ASSERT( max_samples >= 0 );

     break_pos = buffer->break_pos;
     if (break_pos >= 0 && pos >= break_pos)
          break_pos += buffer->length;
     
     switch (buffer->format) {
          case FSSF_S16:
               pos += mix_from_16bit( buffer, pos, dest,
                                      max_samples, pan, loop, break_pos );
               break;
          
          case FSSF_U8:
               pos += mix_from_8bit( buffer, pos, dest,
                                     max_samples, pan, loop, break_pos );
               break;

          default:
               BUG( "unknown sample format" );
               return DFB_BUG;
     }

     if (break_pos >= 0 && pos >= break_pos) {
          pos = buffer->break_pos;
          
          ret = DFB_BUFFEREMPTY;
     }
     else if (pos >= buffer->length) {
          pos %= buffer->length;
          
          if (!loop)
               ret = DFB_BUFFEREMPTY;
     }

     if (ret_pos)
          *ret_pos = pos;

     return ret;
}

DFBResult
fs_buffer_playback_notify( CoreSoundBuffer                  *buffer,
                           CoreSoundBufferNotificationFlags  flags,
                           int                               pos )
{
     CoreSoundBufferNotification notification;

     DFB_ASSERT( buffer != NULL );
     DFB_ASSERT( flags == CABNF_PLAYBACK_ADVANCED ||
                 flags == CABNF_PLAYBACK_ENDED );
     DFB_ASSERT( pos >= 0 );
     DFB_ASSERT( pos < buffer->length );

     if (!buffer->notify)
          return DFB_OK;

     notification.flags  = flags;
     notification.buffer = buffer;
     notification.pos    = pos;

     fs_buffer_dispatch( buffer, &notification, NULL );

     return DFB_OK;
}


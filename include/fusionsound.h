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

#ifndef __FUSIONSOUND_H__
#define __FUSIONSOUND_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Main FusionSound interface.
 */
DECLARE_INTERFACE( IFusionSound )

/*
 * Static sound buffer for playback of smaller samples.
 */
DECLARE_INTERFACE( IFusionSoundBuffer )

/*
 * Streaming sound buffer for playback of large files or real time data.
 */
DECLARE_INTERFACE( IFusionSoundStream )

/*
 * The sample format is the way of storing audible information.
 */
typedef enum {
     DASF_UNKNOWN        = 0x00000000,      /* Unknown/invalid format. */
     DASF_S16            = 0x00000001,      /* Signed 16 bit (linear). */
     DASF_U8             = 0x00000002       /* Unsigned 8 bit (linear). */
} DASampleFormat;

/*
 * Each buffer description flag validates one field of the buffer description.
 */
typedef enum {
     DABDF_NONE          = 0x00000000,      /* None of these. */
     DABDF_LENGTH        = 0x00000001,      /* Buffer length is set. */
     DABDF_CHANNELS      = 0x00000002,      /* Number of channels is set. */
     DABDF_SAMPLEFORMAT  = 0x00000004,      /* Sample format is set. */
     DABDF_SAMPLERATE    = 0x00000008,      /* Sample rate is set. */
     DABDF_ALL           = 0x0000000F       /* All of these. */
} DABufferDescriptionFlags;

/*
 * The buffer description is used to create static sound buffers.
 */
typedef struct {
     DABufferDescriptionFlags flags;        /* Defines which fields are set. */

     int                      length;       /* Buffer length specified as
                                               number of samples per channel. */
     int                      channels;     /* Number of channels. */
     DASampleFormat           sampleformat; /* Format of each sample. */
     int                      samplerate;   /* Number of samples per second. */
} DABufferDescription;

/*
 * Each stream description flag validates one field of the stream description.
 */
typedef enum {
     DASDF_NONE          = 0x00000000,      /* None of these. */
     DASDF_BUFFERSIZE    = 0x00000001,      /* Ring buffer size is set. */
     DASDF_CHANNELS      = 0x00000002,      /* Number of channels is set. */
     DASDF_SAMPLEFORMAT  = 0x00000004,      /* Sample format is set. */
     DASDF_SAMPLERATE    = 0x00000008,      /* Sample rate is set. */
     DASDF_ALL           = 0x0000000F       /* All of these. */
} DAStreamDescriptionFlags;

/*
 * The stream description is used to create streaming sound buffers.
 */
typedef struct {
     DAStreamDescriptionFlags flags;        /* Defines which fields are set. */

     int                      buffersize;   /* Ring buffer size specified as
                                               number of samples per channel. */
     int                      channels;     /* Number of channels. */
     DASampleFormat           sampleformat; /* Format of each sample. */
     int                      samplerate;   /* Number of samples per second. */
} DAStreamDescription;

/*
 * <i><b>IFusionSound</b></i> is the main FusionSound interface and can be
 * retrieved by calling <i>IDirectFB::GetInterface()</i>.
 *
 * <b>Static sound buffers</b> for smaller samples like sound effects in
 * games or audible feedback in user interfaces are created by calling
 * <i>CreateBuffer()</i>. They can be played several times with an unlimited
 * number of <b>concurrent playbacks</b>. Playback can also be started in
 * <b>looping</b> mode or <b>panned</b>.
 *
 * <b>Streaming sound buffers</b> for large or compressed files and for
 * streaming of real time sound data are created by calling
 * <i>CreateStream()</i>. There's only one <b>single playback</b> that
 * automatically starts when data is written to the <b>ring buffer</b> for the
 * first time. If the buffer underruns the playback automatically stops and
 * is started again when the ring buffer is written to again.
 */
DEFINE_INTERFACE( IFusionSound,

   /** Buffers **/

     /*
      * Create a static sound buffer.
      */
     DFBResult (*CreateBuffer) (
          IFusionSound             *thiz,
          DABufferDescription      *desc,
          IFusionSoundBuffer      **interface
     );

     /*
      * Create a streaming sound buffer.
      *
      * If <i>desc</i> is NULL, all default values will be used.
      * Defaults are 44kHz, stereo, 16 bit (DASF_S16) with a ring buffer
      * size that holds enough samples for one second of playback.
      */
     DFBResult (*CreateStream) (
          IFusionSound             *thiz,
          DAStreamDescription      *desc,
          IFusionSoundStream      **interface
     );
)

/*
 * Playback flags control the bahaviour of <i>IFusionSoundBuffer::Play()</i>.
 */
typedef enum {
     DAPLAY_NOFX         = 0x00000000,  /* No effects are applied. */
     DAPLAY_LOOPING      = 0x00000001,  /* Playback will continue at the
                                           beginning of the buffer as soon as
                                           the end is reached. There's no gap
                                           produced by concatenation. */
     DAPLAY_PAN          = 0x00000002,  /* Use the value passed to
                                           <i>IFusionSoundBuffer::SetPan()</i>
                                           to control the balance between left
                                           and right speakers. */
     DAPLAY_ALL          = 0x00000003   /* All of these. */
} DABufferPlayFlags;

DEFINE_INTERFACE( IFusionSoundBuffer,

   /** Access **/

     /*
      * Lock a buffer to access its data.
      */
     DFBResult (*Lock) (
          IFusionSoundBuffer       *thiz,
          void                    **data
     );

     /*
      * Unlock a buffer.
      */
     DFBResult (*Unlock) (
          IFusionSoundBuffer       *thiz
     );
     

   /** Playback **/

     /*
      * Set panning value.
      *
      * The <i>value</i> ranges from -1.0f (left) to 1.0f (right).
      */
     DFBResult (*SetPan) (
          IFusionSoundBuffer       *thiz,
          float                     value
     );

     /*
      * Start a playback of the buffer.
      */
     DFBResult (*Play) (
          IFusionSoundBuffer       *thiz,
          DABufferPlayFlags         flags
     );

     /*
      * Stop all running playbacks of the buffer.
      */
     DFBResult (*StopAll) (
          IFusionSoundBuffer       *thiz
     );
)

DEFINE_INTERFACE( IFusionSoundStream,

   /** Ring buffer **/

     /*
      * Fill the ring buffer with data.
      *
      * Writes the sample <i>data</i> into the ring buffer.
      * The <i>length</i> specifies the number of samples per channel.
      *
      * If the ring buffer is full the method blocks until there's enough
      * space. If this method returns successfully, all data has been written.
      */
     DFBResult (*Write) (
          IFusionSoundStream       *thiz,
          const void               *data,
          int                       length
     );

     /*
      * Wait for a specified amount of free ring buffer space.
      *
      * This method blocks until there's enough space in the ring buffer
      * so that writing data of the specified <i>length</i> wouldn't block.
      */
     DFBResult (*Wait) (
          IFusionSoundStream       *thiz,
          int                       length
     );

     /*
      * Query ring buffer status.
      *
      * Returns the number of samples the ring buffer is <i>filled</i> with,
      * the <i>total</i> number of samples that can be stored (buffer size),
      * current <i>read_position</i> and current <i>write_position</i>.
      *
      * Simply pass NULL for values that are not of interest.
      */
     DFBResult (*GetStatus) (
          IFusionSoundStream       *thiz,
          int                      *filled,
          int                      *total,
          int                      *read_position,
          int                      *write_position
     );
)


#ifdef __cplusplus
}
#endif

#endif


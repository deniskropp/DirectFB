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
 * Advanced playback control for static sound buffers.
 */
DECLARE_INTERFACE( IFusionSoundPlayback )

/*
 * The sample format is the way of storing audible information.
 */
typedef enum {
     FSSF_UNKNOWN        = 0x00000000,      /* Unknown/invalid format. */
     FSSF_S16            = 0x00000001,      /* Signed 16 bit (linear). */
     FSSF_U8             = 0x00000002       /* Unsigned 8 bit (linear). */
} FSSampleFormat;

/*
 * Each buffer description flag validates one field of the buffer description.
 */
typedef enum {
     FSBDF_NONE          = 0x00000000,      /* None of these. */
     FSBDF_LENGTH        = 0x00000001,      /* Buffer length is set. */
     FSBDF_CHANNELS      = 0x00000002,      /* Number of channels is set. */
     FSBDF_SAMPLEFORMAT  = 0x00000004,      /* Sample format is set. */
     FSBDF_SAMPLERATE    = 0x00000008,      /* Sample rate is set. */
     FSBDF_ALL           = 0x0000000F       /* All of these. */
} FSBufferDescriptionFlags;

/*
 * The buffer description is used to create static sound buffers.
 */
typedef struct {
     FSBufferDescriptionFlags flags;        /* Defines which fields are set. */

     int                      length;       /* Buffer length specified as
                                               number of samples per channel. */
     int                      channels;     /* Number of channels. */
     FSSampleFormat           sampleformat; /* Format of each sample. */
     int                      samplerate;   /* Number of samples per second. */
} FSBufferDescription;

/*
 * Each stream description flag validates one field of the stream description.
 */
typedef enum {
     FSSDF_NONE          = 0x00000000,      /* None of these. */
     FSSDF_BUFFERSIZE    = 0x00000001,      /* Ring buffer size is set. */
     FSSDF_CHANNELS      = 0x00000002,      /* Number of channels is set. */
     FSSDF_SAMPLEFORMAT  = 0x00000004,      /* Sample format is set. */
     FSSDF_SAMPLERATE    = 0x00000008,      /* Sample rate is set. */
     FSSDF_ALL           = 0x0000000F       /* All of these. */
} FSStreamDescriptionFlags;

/*
 * The stream description is used to create streaming sound buffers.
 */
typedef struct {
     FSStreamDescriptionFlags flags;        /* Defines which fields are set. */

     int                      buffersize;   /* Ring buffer size specified as
                                               number of samples per channel. */
     int                      channels;     /* Number of channels. */
     FSSampleFormat           sampleformat; /* Format of each sample. */
     int                      samplerate;   /* Number of samples per second. */
} FSStreamDescription;

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
          FSBufferDescription      *desc,
          IFusionSoundBuffer      **interface
     );

     /*
      * Create a streaming sound buffer.
      *
      * If <i>desc</i> is NULL, all default values will be used.
      * Defaults are 44kHz, stereo, 16 bit (FSSF_S16) with a ring buffer
      * size that holds enough samples for one second of playback.
      */
     DFBResult (*CreateStream) (
          IFusionSound             *thiz,
          FSStreamDescription      *desc,
          IFusionSoundStream      **interface
     );
)

/*
 * Flags for simple playback using <i>IFusionSoundBuffer::Play()</i>.
 */
typedef enum {
     FSPLAY_NOFX         = 0x00000000,  /* No effects are applied. */
     FSPLAY_LOOPING      = 0x00000001,  /* Playback will continue at the
                                           beginning of the buffer as soon as
                                           the end is reached. There's no gap
                                           produced by concatenation. Only one
                                           looping playback at a time is
                                           supported by the simple playback.
                                           See also <i>CreatePlayback()</i>. */
     FSPLAY_PAN          = 0x00000002,  /* Use value passed to <i>SetPan()</i>
                                           to control the balance between left
                                           and right speakers. */
     FSPLAY_ALL          = 0x00000003   /* All of these. */
} FSBufferPlayFlags;


DEFINE_INTERFACE( IFusionSoundBuffer,

   /** Information **/

     /*
      * Get a description of the buffer.
      */
     DFBResult (*GetDescription) (
          IFusionSoundBuffer       *thiz,
          FSBufferDescription      *desc
     );


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
     

   /** Simple playback **/

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
      * Start playing the buffer.
      *
      * There's no limited number of concurrent playbacks, but the simple
      * playback only provides one looping playback at a time.
      *
      * See also <i>CreatePlayback()</i>.
      */
     DFBResult (*Play) (
          IFusionSoundBuffer       *thiz,
          FSBufferPlayFlags         flags
     );

     /*
      * Stop looping playback.
      *
      * This method is for the one concurrently looping playback that is
      * provided by the simple playback.
      *
      * See also <i>CreatePlayback()</i>.
      */
     DFBResult (*Stop) (
          IFusionSoundBuffer       *thiz
     );


   /** Advanced playback **/

     /*
      * Retrieve advanced playback control interface.
      */
     DFBResult (*CreatePlayback) (
          IFusionSoundBuffer       *thiz,
          IFusionSoundPlayback    **interface
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
      *
      * Specifying a <i>length</i> of zero waits until playback has finished.
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

DEFINE_INTERFACE( IFusionSoundPlayback,

   /** Commands **/

     /*
      * Start playback of the buffer.
      *
      * The <i>start</i> position specifies the sample at which the playback
      * is going to start.
      *
      * The <i>stop</i> position specifies the sample after the last sample
      * being played. The default value of zero causes the playback to stop
      * after the last sample in the buffer, i.e. upon completion. A negative
      * value means unlimited playback (looping).
      *
      * This method can be used for seeking if the playback is already running.
      */
     DFBResult (*Start) (
          IFusionSoundPlayback     *thiz,
          int                       start,
          int                       stop
     );

     /*
      * Stop playback of the buffer.
      *
      * This method stops a running playback. The playback can be continued
      * by calling <i>Continue()</i> or restarted using <i>Start()</i>.
      */
     DFBResult (*Stop) (
          IFusionSoundPlayback     *thiz
     );

     /*
      * Continue playback of the buffer.
      *
      * This method is used to continue an explicitly stopped playback.
      *
      * It returns an error, if the playback is still running, or if it has
      * stopped automatically by reaching the stop position.
      */
     DFBResult (*Continue) (
          IFusionSoundPlayback     *thiz
     );

     /*
      * Wait until playback of the buffer has finished.
      *
      * This method will block as long as the playback is running.
      *
      * If the playback is in looping mode the method returns immediately
      * with an error.
      */
     DFBResult (*Wait) (
          IFusionSoundPlayback     *thiz
     );


   /** Parameters **/

     /*
      * Set volume level.
      *
      * The <i>level</i> is a linear factor being 1.0f by default, currently
      * ranges from 0.0f to 256.0f due to internal mixing limitations.
      */
     DFBResult (*SetVolume) (
          IFusionSoundPlayback     *thiz,
          float                     level
     );

     /*
      * Set panning value.
      *
      * The <i>value</i> ranges from -1.0f (left) to 1.0f (right).
      */
     DFBResult (*SetPan) (
          IFusionSoundPlayback     *thiz,
          float                     value
     );

     /*
      * Set pitch value.
      *
      * The <i>value</i> is a linear factor being 1.0f by default, currently
      * ranges from 0.0f to 256.0f due to internal mixing limitations.
      */
     DFBResult (*SetPitch) (
          IFusionSoundPlayback     *thiz,
          float                     value
     );
)

#ifdef __cplusplus
}
#endif

#endif


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

#ifndef __FUSIONSOUND_H__
#define __FUSIONSOUND_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Version handling.
 */
extern const unsigned int fusionsound_major_version;
extern const unsigned int fusionsound_minor_version;
extern const unsigned int fusionsound_micro_version;
extern const unsigned int fusionsound_binary_age;
extern const unsigned int fusionsound_interface_age;

/*
 * Check for a certain FusionSound version.
 * In case of an error a message is returned describing the mismatch.
 */
const char * FusionSoundCheckVersion( unsigned int required_major,
                                      unsigned int required_minor,
                                      unsigned int required_micro );

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
 * Rendering music data into a stream.
 */
DECLARE_INTERFACE( IFusionSoundMusicProvider )

/*
 * Parses the command-line and initializes some variables. You absolutely need to
 * call this before doing anything else. Removes all options used by FusionSound from argv.
 */
DFBResult FusionSoundInit(
                           int    *argc,   /* pointer to main()'s argc */
                           char *(*argv[]) /* pointer to main()'s argv */
                         );

/*
 * Sets configuration parameters supported on command line and in config file.
 * Can only be called before FusionSoundCreate but after FusionSoundInit.
 */
DFBResult FusionSoundSetOption(
                                const char *name,
                                const char *value
                              );

/*
 * Creates the super interface.
 */
DFBResult FusionSoundCreate(
                             IFusionSound **ret_interface  /* pointer to the created interface */
                           );

/*
 * Print a description of the result code along with an
 * optional message that is put in front with a colon.
 */
DFBResult FusionSoundError(
                            const char *msg,    /* optional message */
                            DFBResult   result  /* result code to interpret */
                          );
                          
/*
 * Behaves like FusionSoundError, but shuts down the calling application.
 */
DFBResult FusionSoundErrorFatal(
                            const char *msg,    /* optional message */
                            DFBResult   result  /* result code to interpret */
                          );

/*
 * Returns a string describing 'result'.
 */
const char *FusionSoundErrorString(
                                    DFBResult result
                                  );
                                  
/*
 * Retrieves information about supported command-line flags in the
 * form of a user-readable string formatted suitable to be printed
 * as usage information.
 */
const char *FusionSoundUsageString( void );


#define FS_SOUND_DRIVER_INFO_NAME_LENGTH     40
#define FS_SOUND_DRIVER_INFO_VENDOR_LENGTH   60
#define FS_SOUND_DRIVER_INFO_URL_LENGTH     100
#define FS_SOUND_DRIVER_INFO_LICENSE_LENGTH  40

/*
 * Description of the sound driver.
 */
typedef struct {
     int  major;                                        /* Major version */
     int  minor;                                        /* Minor version */
     
     char name[FS_SOUND_DRIVER_INFO_NAME_LENGTH];       /* Driver name */
     char vendor[FS_SOUND_DRIVER_INFO_VENDOR_LENGTH];   /* Driver vendor */
     char url[FS_SOUND_DRIVER_INFO_URL_LENGTH];         /* Driver URL */
     char license[FS_SOUND_DRIVER_INFO_LICENSE_LENGTH]; /* Driver license */
} FSSoundDriverInfo;

#define FS_SOUND_DEVICE_INFO_NAME_LENGTH    256

/*
 * Description of the sound device.
 */
typedef struct {
     char name[FS_SOUND_DEVICE_INFO_NAME_LENGTH];       /* Device name */
     
     FSSoundDriverInfo driver;
} FSDeviceDescription;

/*
 * @internal
 *
 * Encodes format constants in the following way (bit 31 - 0):
 *
 * 0000:0000 | 0000:0dcc | cccc:cbbb | bbbb:aaaa
 *
 * a) sampleformat index<br>
 * b) total bits per sample<br>
 * c) effective sound bits per sample (i.e. depth)<br>
 * d) signed sample format
 */
#define FS_SAMPLEFORMAT( index, bits, depth, sgned ) \
     ( ((index & 0x0F)      ) |                      \
       ((bits  & 0x7F) <<  4) |                      \
       ((depth & 0x7F) << 11) |                      \
       ((sgned & 0x01) << 18) )

/*
 * The sample format is the way of storing audible information.
 *
 * 8, 16 and 32 bit samples are always stored in <b>native endian</b>, while
 * 24 bit samples are stored in <b>little endian</b>. This keeps the library and
 * applications simple and clean. Always access sample buffers like arrays of
 * 8, 16 or 32 bit integers depending on the sample format, unless data is
 * written with endianness being taken care of. This does not excuse from endian
 * conversion that might be necessary when reading data from files.
 */
typedef enum {
     FSSF_UNKNOWN        = 0x00000000, /* Unknown or invalid format. */
     
     /* Unsigned 8 bit. */ 
     FSSF_U8             = FS_SAMPLEFORMAT( 0,  8,  8, 0 ),
     
     /* Signed 16 bit (native endian). */
     FSSF_S16            = FS_SAMPLEFORMAT( 1, 16, 16, 1 ),

     /* Signed 24 bit (native endian). */
     FSSF_S24            = FS_SAMPLEFORMAT( 2, 24, 24, 1 ),

     /* Signed 32 bit (native endian). */
     FSSF_S32            = FS_SAMPLEFORMAT( 3, 32, 32, 1 ),
     
     /* Floating-point 32 bit. */
     FSSF_FLOAT          = FS_SAMPLEFORMAT( 4, 32, 32, 1 ),
} FSSampleFormat;

/* Number of defined sample formats */
#define FS_NUM_SAMPLEFORMATS  5

/* These macros extract information about the sample format. */
#define FS_SAMPLEFORMAT_INDEX( fmt )  (((fmt) & 0x0000000F)      )

#define FS_BITS_PER_SAMPLE( fmt )     (((fmt) & 0x000007F0) >>  4)

#define FS_BYTES_PER_SAMPLE( fmt )    (((fmt) & 0x000007F0) >>  7)

#define FS_SAMPLEFORMAT_DEPTH( fmt )  (((fmt) & 0x0003f800) >> 11)

#define FS_SIGNED_SAMPLEFORMAT( fmt ) (((fmt) & 0x00040000) ? 1 : 0)


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
     FSSDF_PREBUFFER     = 0x00000010,      /* Prebuffer amount is set. */
     FSSDF_ALL           = 0x0000001F       /* All of these. */
} FSStreamDescriptionFlags;

/*
 * The stream description is used to create streaming sound buffers.
 */
typedef struct {
     FSStreamDescriptionFlags flags;        /* Defines which fields are set. */

     int                      buffersize;   /* Ring buffer size specified as
                                               a number of samples (per channel). */
     int                      channels;     /* Number of channels. */
     FSSampleFormat           sampleformat; /* Format of each sample. */
     int                      samplerate;   /* Number of samples per second (per channel). */
     int                      prebuffer;    /* Samples to buffer before starting the playback.
                                               A negative value disables auto start of playback. */
} FSStreamDescription;

/*
 * Information about an IFusionSoundMusicProvider.
 */
typedef enum {
     FMCAPS_BASIC      = 0x00000000,  /* basic ops (PlayTo, Stop)       */
     FMCAPS_SEEK       = 0x00000001,  /* supports SeekTo                */
     FMCAPS_RESAMPLE   = 0x00000002,  /* can resample the audio         */
} FSMusicProviderCapabilities;

/*
 * Information about the status of a music provider.
 */
typedef enum {
     FMSTATE_UNKNOWN   = 0x00000000, /* unknown status       */
     FMSTATE_PLAY      = 0x00000001, /* provider is playing  */
     FMSTATE_STOP      = 0x00000002, /* playback was stopped */
     FMSTATE_FINISHED  = 0x00000003, /* playback is finished */
} FSMusicProviderStatus;

/*
 * Flags controlling playback of a music provider.
 */
typedef enum {
     FMPLAY_NOFX       = 0x000000000, /* normal playback                     */
     FMPLAY_LOOPING    = 0x000000001, /* automatically restart playback when
                                         the end of current track is reached */
} FSMusicProviderPlaybackFlags;

/*
 * Track ID.
 */
typedef unsigned int FSTrackID;

#define FS_TRACK_DESC_ARTIST_LENGTH   32
#define FS_TRACK_DESC_TITLE_LENGTH    125
#define FS_TRACK_DESC_ALBUM_LENGTH    125
#define FS_TRACK_DESC_GENRE_LENGTH    32
#define FS_TRACK_DESC_ENCODING_LENGTH 32

/*
 * Description of a track provided by a music provider.
 */
typedef struct {
     char  artist[FS_TRACK_DESC_ARTIST_LENGTH];     /* Artist */
     char  title[FS_TRACK_DESC_TITLE_LENGTH];       /* Title */
     char  album[FS_TRACK_DESC_ALBUM_LENGTH];       /* Album */
     short year;                                    /* Year */
     char  genre[FS_TRACK_DESC_GENRE_LENGTH];       /* Genre */
     char  encoding[FS_TRACK_DESC_ENCODING_LENGTH]; /* Encoding (for example: MPEG Layer-1) */
     int   bitrate;                                 /* Bitrate in bits/s */
} FSTrackDescription;

/*
 * Called for each track provided by a music provider.
 */
typedef DFBEnumerationResult (*FSTrackCallback) (
     FSTrackID                track_id,
     FSTrackDescription       desc,
     void                    *callbackdata
);

/*
 * <i><b>IFusionSound</b></i> is the main FusionSound interface. Currently it
 * can only be retrieved by calling <i>IDirectFB::GetInterface()</i>. This will
 * change when Fusion and other relevant parts of DirectFB are dumped into a
 * base library.
 *
 * <b>Static sound buffers</b> for smaller samples like sound effects in
 * games or audible feedback in user interfaces are created by calling
 * <i>CreateBuffer()</i>. They can be played several times with an unlimited
 * number of <b>concurrent playbacks</b>. Playback can be started in
 * <b>looping</b> mode. Other per-playback control includes <b>pan value</b>,
 * <b>volume level</b> and <b>pitch</b>.
 *
 * <b>Streaming sound buffers</b> for large or compressed files and for
 * streaming of real time sound data are created by calling
 * <i>CreateStream()</i>. There's only one <b>single playback</b> that
 * automatically starts when data is written to the <b>ring buffer</b> for the
 * first time. If the buffer underruns, the playback automatically stops and
 * continues when the ring buffer is written to again.
 */
DEFINE_INTERFACE( IFusionSound,

   /** Device info **/
   
     /* 
      * Get a description of the sound device.
      */
     DFBResult (*GetDeviceDescription) (
          IFusionSound           *thiz,
          FSDeviceDescription    *ret_desc
     );

   /** Buffers **/

     /*
      * Create a static sound buffer.
      *
      * This requires a <b>desc</b> with at least the length being set.
      *
      * Default values for sample rate, sample format and number of channels
      * are 44kHz, 16 bit (FSSF_S16) with two channels.
      */
     DFBResult (*CreateBuffer) (
          IFusionSound               *thiz,
          const FSBufferDescription  *desc,
          IFusionSoundBuffer        **interface
     );

     /*
      * Create a streaming sound buffer.
      *
      * If <b>desc</b> is NULL, all default values will be used.
      * Defaults are 44kHz, stereo, 16 bit (FSSF_S16) with a ring buffer
      * size that holds enough samples for one second of playback.
      */
     DFBResult (*CreateStream) (
          IFusionSound               *thiz,
          const FSStreamDescription  *desc,
          IFusionSoundStream        **interface
     );

     /*
      * Create a music provider.
      */
     DFBResult (*CreateMusicProvider) (
          IFusionSound               *thiz,
          const char                 *filename,
          IFusionSoundMusicProvider **interface
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
     FSPLAY_REWIND       = 0x00000004,  /* Play backward from the end of the buffer
                                           to the beginning. */
     FSPLAY_ALL          = 0x00000007   /* All of these. */
} FSBufferPlayFlags;


/*
 * <i><b>IFusionSoundBuffer</b></i> represents a static block of sample data.
 *
 * <b>Data access</b> is simply provided by <i>Lock()</i> and <i>Unlock()</i>.
 *
 * There are <b>two ways of playback</b>.
 *
 * <b>Simple playback</b> is provided by this interface. It includes an
 * unlimited number of non-looping playbacks plus one looping playback at a
 * time. Before <b>starting</b> a playback with <i>Play()</i> the application
 * can <b>adjust the pan value</b> with <i>SetPan()</i> and set the FSPLAY_PAN
 * playback flag. To start the <b>looping</b> playback use FSPLAY_LOOPING. It
 * will <b>stop</b> when the interface is destroyed or <i>Stop()</i> is called.
 *
 * <b>Advanced playback</b> is provided by an extra interface called
 * <i>IFusionSoundPlayback</i> which is created by <i>CreatePlayback()</i>.
 * It includes <b>live</b> control over <b>pan</b>, <b>volume</b>, <b>pitch</b>
 * and provides <b>versatile playback commands</b>.
 */
DEFINE_INTERFACE( IFusionSoundBuffer,

   /** Information **/

     /*
      * Get a description of the buffer.
      */
     DFBResult (*GetDescription) (
          IFusionSoundBuffer       *thiz,
          FSBufferDescription      *ret_desc
     );


   /** Access **/

     /*
      * Lock a buffer to access its data.
      *
      * Lock/unlock semantics are weak right now, API will change!
      */
     DFBResult (*Lock) (
          IFusionSoundBuffer       *thiz,
          void                    **data
     );

     /*
      * Unlock a buffer.
      *
      * Lock/unlock semantics are weak right now, API will change!
      */
     DFBResult (*Unlock) (
          IFusionSoundBuffer       *thiz
     );


   /** Simple playback **/

     /*
      * Set panning value.
      *
      * The <b>value</b> ranges from -1.0f (left) to 1.0f (right).
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
      *
      * Each playback instance represents one concurrent playback of the buffer.
      */
     DFBResult (*CreatePlayback) (
          IFusionSoundBuffer       *thiz,
          IFusionSoundPlayback    **interface
     );
)

/*
 * <i><b>IFusionSoundStream</b></i> represents a ring buffer for streamed
 * playback which fairly maps to writing to a sound device. Use it for easy
 * porting of applications that use exclusive access to a sound device.
 *
 * <b>Writing</b> to the ring buffer <b>triggers the playback</b> if it's not
 * already running. The method <i>Write()</i> can be called with an <b>arbitrary
 * number of samples</b>. It returns after all samples have been written to the
 * ring buffer and <b>sleeps</b> while the ring buffer is full.
 * Blocking writes are perfect for accurate filling of the buffer,
 * which keeps the ring buffer as full as possible using a very small block
 * size (depending on sample rate, playback pitch and the underlying hardware).
 *
 * <b>Waiting</b> for a specific amount of <b>free space</b> in the ring buffer
 * is provided by <i>Wait()</i>. It can be used to <b>avoid blocking</b> of
 * <i>Write()</i> or to <b>finish playback</b> before destroying the interface.
 *
 * <b>Status information</b> includes the amount of <b>filled</b> and
 * <b>total</b> space in the ring buffer, along with the current <b>read</b> and
 * <b>write position</b>. It can be retrieved by calling <i>GetStatus()</i> at
 * any time without blocking.
 */
DEFINE_INTERFACE( IFusionSoundStream,

   /** Information **/

     /*
      * Get a description of the stream.
      */
     DFBResult (*GetDescription) (
          IFusionSoundStream       *thiz,
          FSStreamDescription      *ret_desc
     );


   /** Ring buffer **/

     /*
      * Fill the ring buffer with data.
      *
      * Writes the sample <b>data</b> into the ring buffer.
      * The <b>length</b> specifies the number of samples per channel.
      *
      * If the ring buffer gets full, the method blocks until it can write more
      * data.
      *
      * If this method returns successfully, all data has been written.
      */
     DFBResult (*Write) (
          IFusionSoundStream       *thiz,
          const void               *data,
          int                       length
     );

     /*
      * Wait for a specified amount of free ring buffer space.
      *
      * This method blocks until there's free space of at least the specified
      * <b>length</b> (number of samples per channel).
      *
      * Specifying a <b>length</b> of zero waits until playback has finished.
      */
     DFBResult (*Wait) (
          IFusionSoundStream       *thiz,
          int                       length
     );

     /*
      * Query ring buffer status.
      *
      * Returns the number of samples the ring buffer is <b>filled</b> with,
      * the <b>total</b> number of samples that can be stored (buffer size),
      * current <b>read_position</b> and <b>write_position</b> and if the stream
      * is <b>playing</b>.
      *
      * Simply pass NULL for values that are not of interest.
      */
     DFBResult (*GetStatus) (
          IFusionSoundStream       *thiz,
          int                      *filled,
          int                      *total,
          int                      *read_position,
          int                      *write_position,
          DFBBoolean               *playing
     );

     /*
      * Flush the ring buffer.
      *
      * This method stops the playback immediately and 
      * discards any buffered data.
      */
     DFBResult (*Flush) (
          IFusionSoundStream       *thiz
     );

     /*
      * Drop buffered/pending data.
      *
      * This method behaves like <i>Flush()</i>, but it 
      * also discards any pending data.
      */
     DFBResult (*Drop) (
          IFusionSoundStream       *thiz
     );


   /** Timing **/

     /*
      * Query the presentation delay.
      *
      * Returns the amount of time in milli seconds that passes
      * until the last sample stored in the buffer is audible.
      * This includes any buffered data (by hardware or driver)
      * as well as the ring buffer status of the stream.
      *
      * Even if the stream is not playing, e.g. due to pre-buffering,
      * the method behaves as if the playback has just been started.
      */
     DFBResult (*GetPresentationDelay) (
          IFusionSoundStream       *thiz,
          int                      *delay
     );


   /** Playback control **/

     /*
      * Retrieve advanced playback control interface.
      *
      * The returned <b>interface</b> provides advanced control over the playback of the stream.
      * This includes volume, pitch and pan settings as well as manual starting, pausing or
      * stopping of the playback.
      */
     DFBResult (*GetPlayback) (
          IFusionSoundStream       *thiz,
          IFusionSoundPlayback    **interface
     );
)

/*
 * Direction of a playback.
 */
typedef enum {
     FSPD_FORWARD        = +1,
     FSPD_BACKWARD       = -1,
} FSPlaybackDirection;

/*
 * <i><b>IFusionSoundPlayback</b></i> represents one concurrent playback and
 * provides full control over the internal processing of samples.
 *
 * <b>Commands</b> control the playback as in terms of tape transportation.
 * This includes <b>starting</b> the playback at <b>any position</b> with an
 * optional <b>stop position</b>. The default value of <b>zero</b> causes the
 * playback to stop at the <b>end</b>. A <b>negative</b> value puts the playback
 * in <b>looping</b> mode. <i>Start()</i> does <b>seeking</b> if the playback is
 * already running and updates the stop position. Other methods provide
 * <b>pausing</b>, <b>stopping</b> and <b>waiting</b> for the playback to end.
 *
 * <b>Information</b> provided by <i>GetStatus()</i> includes the current
 * <b>position</b> and whether the playback is <b>running</b>.
 *
 * <b>Parameters</b> provide <b>live</b> control over <b>volume</b>, <b>pan</b>
 * and <b>pitch</b> (speed factor) of the playback.
 */
DEFINE_INTERFACE( IFusionSoundPlayback,

   /** Commands **/

     /*
      * Start playback of the buffer.
      *
      * This method is only supported for playback of a buffer.
      * For stream playbacks use <i>Continue()</i>.
      *
      * The <b>start</b> position specifies the sample at which the playback
      * is going to start.
      *
      * The <b>stop</b> position specifies the sample after the last sample
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
      * Continue playback of the buffer or start playback of a stream.
      *
      * This method is used to continue a playback that isn't running (anymore).
      *
      * The playback will begin at the position where it stopped, either
      * explicitly by <i>Stop()</i> or by reaching the stop position.
      *
      * If the playback has never been started, it uses the default start and
      * stop position which means non-looping playback from the beginning
      * to the end.
      *
      * It returns WITHOUT an error if the playback is running.
      *
      * This method can be used to trigger one-at-a-time playback without having
      * to check if it's already running. It's similar to simple playback via
      * <i>IFusionSoundBuffer::Play()</i>, but rejects multiple concurrent
      * playbacks.
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


   /** Information **/

     /*
      * Get the current playback status.
      *
      * This method can be used to check if the playback is <b>running</b>.
      *
      * It also returns the current playback <b>position</b> or the position
      * where <i>Continue()</i> would start to play.
      */
     DFBResult (*GetStatus) (
          IFusionSoundPlayback     *thiz,
          DFBBoolean               *running,
          int                      *position
     );


   /** Parameters **/

     /*
      * Set volume level.
      *
      * The <b>level</b> is a linear factor being 1.0f by default, currently
      * ranges from 0.0f to 256.0f due to internal mixing limitations.
      */
     DFBResult (*SetVolume) (
          IFusionSoundPlayback     *thiz,
          float                     level
     );

     /*
      * Set panning value.
      *
      * The <b>value</b> ranges from -1.0f (left) to 1.0f (right).
      */
     DFBResult (*SetPan) (
          IFusionSoundPlayback     *thiz,
          float                     value
     );

     /*
      * Set pitch value.
      *
      * The <b>value</b> is a linear factor being 1.0f by default, currently
      * ranges from 0.0f to 64.0f due to internal mixing limitations.
      */
     DFBResult (*SetPitch) (
          IFusionSoundPlayback     *thiz,
          float                     value
     );
     
     /*
      * Set the direction of the playback.
      */
     DFBResult (*SetDirection) (
          IFusionSoundPlayback     *thiz,
          FSPlaybackDirection       direction
     );
)

/*
 * Called after each buffer write.
 */
typedef void (*FMBufferCallback)( int length, void *ctx );

/*
 * <i>No summary yet...</i>
 */
DEFINE_INTERFACE(   IFusionSoundMusicProvider,
   /** Retrieving information **/

     /*
      * Retrieve information about the music provider's
      * capabilities.
      */
     DFBResult (*GetCapabilities) (
          IFusionSoundMusicProvider   *thiz,
          FSMusicProviderCapabilities *ret_caps
     );

     /*
      * Enumerate all tracks contained in the file.
      *
      * Calls the given callback for all available tracks.<br>
      * The callback is passed the track id that can be
      * used to select a track for playback using 
      * IFusionSoundMusicProvider::SelectTrack().
      */
     DFBResult (*EnumTracks) (
          IFusionSoundMusicProvider *thiz,
          FSTrackCallback            callback,
          void                      *callbackdata
     );

     /*
      * Get the unique ID of the current track.
      */
     DFBResult (*GetTrackID) (
          IFusionSoundMusicProvider *thiz,
          FSTrackID                 *ret_track_id
     );

     /*
      * Get a description of the current track.
      */
     DFBResult (*GetTrackDescription) (
          IFusionSoundMusicProvider *thiz,
          FSTrackDescription        *ret_desc
     );

     /*
      * Get a stream description that best matches the music
      * contained in the file.
      */
     DFBResult (*GetStreamDescription) (
          IFusionSoundMusicProvider *thiz,
          FSStreamDescription       *ret_desc
     );
     
     /*
      * Get a buffer description that best matches the music
      * contained in the file.
      */
     DFBResult (*GetBufferDescription) (
          IFusionSoundMusicProvider *thiz,
          FSBufferDescription       *ret_desc
     );

   /** Playback **/

     /*
      * Select a track by its unique ID.
      */
     DFBResult (*SelectTrack) (
          IFusionSoundMusicProvider *thiz,
          FSTrackID                  track_id 
     );
     
     /*
      * Play selected track rendering it into
      * the destination stream.
      */
     DFBResult (*PlayToStream) (
          IFusionSoundMusicProvider *thiz,
          IFusionSoundStream        *destination
     );

     /*
      * Play selected track rendering it into
      * the destination buffer.
      *
      * Optionally a callback can be registered
      * that is called after each buffer write.<br>
      * The callback is passed the number of 
      * samples per channels actually written
      * to the destination buffer.
      */
     DFBResult (*PlayToBuffer) (
          IFusionSoundMusicProvider *thiz,
          IFusionSoundBuffer        *destination,
          FMBufferCallback           callback,
          void                      *ctx
     );

     /*
      * Stop playback.
      */
     DFBResult (*Stop) (
          IFusionSoundMusicProvider *thiz
     );
     
     /*
      * Get playback status.
      */
     DFBResult (*GetStatus) (
          IFusionSoundMusicProvider *thiz,
          FSMusicProviderStatus     *ret_status
     );

   /** Media Control **/

     /*
      * Seeks to a position within the current track.
      */
     DFBResult (*SeekTo) (
          IFusionSoundMusicProvider *thiz,
          double                     seconds
     );

     /*
      * Gets current position within the current track.
      */
     DFBResult (*GetPos) (
          IFusionSoundMusicProvider *thiz,
          double                    *ret_seconds
     );

     /*
      * Gets the length of the current track.
      */
     DFBResult (*GetLength) (
          IFusionSoundMusicProvider *thiz,
          double                    *ret_seconds
     );
     
   /** Advanced Playback **/
     
     /*
      * Set the flags controlling playback.
      */
     DFBResult (*SetPlaybackFlags) (
          IFusionSoundMusicProvider    *thiz,
          FSMusicProviderPlaybackFlags  flags
     );
)

#ifdef __cplusplus
}
#endif

#endif


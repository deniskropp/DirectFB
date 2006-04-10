/*
 * Copyright (C) 2005-2006 Claudio Ciccani <klan@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_CDDB
# include <cddb/cddb.h>
#endif

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/thread.h>
#include <direct/util.h>

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, CDDA )


struct cdda_track {
     unsigned int  start;  /* first frame   */
     unsigned int  length; /* total frames  */
     unsigned int  frame;  /* current frame */
     
     char         *artist;
     char         *title;
     char         *genre;
     char         *album;
     short         year;
};

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                      ref;        /* reference counter */
     
     int                      fd;

     unsigned int             current_track;
     unsigned int             total_tracks;
     struct cdda_track       *tracks;
     
     DirectThread            *thread;
     pthread_mutex_t          lock;
     bool                     playing;
     bool                     finished;
     
     int                      buffered_frames;
     __s16                   *src_buffer;
     __u8                    *dst_buffer;
     
     struct {
          IFusionSoundStream *stream;
          IFusionSoundBuffer *buffer;
          int                 format;
          int                 channels;
          int                 length;
     } dest;

     FMBufferCallback         callback;
     void                    *ctx;
} IFusionSoundMusicProvider_CDDA_data;



#define CD_FRAMES_PER_SECOND  75
#define CD_BYTES_PER_FRAME    2352


#if defined(__linux__)
 
#include <linux/cdrom.h>

static DFBResult
cdda_probe( int fd )
{
     struct cdrom_tochdr tochdr;

     if (ioctl( fd, CDROM_DRIVE_STATUS, CDSL_CURRENT ) != CDS_DISC_OK)
          return DFB_UNSUPPORTED;
     
     if (ioctl( fd, CDROMREADTOCHDR, &tochdr ) < 0)
          return DFB_UNSUPPORTED;
     
     return DFB_OK;
}

static DFBResult
cdda_build_tracklits( int fd, struct cdda_track **ret_tracks,
                              unsigned int       *ret_num )
{
     int                        total_tracks;
     struct cdda_track         *tracks, *track;
     struct cdrom_tochdr        tochdr;
     struct cdrom_tocentry      tocentry;
     struct cdrom_multisession  ms;
     struct cdrom_msf0          msf;
     int                        i;
     
     /* read TOC header */
     if (ioctl( fd, CDROMREADTOCHDR, &tochdr ) == -1) {
          D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                    "ioctl( CDROMREADTOCHDR ) failed!\n" );
          return DFB_IO;
     }
     
     ms.addr_format = CDROM_LBA;
     if (ioctl( fd, CDROMMULTISESSION, &ms ) == -1) {
          D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                    "ioctl( CDROMMULTISESSION ) failed!\n" );
          return DFB_IO;
     }
     
     total_tracks = tochdr.cdth_trk1 - tochdr.cdth_trk0 + 1;
          
     tracks = D_CALLOC( total_tracks+1, sizeof(struct cdda_track) );
     if (!tracks)
          return D_OOM();
     
     /* iterate through tracks */
     for (i = tochdr.cdth_trk0, track = tracks; i <= tochdr.cdth_trk1; i++) {          
          memset( &tocentry, 0, sizeof(tocentry) );
          
          tocentry.cdte_track  = i;
          tocentry.cdte_format = CDROM_MSF;
          if (ioctl( fd, CDROMREADTOCENTRY, &tocentry ) == -1) {
               D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                         "ioctl( CDROMREADTOCENTRY ) failed!\n" );
               D_FREE( tracks );
               return DFB_IO;
          }
          
          /* skip data tracks */
          if (tocentry.cdte_ctrl & 0x04) {
               total_tracks--;
               continue;
          }
          
          msf = tocentry.cdte_addr.msf;
          track->start = (msf.minute * 60 * CD_FRAMES_PER_SECOND) +
                         (msf.second      * CD_FRAMES_PER_SECOND) +
                          msf.frame;
          track++;
     }
     
     if (total_tracks < 1) {
          D_FREE( tracks );
          return DFB_FAILURE;
     }
     
     memset( &tocentry, 0, sizeof(tocentry) );
     
     /* get leadout track */
     tocentry.cdte_track  = 0xaa;
     tocentry.cdte_format = CDROM_MSF;
     if (ioctl( fd, CDROMREADTOCENTRY, &tocentry ) == -1) {
          D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                    "ioctl( CDROMREADTOCENTRY ) failed!\n" );
          D_FREE( tracks );
          return DFB_IO;
     }          
     
     if (!ms.xa_flag) {
          msf = tocentry.cdte_addr.msf;
          tracks[total_tracks].start = (msf.minute * 60 * CD_FRAMES_PER_SECOND) +
                                       (msf.second      * CD_FRAMES_PER_SECOND) +
                                        msf.frame;
     } else {
          tracks[total_tracks].start = ms.addr.lba - ((60 + 90 + 2) * CD_FRAMES) + 150;
     }
     
     /* compute tracks' length */
     for (i = 0; i < total_tracks; i++)
          tracks[i].length = tracks[i+1].start - tracks[i].start;
          
     *ret_tracks = tracks;
     *ret_num    = total_tracks;
     
     return DFB_OK;
}

static int
cdda_read_audio( int fd, __u8 *buf, int pos, int len )
{
     struct cdrom_read_audio ra;
     
     ra.addr.msf.minute =  pos / (60 * CD_FRAMES_PER_SECOND);
     ra.addr.msf.second = (pos / CD_FRAMES_PER_SECOND) % 60;
     ra.addr.msf.frame  =  pos % CD_FRAMES_PER_SECOND;
     ra.addr_format     = CDROM_MSF;
     ra.nframes         = len;
     ra.buf             = buf;
          
     if (ioctl( fd, CDROMREADAUDIO, &ra ) < 0) {
          D_PERROR( "IFusionSoundMusicProvider_CDDA: "
                    "ioctl( CDROMREADAUDIO ) failed!\n" );
          return -1;
     }
     
     return ra.nframes;
}

#elif defined(__FreeBSD__)

#include <sys/cdio.h>

static DFBResult
cdda_probe( int fd )
{
     struct ioc_toc_header tochdr;
     
     if (ioctl( fd, CDIOREADTOCHEADER, &tochdr ) < 0)
          return DFB_UNSUPPORTED;
          
     return DFB_OK;
}

static DFBResult
cdda_build_tracklits( int fd, struct cdda_track **ret_tracks,
                              unsigned int       *ret_num )
{
     int                               total_tracks;
     struct cdda_track                *tracks, *track;
     struct ioc_toc_header             tochdr;
     struct ioc_read_toc_single_entry  tocentry;
     union msf_lba                     msf_lba;
     int                               i;
     
     /* read TOC header */
     if (ioctl( fd, CDIOREADTOCHEADER, &tochdr ) == -1) {
          D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                    "ioctl( CDIOREADTOCHEADER ) failed!\n" );
          return DFB_IO;
     }
     
     total_tracks = tochdr.ending_track - tochdr.starting_track + 1;
          
     tracks = D_CALLOC( total_tracks+1, sizeof(struct cdda_track) );
     if (!tracks)
          return D_OOM();
     
     /* iterate through tracks */
     for (i = tochdr.starting_track, track = tracks; i <= tochdr.ending_track; i++) {          
          memset( &tocentry, 0, sizeof(tocentry) );
          
          tocentry.track          = i;
          tocentry.address_format = CD_MSF_FORMAT;
          if (ioctl( fd, CDIOREADTOCENTRY, &tocentry ) == -1) {
               D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                         "ioctl( CDIOREADTOCENTRY ) failed!\n" );
               D_FREE( tracks );
               return DFB_IO;
          }
          
          /* skip data tracks */
          if (tocentry.entry.control & 0x04) {
               total_tracks--;
               continue;
          }
          
          msf_lba = tocentry.entry.addr;
          track->start = (msf_lba.msf.minute * 60 * CD_FRAMES_PER_SECOND) +
                         (msf_lba.msf.second      * CD_FRAMES_PER_SECOND) +
                          msf_lba.msf.frame;
          track++;
     }
     
     if (total_tracks < 1) {
          D_FREE( tracks );
          return DFB_FAILURE;
     }
     
     memset( &tocentry, 0, sizeof(tocentry) );
     
     /* get leadout track */
     tocentry.track          = 0xaa;
     tocentry.address_format = CD_MSF_FORMAT;
     if (ioctl( fd, CDIOREADTOCENTRY, &tocentry ) == -1) {
          D_PERROR( "IFusionSoundMusicProvide_CDDA: "
                    "ioctl( CDIOREADTOCENTRY ) failed!\n" );
          D_FREE( tracks );
          return DFB_IO;
     }          
     
     msf_lba = tocentry.entry.addr;
     tracks[total_tracks].start = (msf_lba.msf.minute * 60 * CD_FRAMES_PER_SECOND) +
                                  (msf_lba.msf.second      * CD_FRAMES_PER_SECOND) +
                                   msf_lba.msf.frame;
     
     /* compute tracks' length */
     for (i = 0; i < total_tracks; i++)
          tracks[i].length = tracks[i+1].start - tracks[i].start;
          
     *ret_tracks = tracks;
     *ret_num    = total_tracks;
     
     return DFB_OK;
}

static int
cdda_read_audio( int fd, __u8 *buf, int pos, int len )
{
     struct ioc_read_audio ra;
     
     ra.address.msf.minute =  pos / (60 * CD_FRAMES_PER_SECOND);
     ra.address.msf.second = (pos / CD_FRAMES_PER_SECOND) % 60;
     ra.address.msf.frame  =  pos % CD_FRAMES_PER_SECOND;
     ra.address_format     = CD_MSF_FORMAT;
     ra.nframes            = len;
     ra.buffer             = buf;
          
     if (ioctl( fd, CDIOCREADAUDIO, &ra ) < 0) {
          D_PERROR( "IFusionSoundMusicProvider_CDDA: "
                    "ioctl( CDIOCREADAUDIO ) failed!\n" );
          return -1;
     }
     
     return ra.nframes;
}

#else

static DFBResult
cdda_probe( int fd )
{
     D_WARN( "unsupported system" );
     return DFB_UNSUPPORTED;
}

static DFBResult
cdda_build_tracklits( int fd, struct cdda_track **ret_tracks,
                              unsigned int       *ret_num )
{
     return DFB_UNSUPPORTED;
}

static int
cdda_read_audio( int fd, __u8 *buf, int pos, int frames )
{
     return -1;
}

#endif

#ifdef HAVE_CDDB     
static void
cdda_get_metadata( struct cdda_track *tracks,
                   unsigned int       total_tracks )
{
     const char   *cddb_cats[] = { "blues", "classical", "country", "data",
                                   "folk", "jazz", "misc", "newage", "reggae",
                                   "rock", "soundtrack" };
     cddb_conn_t  *conn;
     cddb_disc_t  *disc;
     cddb_track_t *track;
     unsigned int  disclen;
     unsigned int  discid  = 0;
     int           i;
     
     /* init libcddb */
     libcddb_init();
     
     /* create a new connection */
     conn = cddb_new();
     if (!conn)
          return;
     /* suppress messages */
     cddb_log_set_level( CDDB_LOG_NONE );
     /* set timeout to 10 seconds */
     cddb_set_timeout( conn, 10 );

     /* compute disc length */
     disclen = tracks[total_tracks].start/CD_FRAMES_PER_SECOND - 
               tracks[0].start/CD_FRAMES_PER_SECOND;
     
     /* compute disc id */
     for (i = 0; i < total_tracks; i++) {
          unsigned int start = tracks[i].start/CD_FRAMES_PER_SECOND;
          
          while (start) {
               discid += start % 10;
               start  /= 10;
          }
     }    
     discid = ((discid % 0xff) << 24) | (disclen << 8) | total_tracks;
     
     D_DEBUG( "IFusionSoundMusicProvider_CDDA: CDDB Disc ID = 0x%08x.\n", discid );
     
     /* create a new disc */
     disc = cddb_disc_new();
     if (!disc) {
          cddb_destroy( conn );
          return;
     }

     /* set disc id */ 
     cddb_disc_set_discid( disc, discid );
     
     /* search through categories */
     for (i = 0; i < sizeof(cddb_cats)/sizeof(cddb_cats[0]); i++) {
          cddb_disc_set_category_str( disc, cddb_cats[i] );
          
          /* retrieve informations from the server */
          if (cddb_read( conn, disc )) {
               const char *artist;
               const char *title;
               const char *genre  = cddb_disc_get_genre( disc );
               const char *album  = cddb_disc_get_title( disc );
               short       year   = cddb_disc_get_year ( disc );
          
               /* iterate through tracks */
               for (track = cddb_disc_get_track_first( disc );
                    track != NULL;
                    track = cddb_disc_get_track_next( disc ))
               {
                    i = cddb_track_get_number( track ) - 1;
               
                    if (i < total_tracks) {
                         artist = cddb_track_get_artist( track );
                         title  = cddb_track_get_title( track );
                    
                         if (artist)
                              tracks[i].artist = D_STRDUP( artist );
                         if (title)
                          tracks[i].title  = D_STRDUP( title );
                         if (genre)
                              tracks[i].genre  = D_STRDUP( genre );
                         if (album)
                              tracks[i].album  = D_STRDUP( album );
                         tracks[i].year = year;
                    }
               }
               
               break;
          }
     }
     
     /* release resources */
     cddb_disc_destroy( disc );
     cddb_destroy( conn );
     libcddb_shutdown();
}
#endif                

static void
cdda_mix_audio( __s16 *src, __u8 *dst, int len, int format, int channels )
{
     int i;
     
     switch (format) {
          case 8:
               if (channels == 1) {
                    for (i = 0; i < len*2; i += 2) {
                         *dst = ((src[i] + src[i+1]) >> 9) + 128;
                         dst++;
                    }
               } else {
                    for (i = 0; i < len*2; i++)
                         dst[i] = (src[i] >> 8) + 128;
               }
               break;
          case 16:
               if (channels == 1) {
                    for (i = 0; i < len*2; i += 2) {
                         *((__s16*)dst) = (src[i] + src[i+1]) >> 1;
                         dst += 2;
                    }
               }
               break;
          case 24:
               if (channels == 1) {
                    for (i = 0; i < len*2; i += 2) {
                         int d = (src[i] + src[i+1]) << 7;
                         dst[0] = d;
                         dst[1] = d >> 8;
                         dst[2] = d >> 16;
                         dst += 3;
                    }
               } else {
                    for (i = 0; i < len; i++) {
                         int d = src[i] << 8;
                         dst[0] = d;
                         dst[1] = d >> 8;
                         dst[2] = d >> 16;
                         dst += 3;
                    }
               }
               break;
          case 32:
               if (channels == 1) {
                    for (i = 0; i < len*2; i += 2) {
                         *((__s32*)dst) = (src[i] + src[i+1]) << 15;
                         dst += 4;
                    }
               } else {
                    for (i = 0; i < len; i++)
                         ((__s32*)dst)[i] = src[i] << 16;
               }
               break;   
          default:
               D_BUG( "unexpected sampleformat" );
               break;
     }
}


static void
IFusionSoundMusicProvider_CDDA_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_CDDA_data *data =
         (IFusionSoundMusicProvider_CDDA_data*)thiz->priv;
     int i;

     thiz->Stop( thiz );
    
     close( data->fd );

     pthread_mutex_destroy( &data->lock );
     
     for (i = 0; i < data->total_tracks; i++) {
          if (data->tracks[i].artist)
               D_FREE( data->tracks[i].artist );
          if (data->tracks[i].title)
               D_FREE( data->tracks[i].title );
          if (data->tracks[i].genre)
               D_FREE( data->tracks[i].genre );
          if (data->tracks[i].album)
               D_FREE( data->tracks[i].album );
     }
     
     D_FREE( data->tracks );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_CDDA_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (--data->ref == 0)
          IFusionSoundMusicProvider_CDDA_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!caps)
          return DFB_INVARG;

     *caps = FMCAPS_BASIC | FMCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_EnumTracks( IFusionSoundMusicProvider *thiz,
                                           FSTrackCallback            callback,
                                           void                      *callbackdata )
{
     FSTrackDescription desc;
     int                i;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!callback)
          return DFB_INVARG;
    
     for (i = 0; i < data->total_tracks; i++) {
          struct cdda_track *track = &data->tracks[i];
          
          memset( &desc, 0, sizeof(desc) );
          
          if (track->artist) {
               snprintf( desc.artist,
                         FS_TRACK_DESC_ARTIST_LENGTH, track->artist );
          }
          if (track->title) {
               snprintf( desc.title,
                         FS_TRACK_DESC_TITLE_LENGTH, track->title );  
          }
          if (track->genre) {
               snprintf( desc.genre,
                         FS_TRACK_DESC_GENRE_LENGTH, track->genre );
          }
          if (track->album) {
               snprintf( desc.album,
                         FS_TRACK_DESC_ALBUM_LENGTH, track->album );
          }
          desc.year = track->year;                
               
          snprintf( desc.encoding,
                    FS_TRACK_DESC_ENCODING_LENGTH, "PCM 16 bit" );
          desc.bitrate = CD_FRAMES_PER_SECOND * CD_BYTES_PER_FRAME * 8;

          if (callback( i, desc, callbackdata ) != DFENUM_OK)
               break;
     }

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_GetTrackID( IFusionSoundMusicProvider *thiz,
                                           FSTrackID                 *ret_track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!ret_track_id)
          return DFB_INVARG;

     *ret_track_id = data->current_track;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                    FSTrackDescription        *desc )
{
     struct cdda_track *track;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!desc)
          return DFB_INVARG;
          
     memset( desc, 0, sizeof(FSTrackDescription) );
     
     track = &data->tracks[data->current_track];
          
     if (track->artist) {
          snprintf( desc->artist,
                    FS_TRACK_DESC_ARTIST_LENGTH, track->artist );
     }
     if (track->title) {
          snprintf( desc->title,
                    FS_TRACK_DESC_TITLE_LENGTH, track->title );  
     }
     if (track->genre) {
          snprintf( desc->genre,
                    FS_TRACK_DESC_GENRE_LENGTH, track->genre );
     }
     if (track->album) {
          snprintf( desc->album,
                    FS_TRACK_DESC_ALBUM_LENGTH, track->album );
     }
     desc->year = track->year;
      
     snprintf( desc->encoding,
               FS_TRACK_DESC_ENCODING_LENGTH, "PCM 16 bit" );
     desc->bitrate = CD_FRAMES_PER_SECOND * CD_BYTES_PER_FRAME * 8;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                     FSStreamDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!desc)
          return DFB_INVARG;
 
     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = 44100;
     desc->channels     = 2;
     desc->sampleformat = FSSF_S16;
     desc->buffersize   = 5292; /* 120 ms */

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                     FSBufferDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!desc)
          return DFB_INVARG;
 
     desc->flags        = FSBDF_LENGTH       | FSBDF_CHANNELS  |
                          FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE;
     desc->samplerate   = 44100;
     desc->channels     = 2;
     desc->sampleformat = FSSF_S16;
     desc->length       = 5292; /* 120 ms */

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_SelectTrack( IFusionSoundMusicProvider *thiz,
                                            FSTrackID                  track_id )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (track_id > data->total_tracks)
          return DFB_INVARG;
     
     pthread_mutex_lock( &data->lock );     
     data->tracks[data->current_track].frame = 0;
     data->current_track = track_id;
     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
CDDAStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_CDDA_data *data = 
          (IFusionSoundMusicProvider_CDDA_data*) ctx;
     
     while (data->playing) {
          int len, pos;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          pos = data->tracks[data->current_track].frame;
          len = MIN( data->buffered_frames, 
                     data->tracks[data->current_track].length - pos );
          if (len <= 0) {
               data->finished = true;
               pthread_mutex_unlock( &data->lock );
               break;
          }
          pos += data->tracks[data->current_track].start;          
          
          len = cdda_read_audio( data->fd, (__u8*)data->src_buffer, pos, len );
          if (len < 1) {
               data->finished = true;
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          data->tracks[data->current_track].frame += len;

          pthread_mutex_unlock( &data->lock );
          
          len = len * CD_BYTES_PER_FRAME / 4;
                   
          cdda_mix_audio( data->src_buffer, data->dst_buffer, len, 
                          data->dest.format, data->dest.channels);   

          data->dest.stream->Write( data->dest.stream, 
                                    data->dst_buffer, len );
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_PlayToStream( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundStream        *destination )
{
     FSStreamDescription  desc;
     __u32                dst_format = 0;
     int                  src_size   = 0;
     int                  dst_size   = 0;
     void                *buffer;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != 44100)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
               dst_format = FS_BITS_PER_SAMPLE(desc.sampleformat);
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     /* check destination buffer size */
     if (desc.buffersize < CD_BYTES_PER_FRAME/4)
          return DFB_UNSUPPORTED;

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     /* release buffer */
     if (data->src_buffer) {
          D_FREE( data->src_buffer );
          data->src_buffer = NULL;
          data->dst_buffer = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }
     
     data->buffered_frames = (desc.buffersize << 2) / CD_BYTES_PER_FRAME;
     
     src_size = data->buffered_frames * CD_BYTES_PER_FRAME;
     if (dst_format != 16 || desc.channels != 2)
          dst_size = desc.buffersize * desc.channels * dst_format >> 3;
          
     buffer = D_MALLOC( src_size + dst_size );
     if (!buffer) {
          pthread_mutex_unlock( &data->lock );
          return D_OOM();
     }
     
     data->src_buffer = buffer;
     data->dst_buffer = (dst_size) ? (buffer + src_size) : buffer;
 
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.stream   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.buffersize;
    
     if (data->finished) {
          struct cdda_track *track = &data->tracks[data->current_track];
          track->frame = track->start;
          data->finished = false;
     } 
    
     /* start thread */
     data->playing  = true;
     data->thread   = direct_thread_create( DTT_DEFAULT,
                                            CDDAStreamThread, data, "CDDA" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
CDDABufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_CDDA_data *data = 
          (IFusionSoundMusicProvider_CDDA_data*) ctx;
     
     IFusionSoundBuffer *buffer = data->dest.buffer;
     
     while (data->playing) {
          int   len, pos;
          __u8 *dst;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          pos = data->tracks[data->current_track].frame;
          len = MIN( data->buffered_frames, 
                     data->tracks[data->current_track].length - pos );
          if (len <= 0) {
               data->finished = true;
               pthread_mutex_unlock( &data->lock );
               break;
          }
          pos += data->tracks[data->current_track].start;
          
          if (buffer->Lock( buffer, (void*)&dst ) != DFB_OK) {
               D_ERROR( "IFusionSoundMusicProvider_CDDA: "
                        "Couldn't lock destination buffer!\n" );
               data->finished = true;
               pthread_mutex_unlock( &data->lock );
               break;
          }          
          
          len = cdda_read_audio( data->fd,
                                 (__u8*)data->src_buffer ? : dst, pos, len );
          if (len < 1) {
               data->finished = true;
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          data->tracks[data->current_track].frame += len;

          pthread_mutex_unlock( &data->lock );
          
          len = len * CD_BYTES_PER_FRAME / 4;
          
          if (data->src_buffer) {          
               cdda_mix_audio( data->src_buffer, dst, len, 
                               data->dest.format, data->dest.channels );
          }
          
          buffer->Unlock( buffer );

          if (data->callback)
               data->callback( len, data->ctx );
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundBuffer        *destination,
                                             FMBufferCallback           callback,
                                             void                      *ctx )
{
     FSBufferDescription  desc;
     __u32                dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != 44100)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
               dst_format = FS_BITS_PER_SAMPLE(desc.sampleformat);
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     /* check destination buffer size */
     if (desc.length < CD_BYTES_PER_FRAME/4)
          return DFB_UNSUPPORTED;

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     /* release buffer */
     if (data->src_buffer) {
          D_FREE( data->src_buffer );
          data->src_buffer = NULL;
          data->dst_buffer = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }
     
     data->buffered_frames = (desc.length << 2) / CD_BYTES_PER_FRAME;
     
     if (dst_format != 16 || desc.channels != 2) {
          data->src_buffer = D_MALLOC( data->buffered_frames * CD_BYTES_PER_FRAME );
          if (!data->src_buffer) {
               pthread_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }
 
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.buffer   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.length;
     data->callback      = callback;
     data->ctx           = ctx;

     if (data->finished) {
          struct cdda_track *track = &data->tracks[data->current_track];
          track->frame = track->start;
          data->finished = false;
     } 
     
     /* start thread */
     data->playing  = true;
     data->thread   = direct_thread_create( DTT_DEFAULT,
                                            CDDABufferThread, data, "CDDA" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_CDDA_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     /* release buffer */
     if (data->src_buffer) {
          D_FREE( data->src_buffer );
          data->src_buffer = NULL;
          data->dst_buffer = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_CDDA_SeekTo( IFusionSoundMusicProvider *thiz,
                                       double                     seconds )
{
     struct cdda_track *track;
     unsigned int       frame;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (seconds < 0.0)
          return DFB_INVARG;

     track = &data->tracks[data->current_track];
     frame = seconds * CD_FRAMES_PER_SECOND;
     if (frame >= track->length) {
          frame = track->length;
          data->finished = true;
     } else {
          data->finished = false;
     }

     pthread_mutex_lock( &data->lock );
     track->frame = frame;
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_CDDA_GetPos( IFusionSoundMusicProvider *thiz,
                                       double                    *seconds )
{
     struct cdda_track *track;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )

     if (!seconds)
          return DFB_INVARG;
          
     track = &data->tracks[data->current_track];

     if (data->finished) {
          *seconds = (double)track->length / CD_FRAMES_PER_SECOND;
          return DFB_EOF;
     }

     *seconds = (double)track->frame / CD_FRAMES_PER_SECOND;
          
     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_CDDA_GetLength( IFusionSoundMusicProvider *thiz,
                                          double                    *seconds )
{
     struct cdda_track *track;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_CDDA )
     
     if (!seconds)
          return DFB_INVARG;
      
     track = &data->tracks[data->current_track];   
     *seconds = (double)track->length / CD_FRAMES_PER_SECOND;

     return DFB_OK;
}


/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     int       fd;
     DFBResult ret;
     
     fd = open( ctx->filename, O_RDONLY | O_NONBLOCK );
     if (fd < 0)
          return DFB_UNSUPPORTED;
      
     ret = cdda_probe( fd );
     close( fd );

     return ret;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     DFBResult err;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_CDDA )

     data->ref = 1;
     
     data->fd = open( filename, O_RDONLY | O_NONBLOCK );
     if (data->fd < 0)
          return DFB_IO;
    
     err = cdda_build_tracklits( data->fd, &data->tracks, &data->total_tracks );
     if (err != DFB_OK) {
          close( data->fd );
          return err;
     }
     
#ifdef HAVE_CDDB
     cdda_get_metadata( data->tracks, data->total_tracks );
#endif     
     
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_CDDA_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_CDDA_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_CDDA_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_CDDA_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_CDDA_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_CDDA_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_CDDA_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_CDDA_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_CDDA_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_CDDA_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_CDDA_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_CDDA_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_CDDA_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_CDDA_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_CDDA_GetLength;

     return DFB_OK;
}


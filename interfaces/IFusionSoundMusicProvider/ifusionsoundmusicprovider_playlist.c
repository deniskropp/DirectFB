/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
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
#include <ctype.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/stream.h>
#include <direct/util.h>


static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Playlist )


typedef enum {
     PLT_NONE = 0,
     PLT_M3U,
     PLT_RAM,
     PLT_PLS,
     PLT_SMIL
} PlaylistType;

typedef struct {
     DirectLink                 link;
     
     FSTrackID                  id;
     
     char                      *url;
     char                      *artist;
     char                      *title;
     
     double                     start;
     
     IFusionSoundMusicProvider *provider;
} PlaylistEntry;

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                           ref;        /* reference counter */
     
     PlaylistEntry                *playlist;
     PlaylistEntry                *selected;
     
     IFusionSoundStream           *stream;
     IFusionSoundBuffer           *buffer;
     
     FMBufferCallback              callback;
     void                         *callback_ctx;
     
     FSMusicProviderPlaybackFlags  playback_flags;
} IFusionSoundMusicProvider_Playlist_data;

/*****************************************************************************/

static DFBResult
add_media( FSTrackID       id, 
           const char     *url, 
           const char     *artist, 
           const char     *title,
           double          start,
           PlaylistEntry **playlist )
{
     PlaylistEntry *entry;
     
     D_ASSERT( url != NULL );
     D_ASSERT( playlist != NULL );
     
     entry = D_CALLOC( 1, sizeof(PlaylistEntry) );
     if (!entry)
          return D_OOM();
          
     entry->id = id;
     entry->url = D_STRDUP( url );
     if (artist && *artist)
          entry->artist = D_STRDUP( artist );
     if (title && *title)
          entry->title = D_STRDUP( title );
     entry->start = start;
     
     direct_list_append( (DirectLink**)playlist, &entry->link );
     
     return DFB_OK;
}

static void
remove_media( PlaylistEntry *entry, PlaylistEntry **playlist )
{
     D_ASSERT( entry != NULL );
     D_ASSERT( playlist != NULL );
     
     direct_list_remove( (DirectLink**)playlist, &entry->link );
          
     if (entry->url)
          D_FREE( entry->url );
     if (entry->artist)
          D_FREE( entry->artist );
     if (entry->title)
          D_FREE( entry->title );
     if (entry->provider)
          entry->provider->Release( entry->provider );
          
     D_FREE( entry );
}
 
/*****************************************************************************/   

static DFBResult 
fetch_line( DirectStream *stream, char buf[], int len )
{
     DFBResult ret;
     int       i = 0;
     
     do {
          direct_stream_wait( stream, 1, NULL );
          ret = direct_stream_read( stream, 1, &buf[i++], NULL );
          if (ret) {
               if (ret == DFB_EOF && i > 1)
                    break;
               return ret;
          }
     } while (i < len && buf[i-1] != '\n');
     
     buf[i-1] = '\0';
     if (i > 1 && buf[i-2] == '\r')
          buf[i-2] = '\0';
     
     return DFB_OK;
}

static DFBResult
fetch_tag( DirectStream *stream, char buf[], int len )
{
     DFBResult ret;
     int       i = 0;
     char      c;
     
     do {
          direct_stream_wait( stream, 1, NULL );
          ret = direct_stream_read( stream, 1, &c, NULL );
          if (ret)
               return ret;
     } while (c != '<');
     
     do {
          direct_stream_wait( stream, 1, NULL );
          ret = direct_stream_read( stream, 1, &buf[i++], NULL );
          if (ret)
               return ret;
     } while (i < len && buf[i-1] != '>');
     
     buf[i-1] = '\0';
     if (i > 1 && buf[i-2] == '/')
          buf[i-2] = '\0';
          
     return DFB_OK;
}    

/*****************************************************************************/

static PlaylistType 
get_playlist_type( const char *mimetype, 
                   const char *filename,
                   const char *header )
{
     if (mimetype) {
          if (!strcmp( mimetype, "audio/mpegurl" ) ||
              !strcmp( mimetype, "audio/x-mpegurl" ))
               return PLT_M3U;
          
          if (!strcmp( mimetype, "audio/x-scpls" ))
               return PLT_PLS;
               
          if (!strcmp( mimetype, "application/smil" ))
               return PLT_SMIL;
     }
     
     if (filename) {
          char *ext = strrchr( filename, '.' );
          if (ext) {
               if (!strcasecmp( ext, ".m3u" ))
                    return PLT_M3U;
                    
               if (!strcasecmp( ext, ".ram" ) || 
                   !strcasecmp( ext, ".rpm" ))
                    return PLT_RAM;
                    
               if (!strcasecmp( ext, ".pls" ))
                    return PLT_PLS;
                    
               if (!strcasecmp( ext, ".smi" ) ||
                   !strcasecmp( ext, ".smil" ))
                    return PLT_SMIL;
          }
     }
     
     if (header) {
          if (!strncmp( header, "#EXTM3U", 7 ))
               return PLT_M3U;
               
          if (!strncmp( header, "[Playlist]", 10 ))
               return PLT_PLS;
               
          if (!strncmp( header, "<smil", 5 ))
               return PLT_SMIL;
     } 
     
     return PLT_NONE;
}     
               
static void
m3u_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, 
                    DirectStream                            *stream )
{
     char         buf[4096];
     char         title[1024] = { 0, };
     unsigned int id = 0;     
     
     while (fetch_line( stream, buf, sizeof(buf) ) == DFB_OK) {          
          if (buf[0] == '\0')
               continue;
               
          if (buf[0] == '#') {
               if (!strncmp( buf+1, "EXTINF:", 7 )) {
                    char *tmp = strchr( buf+8, ',' );
                    if (tmp)
                         snprintf( title, sizeof(title), "%s", ++tmp );
               }
               continue;
          }

          add_media( id++, buf, NULL, title, 0, &data->playlist );
          *title = '\0';
     }
}

static void
ram_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, 
                    DirectStream                            *stream )
{
     char         buf[4096];
     unsigned int id = 0;     
     
     while (fetch_line( stream, buf, sizeof(buf) ) == DFB_OK) {
          char *info, *artist = NULL, *title = NULL;
                    
          if (buf[0] == '\0' || buf[0] == '#')
               continue;
               
          if (!strncmp( buf, "--stop--", 8 ))
               break;

          info = strrchr( buf, '?' );
          if (info) {
               char *tmp;
               
               *info++ = '\0';
               artist = strstr( info, "artist=" );
               title  = strstr( info, "title=" );
               
               if (artist) {
                    artist += 7;
                    tmp = strchr( artist, '&' );
                    if (tmp) *tmp = '\0';
               }
               if (title) {
                    title += 6;
                    tmp = strchr( title, '&' );
                    if (tmp) *tmp = '\0';
               }
          }    
               
          add_media( id++, buf, artist, title, 0, &data->playlist );
     }
}

static void
pls_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, 
                    DirectStream                            *stream )
{
     char buf[4096];
     
     while (fetch_line( stream, buf, sizeof(buf) ) == DFB_OK) {
          if (!strncmp( buf, "File", 4 )) {
               int   id = 1;
               char *url;
               
               sscanf( buf, "File%d", &id );
               id--;
               
               url = strchr( buf, '=' );
               if (!url || !*(++url))
                    continue;
               
               add_media( id, url, NULL, NULL, 0, &data->playlist );
          }
          else if (!strncmp( buf, "Title", 5 )) {                    
               if (data->playlist && data->playlist->link.prev) {
                    PlaylistEntry *entry = (PlaylistEntry*)data->playlist->link.prev;
                    int            id    = 1;
                    
                    sscanf( buf, "Title%d", &id );
                    id--;
                    
                    if (entry->id == id) {
                         char *title = strchr( buf, '=' );
                         if (title && *(++title))
                              entry->title = D_STRDUP( title );
                    }
               }
          }
     }
}

static char*
tag_property( char *tag, const char *prop, const int prop_len )
{
     char  buf[prop_len+2];
     char *p, *e = NULL;
     
     memcpy( buf, prop, prop_len );
     buf[prop_len+0] = '=';
     buf[prop_len+1] = '\0';
     
     while ((p = strstr( tag, buf ))) {
          p += prop_len+1;
          if (*p == '"') {
               e = strchr( ++p, '"' );
          } else {
               for (e = p; *e && !isspace(*e); e++);
          }
          if (e > p)
               return strndup( p, e-p );
          tag = p;
     }
     
     return NULL;
}
          
static void
smil_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, 
                     DirectStream                            *stream )
{
     char         buf[4096];
     unsigned int id = 0;
     
     while (fetch_tag( stream, buf, sizeof(buf) ) == DFB_OK) {
          if (!strncmp( buf, "audio ", 6 )) {
               char *tag, *src;
               
               tag = buf + 6;
               src = tag_property( tag, "src", 3 );
               if (src) {
                    char   *artist, *title, *begin;
                    double  start = 0;
                    
                    artist = tag_property( tag, "author", 6 );
                    title  = tag_property( tag, "title", 5 );
                    begin  = tag_property( tag, "clipBegin" , 9  ) ? :
                             tag_property( tag, "clip-begin", 10 );
                    if (begin) {
                         char *tmp = begin;
                         float h, m, s;

                         if (!strncmp( tmp, "npt=", 4 ))
                              tmp += 4;
                         else if (!strncmp( tmp, "smpte=", 6 ))
                              tmp += 6;
                         
                         if (sscanf( tmp, "%f:%f:%f", &h, &m, &s ) == 3)
                              start = h * 3600.0 + m * 60.0 + s;
                         else
                              start = strtod( tmp, NULL );
                    }
                       
                    add_media( id++, src, artist, title, start, &data->playlist );
                    
                    if (artist)
                         free( artist );
                    if (title)
                         free( title );
                    if (begin)
                         free( begin );
                    free( src );
               }
          }
     }
}                        

/*****************************************************************************/  

static void
IFusionSoundMusicProvider_Playlist_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Playlist_data *data = thiz->priv;
     PlaylistEntry                           *entry, *tmp;
     
     thiz->Stop( thiz );
     
     direct_list_foreach_safe (entry, tmp, data->playlist)
          remove_media( entry, &data->playlist );    
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}
   
static DFBResult
IFusionSoundMusicProvider_Playlist_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     data->ref++;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (--data->ref == 0)
          IFusionSoundMusicProvider_Playlist_Destruct( thiz );
          
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                    FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
      
     if (data->selected->provider)
          return data->selected->provider->GetCapabilities( data->selected->provider, caps );      
        
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_EnumTracks( IFusionSoundMusicProvider *thiz,
                                               FSTrackCallback            callback,
                                               void                      *callbackdata )
{
     PlaylistEntry *entry, *tmp;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!callback)
          return DFB_INVARG;
     
     direct_list_foreach_safe (entry, tmp, data->playlist) {
          FSTrackDescription desc;
          
          memset( &desc, 0, sizeof(FSTrackDescription) );
          
          if (entry->provider)
               entry->provider->GetTrackDescription( entry->provider, &desc );
          
          if (entry->title) {
               snprintf( desc.title, sizeof(desc.title), "%s", entry->title );
               desc.artist[0] = '\0';
          }
          if (entry->artist) {
               snprintf( desc.artist, sizeof(desc.artist), "%s", entry->artist );
          }
                 
          if (callback( entry->id, desc, callbackdata ))
               return DFB_INTERRUPTED;
     }          
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetTrackID( IFusionSoundMusicProvider *thiz,
                                               FSTrackID                 *track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!track_id)
          return DFB_INVARG;
     
     *track_id = data->selected->id;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                        FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!desc)
          return DFB_INVARG;
     
     memset( desc, 0, sizeof(FSTrackDescription) );
     
     if (data->selected->provider) 
          data->selected->provider->GetTrackDescription( data->selected->provider, desc );
     
     if (data->selected->title) {
          snprintf( desc->title, sizeof(desc->title), "%s", data->selected->title );
          desc->artist[0] = '\0';
     }
     if (data->selected->artist) {
          snprintf( desc->artist, sizeof(desc->artist), "%s", data->selected->artist );
     }
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                         FSStreamDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetStreamDescription( data->selected->provider, desc );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                         FSBufferDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetBufferDescription( data->selected->provider, desc );
         
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_SelectTrack( IFusionSoundMusicProvider *thiz,
                                                FSTrackID                  track_id )
{
     PlaylistEntry *entry;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     direct_list_foreach (entry, data->playlist) {
          IFusionSoundMusicProvider *provider;
          
          if (entry->id != track_id)
               continue;
          
          if (data->selected) {     
               provider = data->selected->provider;
               if (provider)
                    provider->Stop( provider );
          } 
          data->selected = entry;
          
          if (!entry->provider) {
               DFBResult ret;
               ret = IFusionSoundMusicProvider_Create( entry->url, &entry->provider );
               if (ret)
                    return ret;
               
               if (entry->start)
                    entry->provider->SeekTo( entry->provider, entry->start );
          }
          
          provider = entry->provider;
          provider->SetPlaybackFlags( provider, data->playback_flags );
          if (data->stream)
               provider->PlayToStream( provider, data->stream );
          if (data->buffer)
               provider->PlayToBuffer( provider, data->buffer, 
                                       data->callback, data->callback_ctx );
          
          return DFB_OK;
     }

     return DFB_ITEMNOTFOUND;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_PlayToStream( IFusionSoundMusicProvider *thiz,
                                                 IFusionSoundStream        *destination )
{
     DFBResult ret = DFB_UNSUPPORTED;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }
     
     if (data->selected->provider) {
          ret = data->selected->provider->PlayToStream( data->selected->provider, destination );
          if (ret == DFB_OK) {
               destination->AddRef( destination );
               data->stream = destination;
          }
     }
     
     return ret;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                                 IFusionSoundBuffer        *destination,
                                                 FMBufferCallback           callback,
                                                 void                      *ctx )
{
     DFBResult ret = DFB_UNSUPPORTED;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }
     
     if (data->selected->provider) {
          ret = data->selected->provider->PlayToBuffer( data->selected->provider,
                                                        destination, callback, ctx );
          if (ret == DFB_OK) {
               destination->AddRef( destination );
               data->buffer = destination;
               data->callback = callback;
               data->callback_ctx = ctx;
          }
     }
     
     return ret;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }
     
     if (data->selected->provider)
          return data->selected->provider->Stop( data->selected->provider );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetStatus( IFusionSoundMusicProvider *thiz,
                                              FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetStatus( data->selected->provider, status );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_SeekTo( IFusionSoundMusicProvider *thiz,
                                           double                     seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->SeekTo( data->selected->provider, seconds );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetPos( IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetPos( data->selected->provider, seconds );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_GetLength( IFusionSoundMusicProvider *thiz,
                                              double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetLength( data->selected->provider, seconds );
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_Playlist_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                     FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     data->playback_flags = flags;
     
     if (data->selected->provider)
          return data->selected->provider->SetPlaybackFlags( data->selected->provider, flags );
          
     return DFB_UNSUPPORTED;
}  
      
/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (get_playlist_type( ctx->mimetype, ctx->filename, ctx->header ))
          return DFB_OK;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, 
           const char                *filename, 
           DirectStream              *stream )
{
     const char *mimetype = direct_stream_mime( stream );
     char        header[64];
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Playlist )
     
     data->ref = 1;
     
     memset( header, 0, sizeof(header) );
     direct_stream_peek( stream, sizeof(header), 0, header, NULL ); 
     
     switch (get_playlist_type( mimetype, filename, header )) {
          case PLT_M3U:
               m3u_playlist_parse( data, stream );
               break;
          case PLT_RAM:
               ram_playlist_parse( data, stream );
               break;
          case PLT_PLS:
               pls_playlist_parse( data, stream );
               break;
          case PLT_SMIL:
               smil_playlist_parse( data, stream );
               break;
          default:
               break;
     }
     
     if (!data->playlist) {
          IFusionSoundMusicProvider_Playlist_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Playlist_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Playlist_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Playlist_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_Playlist_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_Playlist_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Playlist_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Playlist_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Playlist_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_Playlist_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Playlist_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Playlist_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Playlist_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_Playlist_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_Playlist_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Playlist_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Playlist_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_Playlist_SetPlaybackFlags;
     
     /* select first media */
     thiz->SelectTrack( thiz, data->playlist->id );

     return DFB_OK;
}

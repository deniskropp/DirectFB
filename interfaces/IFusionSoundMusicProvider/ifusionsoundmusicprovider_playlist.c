/*
 * Copyright (C) 2006-2008 Claudio Ciccani <klan@users.sf.net>
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
#include <direct/memcpy.h>
#include <direct/stream.h>
#include <direct/util.h>


static DirectResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DirectResult
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
     PLT_SMIL,
     PLT_ASX,
     PLT_XSPF
} PlaylistType;

typedef struct {
     DirectLink                 link;
     
     FSTrackID                  id;
     
     char                      *url;
     char                      *artist;
     char                      *title;
     char                      *album;
     
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

static DirectResult
add_media( FSTrackID       id, 
           const char     *url, 
           const char     *artist, 
           const char     *title,
           const char     *album,
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
     if (album && *album)
          entry->album = D_STRDUP( album );
     entry->start = start;
     
     direct_list_append( (DirectLink**)playlist, &entry->link );
     
     return DR_OK;
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
     if (entry->album)
          D_FREE( entry->album );
     if (entry->provider)
          entry->provider->Release( entry->provider );
          
     D_FREE( entry );
}
 
/*****************************************************************************/

static char* trim( char *s ) {
     char *e;
  
     while (*s && isspace(*s))
          s++;
 
     e = s + strlen(s) - 1;
     while (e > s && isspace(*e))
          *e-- = '\0';
    
     return s;
}

static char* stristr( const char *a, const char *b )
{
     int len = strlen(b) - 1;
     
     while (*a) {
          if (tolower(*a) == tolower(*b) && !strncasecmp( a+1, b+1, len ))
               return (char*)a;
          a++;
     }
     
     return NULL;
}

static int parse_time( const char *s ) {
     int t = 0;
     int i;
  
     if (!s)
          return 0;

     if (!strncmp (s, "npt=", 4))
          s += 4;
     else if (!strncmp (s, "smpte=", 6))
          s += 6;
  
     for (i = 0; i < 3; i++) {
          t *= 60;
          t += atoi(s);
          s = strchr( s, ':' );
          if (!s)
               break;
          s++;
     }

     return t;
}

static void replace_xml_entities( char *s )
{
     char *d = s;
     
     while (*s) {
          if (*s == '&') {
               if (!strncmp( s+1, "amp;", 4 )) {
                    *d++ = '&';
                    s += 5;
                    continue;
               }
               else if (!strncmp( s+1, "apos;", 5 )) {
                    *d++ = '\'';
                    s += 6;
                    continue;
               }
               else if (!strncmp( s+1, "gt;", 3 )) {
                    *d++ = '>';
                    s += 4;
                    continue;
               }
               else if (!strncmp( s+1, "lt;", 3 )) {
                    *d++ = '<';
                    s += 4;
                    continue;
               }
               else if (!strncmp( s+1, "quot;", 5 )) {
                    *d++ = '"';
                    s += 6;
                    continue;
               }
          }
          *d++ = *s++;
     }
     
     *d = '\0';
}               

/*****************************************************************************/

static PlaylistType 
get_playlist_type( const char *mimetype, const char *filename,
                   const char *header, int header_size )
{
     if (mimetype) {
          if (!strcmp( mimetype, "audio/mpegurl" ) ||
              !strcmp( mimetype, "audio/x-mpegurl" ))
               return PLT_M3U;
          if (!strcmp( mimetype, "audio/x-scpls" ))
               return PLT_PLS;
          if (!strcmp( mimetype, "audio/x-ms-wax" ))
               return PLT_ASX;
          if (!strcmp( mimetype, "application/smil" ))
               return PLT_SMIL;
          if (!strcmp( mimetype, "application/xspf+xml" ))
               return PLT_XSPF;
     }
     
     if (filename) {
          char *ext = strrchr( filename, '.' );
          if (ext) {
               if (!strcasecmp( ext, ".m3u" ))
                    return PLT_M3U;           
               if (!strcasecmp( ext, ".ram" ))
                    return PLT_RAM;              
               if (!strcasecmp( ext, ".pls" ))
                    return PLT_PLS;                 
               if (!strcasecmp( ext, ".wax" ))
                    return PLT_ASX;                  
               if (!strcasecmp( ext, ".smi" ) ||
                   !strcasecmp( ext, ".smil" ))
                    return PLT_SMIL;       
               if (!strcasecmp( ext, ".xspf" ))
                    return PLT_XSPF;
          }
     }
     
     if (header) {
          char *end = (char*)header + header_size;
          char *tmp = (char*)header;
          
          while (tmp < end && *tmp && isspace(*tmp))
               tmp++;
          
          if (!strncmp( tmp, "#EXTM3U", 7 ))
               return PLT_M3U;     
          if (!strncmp( tmp, "rtsp://", 7 ) ||
              !strncmp( tmp, "http://", 7 ) ||
              !strncmp( tmp, "file://", 7 ))
               return PLT_RAM;     
          if (!strncmp( tmp, "[Playlist]", 10 ))
               return PLT_PLS;      
          if (!strncasecmp( tmp, "<ASX", 4 ))
               return PLT_ASX;       
          if (!strncmp( tmp, "<smil", 5 ))
               return PLT_SMIL;      
          if (!strncmp( tmp, "<playlist", 9 ))
               return PLT_XSPF;
          if (!strncmp( tmp, "<?xml", 5 )) {
               tmp += 5;
               while (tmp < end && (tmp = strchr( tmp, '<' ))) {
                    if (!strncasecmp( header, "<ASX", 4 ))
                         return PLT_ASX;       
                    if (!strncmp( header, "<smil", 5 ))
                         return PLT_SMIL;      
                    if (!strncmp( header, "<playlist", 9 ))
                         return PLT_XSPF;
                    tmp++;
               }
          }                     
     } 
     
     return PLT_NONE;
}
               
static void
m3u_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     char *title = NULL;
     int   id    = 0;
     
     while (src) {
          end = strchr( src, '\n' );
          if (end)
               *end = '\0';
               
          src = trim(src);
          if (*src == '#') {
               if (!strncmp( src+1, "EXTINF:", 7 )) {
                    title = strchr( src+8, ',' );
                    if (title)
                         title++;
               }
          }
          else if (*src) {
               add_media( id++, src, NULL, title, NULL, 0, &data->playlist );
               title = NULL;
          }
          
          src = end;
          if (src)
               src++;
     }
}

static void
ram_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     int   id = 0;
     
     while (src) {
          end = strchr( src, '\n' );
          if (end)
               *end = '\0';
               
          src = trim(src);
          if (!strcmp( src, "--stop--" ))
               break;
          if (*src && *src != '#') {
               char *tmp, *artist = NULL, *title = NULL;
               
               tmp = strchr( src, '?' );
               if (tmp) {
                    artist = strstr( tmp+1, "artist=" );
                    title  = strstr( tmp+1, "title=" );
                    
                    if (artist) {
                         artist += 7;
                         tmp = strchr( artist, '&' );
                         if (tmp)
                              *tmp = '\0';
                    }
                    if (title) {
                         title += 6;
                         tmp = strchr( title, '&' );
                         if (tmp)
                              *tmp = '\0';
                    }
               }
               
               add_media( id++, src, artist, title, NULL, 0, &data->playlist );
          }
          
          src = end;
          if (src)
               src++;
     }
}

static void
pls_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     int   id;
     
     while (src) {
          end = strchr( src, '\n' );
          if (end)
               *end = '\0';
               
          src = trim(src);
          if (!strncmp( src, "File", 4 )) {
               src += 4;
               id = atoi( src );
               src = strchr( src, '=' );
               if (id && src && *(src+1))
                    add_media( id-1, src+1, NULL, NULL, NULL, 0, &data->playlist );
          }
          else if (!strncmp( src, "Title", 5 )) {
               src += 5;
               id = atoi( src );
               src = strchr( src, '=' );
               if (id && src && *(src+1)) {
                    PlaylistEntry *entry;
                    
                    direct_list_foreach (entry, data->playlist) {
                         if (entry->id == id-1) {
                              if (entry->title)
                                   D_FREE( entry->title );
                              entry->title = D_STRDUP( src+1 );
                              break;
                         }
                    }
               }
          }
          
          src = end;
          if (src)
               src++;
     }
}
   
static void
smil_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     int   id = 0;
     
     while ((src = strchr( src, '<' ))) {
          if (!strncmp( src, "<!--", 4 )) {
               src += 4;
               end = strstr( src, "-->" );
               if (!end)
                    break;
               src = end + 3;
               continue;
          }
          
          end = strchr( src, '>' );
          if (!end)
               break;
          *end = '\0';
               
          if (!strncmp( src, "<audio", 6 )) {
               char *tmp, *url, *author, *title, *start;
               
               src += 6;
               url = strstr( src, "src=\"" );
               author = strstr( src, "author=\"" );
               title = strstr( src, "title=\"" );
               start = strstr( src, "clipBegin=\"" ) ? : strstr( src, "clip-begin=\"" );
               if (url) {
                    url += 5;
                    tmp = strchr( url, '"' );
                    if (tmp)
                         *tmp = '\0';
                    else
                         url = NULL;
               }
               if (author) {
                    author += 8;
                    tmp = strchr( author, '"' );
                    if (tmp) {
                         *tmp = '\0';
                         replace_xml_entities( author );
                    }
                    else {
                         author = NULL;
                    }
               }
               if (title) {
                    title += 7;
                    tmp = strchr( title, '"' );
                    if (tmp) {
                         *tmp = '\0';
                         replace_xml_entities( title );
                    }
                    else {
                         title = NULL;
                    }
               }
               if (start) {
                    start = strchr( start, '=' ) + 2;
                    tmp = strchr( start, '"' );
                    if (tmp)
                         *tmp = '\0';
                    else
                         start = NULL;
               }
  
               if (url)
                    add_media( id++, url, author, title, NULL, parse_time(start), &data->playlist );
          }
          
          src = end + 1;
     }
}

static void
asx_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     char *url    = NULL;
     char *title  = NULL;
     char *author = NULL;
     char *start  = NULL;
     int   id     = 0;
     
     while ((src = strchr( src, '<' ))) {
          if (!strncmp( src, "<!--", 4 )) {
               src += 4;
               end = strstr( src, "-->" );
               if (!end)
                    break;
               src = end + 3;
          }
          else if (!strncasecmp( src, "<entry>", 7 )) {
               src += 7;
               url = title = author = start = NULL;
          }
          else if (!strncasecmp( src, "<title>", 7 )) {
               src += 7;
               end = stristr( src, "</title>" );
               if (end) {
                    *end = '\0';
                    title = trim(src);
                    src = end + 8;
               }
          }
          else if (!strncasecmp( src, "<author>", 8 )) {
               src += 8;
               end = stristr( src, "</author>" );
               if (end) {
                    *end = '\0';
                    author = trim(src);
                    src = end + 9;
               }
          }        
          else if (!strncasecmp( src, "<ref", 4 )) {
               src += 4;
               url = stristr( src, "href=" );
               if (url) {
                    url += 5;
                    if (*url == '"') {
                         end = strchr( ++url, '"' );
                    }
                    else {
                         for (end = url; *end && !isspace(*end); end++);
                    }
                    if (end && *end) {
                         *end = '\0';
                         src = end + 1;
                    }
                    else {
                         url = NULL;
                    }
               }
          }
          else if (!strncasecmp( src, "<starttime", 10 )) {
               src += 10;
               start = stristr( src, "value=" );
               if (start) {
                    start += 6;
                    if (*start == '"') {
                         end = strchr( ++start, '"' );
                    }
                    else {
                         for (end = start; *end && !isspace(*end); end++);
                    }
                    if (end && *end) {
                         *end = '\0';
                         src = end + 1;
                    }
                    else {
                         start = NULL;
                    }
               }
          }
          else if (!strncasecmp( src, "</entry>", 8 )) {
               src += 8;
               if (url) {
                    add_media( id++, url, author, title, NULL, parse_time(start), &data->playlist );
                    url = title = author = start = NULL;
               }
          }
          else {
               src++;
          }
     }
} 

static void
xspf_playlist_parse( IFusionSoundMusicProvider_Playlist_data *data, char *src )
{
     char *end;
     char *url     = NULL;
     char *creator = NULL;
     char *title   = NULL;
     char *album   = NULL;
     int   id      = 0;
     
     while ((src = strchr( src, '<' ))) {
          if (!strncmp( src, "<!--", 4 )) {
               src += 4;
               end = strstr( src, "-->" );
               if (!end)
                    break;
               src = end + 3;
          }
          else if (!strncmp( src, "<track>", 7 )) {
               src += 7;
               url = creator = title = album = NULL;
          }
          else if (!strncmp( src, "<location>", 10 )) {
               src += 10;
               end = strstr( src, "</location>" );
               if (end > src) {
                    *end = '\0';
                    url = trim(src);
                    src = end + 11;
               }
          }
          else if (!strncmp( src, "<creator>", 9 )) {
               src += 9;
               end = strstr( src, "</creator>" );
               if (end > src) {
                    *end = '\0';
                    creator = trim(src);
                    src = end + 10;
               }
          }
          else if (!strncmp( src, "<title>", 7 )) {
               src += 7;
               end = strstr( src, "</title>" );
               if (end > src) {
                    *end = '\0';
                    title = trim(src);
                    src = end + 8;
               }
          }
          else if (!strncmp( src, "<album>", 7 )) {
               src += 7;
               end = strstr( src, "</album>" );
               if (end > src) {
                    *end = '\0';
                    album = trim(src);
                    src = end + 8;
               }
          }
          else if (!strncmp( src, "</track>", 8 )) {
               src += 8;
               if (url) {
                    if (creator)
                         replace_xml_entities( creator );
                    if (title)
                         replace_xml_entities( title );
                    if (album)
                         replace_xml_entities( album );
                    
                    add_media( id++, url, creator, title, album, 0, &data->playlist );
                    
                    url = creator = title = album = NULL;
               }
          }
          else {
               src++;
          }
     }
}      

/*****************************************************************************/  

static void
IFusionSoundMusicProvider_Playlist_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Playlist_data *data = thiz->priv;
     PlaylistEntry                           *entry, *tmp;
     
     direct_list_foreach_safe (entry, tmp, data->playlist)
          remove_media( entry, &data->playlist );
          
     if (data->stream)
          data->stream->Release( data->stream );

     if (data->buffer)
          data->buffer->Release( data->buffer );
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}
   
static DirectResult
IFusionSoundMusicProvider_Playlist_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     data->ref++;
     
     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (--data->ref == 0)
          IFusionSoundMusicProvider_Playlist_Destruct( thiz );
          
     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                    FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
      
     if (data->selected->provider)
          return data->selected->provider->GetCapabilities( data->selected->provider, caps );      
        
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_EnumTracks( IFusionSoundMusicProvider *thiz,
                                               FSTrackCallback            callback,
                                               void                      *callbackdata )
{
     PlaylistEntry *entry, *tmp;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!callback)
          return DR_INVARG;
     
     direct_list_foreach_safe (entry, tmp, data->playlist) {
          FSTrackDescription desc;
          
          memset( &desc, 0, sizeof(FSTrackDescription) );
          
          if (entry->provider)
               entry->provider->GetTrackDescription( entry->provider, &desc );
          
          if (entry->title) {
               direct_snputs( desc.title, entry->title, sizeof(desc.title) );
               desc.artist[0] = '\0';
          }
          if (entry->artist) {
               direct_snputs( desc.artist, entry->artist, sizeof(desc.artist) );
          }
          if (entry->album) {
               direct_snputs( desc.album, entry->album, sizeof(desc.album) );
          }
                 
          if (callback( entry->id, desc, callbackdata ))
               return DR_INTERRUPTED;
     }          
     
     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetTrackID( IFusionSoundMusicProvider *thiz,
                                               FSTrackID                 *track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!track_id)
          return DR_INVARG;
     
     *track_id = data->selected->id;
     
     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                        FSTrackDescription        *desc )
{
     PlaylistEntry *entry;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (!desc)
          return DR_INVARG;
          
     entry = data->selected;
     
     memset( desc, 0, sizeof(FSTrackDescription) );
     
     if (entry->provider) 
          entry->provider->GetTrackDescription( entry->provider, desc );
     
     if (entry->title) {
          direct_snputs( desc->title, entry->title, sizeof(desc->title) );
          desc->artist[0] = '\0';
     }
     if (entry->artist) {
          direct_snputs( desc->artist, entry->artist, sizeof(desc->artist) );
     }
     if (entry->album) {
          direct_snputs( desc->album, entry->album, sizeof(desc->album) );
     }
     
     return DR_OK;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                         FSStreamDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetStreamDescription( data->selected->provider, desc );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                         FSBufferDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetBufferDescription( data->selected->provider, desc );
         
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_SelectTrack( IFusionSoundMusicProvider *thiz,
                                                FSTrackID                  track_id )
{
     PlaylistEntry *entry;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     direct_list_foreach (entry, data->playlist) {
          IFusionSoundMusicProvider *provider;
          DirectResult               ret;
          
          if (entry->id != track_id)
               continue;
          
          if (data->selected) {
               if (data->selected->provider) {
                    data->selected->provider->Stop( data->selected->provider );
                    data->selected->provider->GetPos( data->selected->provider,
                                                      &data->selected->start );
                    data->selected->provider->Release( data->selected->provider );
                    data->selected->provider = NULL;
               }
          }
          data->selected = entry;
          
          ret = IFusionSoundMusicProvider_Create( entry->url, &provider );
          if (ret)
               return ret;
               
          if (entry->start)
               provider->SeekTo( provider, entry->start );
          
          provider->SetPlaybackFlags( provider, data->playback_flags );
          if (data->stream)
               provider->PlayToStream( provider, data->stream );
          if (data->buffer)
               provider->PlayToBuffer( provider, data->buffer, 
                                       data->callback, data->callback_ctx );
                                       
          entry->provider = provider;
          
          return DR_OK;
     }

     return DR_ITEMNOTFOUND;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_PlayToStream( IFusionSoundMusicProvider *thiz,
                                                 IFusionSoundStream        *destination )
{
     DirectResult ret = DR_UNSUPPORTED;
     
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
          if (ret == DR_OK) {
               destination->AddRef( destination );
               data->stream = destination;
          }
     }
     
     return ret;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                                 IFusionSoundBuffer        *destination,
                                                 FMBufferCallback           callback,
                                                 void                      *ctx )
{
     DirectResult ret = DR_UNSUPPORTED;
     
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
          if (ret == DR_OK) {
               destination->AddRef( destination );
               data->buffer = destination;
               data->callback = callback;
               data->callback_ctx = ctx;
          }
     }
     
     return ret;
}

static DirectResult
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
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetStatus( IFusionSoundMusicProvider *thiz,
                                              FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetStatus( data->selected->provider, status );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_SeekTo( IFusionSoundMusicProvider *thiz,
                                           double                     seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->SeekTo( data->selected->provider, seconds );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetPos( IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetPos( data->selected->provider, seconds );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_GetLength( IFusionSoundMusicProvider *thiz,
                                              double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->GetLength( data->selected->provider, seconds );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                     FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     data->playback_flags = flags;
     
     if (data->selected->provider)
          return data->selected->provider->SetPlaybackFlags( data->selected->provider, flags );
          
     return DR_UNSUPPORTED;
}

static DirectResult
IFusionSoundMusicProvider_Playlist_WaitStatus( IFusionSoundMusicProvider *thiz,
                                               FSMusicProviderStatus      mask,
                                               unsigned int               timeout )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Playlist )
     
     if (data->selected->provider)
          return data->selected->provider->WaitStatus( data->selected->provider, mask, timeout );
          
     return DR_UNSUPPORTED;
}
      
/* exported symbols */

static DirectResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (get_playlist_type( ctx->mimetype, ctx->filename,
                            (const char*)ctx->header, sizeof(ctx->header) ))
          return DR_OK;
          
     return DR_UNSUPPORTED;
}

static DirectResult
Construct( IFusionSoundMusicProvider *thiz, 
           const char                *filename, 
           DirectStream              *stream )
{
     char         *src  = NULL;
     unsigned int  size = 0;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Playlist )
     
     data->ref = 1;
     
     size = direct_stream_length( stream );
     if (size) {
          int pos = 0;
               
          src = D_MALLOC( size+1 );
          if (!src) {
               DIRECT_DEALLOCATE_INTERFACE( thiz );
               return D_OOM();
          }
               
          while (pos < size) {
               unsigned int len = 0;
               direct_stream_wait( stream, size, NULL );
               if (direct_stream_read( stream, size, src+pos, &len )) {
                    D_FREE( src );
                    DIRECT_DEALLOCATE_INTERFACE( thiz );
                    return DR_IO;
               }
               pos += len;
          }
     }
     else {
          char buf[1024];
               
          while (1) {
               unsigned int len = 0;
               direct_stream_wait( stream, sizeof(buf), NULL );
               if (direct_stream_read( stream, sizeof(buf), buf, &len ))
                   break;
               src = D_REALLOC( src, size+len+1 );
               if (src) {
                   direct_memcpy( src+size, buf, len );
                   size += len;
               }
          }
               
          if (!src) {
               DIRECT_DEALLOCATE_INTERFACE( thiz );
               return DR_IO;
          }
     }
     src[size] = 0;

     switch (get_playlist_type( direct_stream_mime(stream), filename, src, size )) {
          case PLT_M3U:
               m3u_playlist_parse( data, src );
               break;
          case PLT_RAM:
               ram_playlist_parse( data, src );
               break;
          case PLT_PLS:
               pls_playlist_parse( data, src );
               break;
          case PLT_SMIL:
               smil_playlist_parse( data, src );
               break;
          case PLT_ASX:
               asx_playlist_parse( data, src );
               break;
          case PLT_XSPF:
               xspf_playlist_parse( data, src );
               break;
          default:
               D_BUG( "unexpected playlist format" );
               break;
     }
     
     D_FREE( src );

     if (!data->playlist) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DR_FAILURE;
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
     thiz->WaitStatus           = IFusionSoundMusicProvider_Playlist_WaitStatus;
     
     /* select first media */
     thiz->SelectTrack( thiz, data->playlist->id );

     return DR_OK;
}

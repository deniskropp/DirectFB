/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2007  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@fusionsound.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@fusionsound.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <fusionsound.h>

#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>


typedef struct {
     DirectLink   link;
     
     FSTrackID    id;
} MediaTrack;

typedef struct {
     DirectLink   link;

     const char  *url;

     int          id;

     MediaTrack  *tracks;
} Media;


static IFusionSound              *sound    = NULL;
static IFusionSoundStream        *stream   = NULL;
static IFusionSoundPlayback      *playback = NULL;
static Media                     *playlist = NULL;
static struct termios             term;

static int                        quit     = 0;
static int                        quiet    = 0;
static float                      volume   = 1.0;
static float                      pitch    = 1.0;
static int                        flags    = FMPLAY_NOFX;
static int                        repeat   = 0;
static FSSampleFormat             format   = FSSF_UNKNOWN;
static int                        gain     = 0;
#define RPGAIN_TRACK 1
#define RPGAIN_ALBUM 2

static void
usage( const char *progname )
{
     fprintf( stderr, "fsplay v%s - FusionSound Player\n"
                      "\n"
                      "Usage: %s [options] <file1 file2 ...>\n"
                      "\n"
                      "Options:\n"
                      "  -h, --help      Show this help\n"
                      "  -v, --version   Print version and quit\n"
                      "  -q, --quiet     Suppress messages\n"
                      "  -r, --repeat    Repeat entire playlist\n"
                      "  -d, --depth <n> Force output bit depth (8, 16, 24, or 32)\n"
                      "  -g, --gain <n>  Select replay gain (0:none, 1:track, 2:album)\n"
                      "\n"
                      "Playback Control:\n"
                      "  [p]   start playback\n"
                      "  [s]   stop playback\n"
                      "  [f]   seek forward (+15s)\n"
                      "  [b]   seek backward (-15s)\n"
                      "  [0-9] seek to absolute position (%%)\n" 
                      "  [>]   switch to next track\n"
                      "  [<]   switch to previous track\n"
                      "  [l]   toggle track looping\n"
                      "  [r]   toggle playlist repeating\n"
                      "  [-]   decrease volume level\n"
                      "  [+]   increase volume level\n"
                      "  [/]   decrease playback speed\n"
                      "  [*]   increase playback speed\n"
                      "  [q]   quit\n"
                      "\n", FUSIONSOUND_VERSION, progname );
     exit( 1 );
}

static void
parse_options( int argc, char **argv )
{
     int id = 0;
     int i;
     
     for (i = 1; i < argc; i++) {
          char *opt = argv[i];
          
          if (!strcmp( opt, "-h" ) || !strcmp( opt, "--help" )) {
               usage( argv[0] );
          }
          else if (!strcmp( opt, "-v" ) || !strcmp( opt, "--version" )) {
               puts( FUSIONSOUND_VERSION );
               exit( 0 );
          }
          else if (!strcmp( opt, "-q" ) || !strcmp( opt, "--quiet" )) {
               quiet = 1;
          }
          else if (!strcmp( opt, "-r" ) || !strcmp( opt, "--repeat" )) {
               repeat = 1;
          }
          else if (!strcmp( opt, "-d" ) || !strcmp( opt, "--depth" )) {
               if (++i < argc) {
                    switch (atoi( argv[i] )) {
                         case 8:
                              format = FSSF_U8;
                              break;
                         case 16:
                              format = FSSF_S16;
                              break;
                         case 24:
                              format = FSSF_S24;
                              break;
                         case 32:
                              format = FSSF_S32;
                              break;
                         default:
                              fprintf( stderr, "Unsupported bit depth!\n" );
                              usage( argv[0] );
                    }
               }
               else {
                    fprintf( stderr, "No bit depth specified!\n" );
                    usage( argv[0] );
               }
          }
          else if (!strcmp( opt, "-g" ) || !strcmp( opt, "--gain" )) {
               if (++i < argc) {
                    gain = atoi( argv[i] );
               }
               else {
                    fprintf( stderr, "No gain specified!\n" );
                    usage( argv[0] );
               }
          }               
          else {
               Media *media;
               media = D_MALLOC( sizeof(Media) );
               if (!media)
                    exit( D_OOM() );
               media->url = opt;
               media->id = id++;
               media->tracks = NULL;
               direct_list_append( (DirectLink**)&playlist, &media->link );
          }
     }
     
     if (!playlist)
          usage( argv[0] );
}                   

static DFBEnumerationResult
track_add_callback( FSTrackID id, FSTrackDescription desc, void *ctx )
{
     Media      *media = ctx;
     MediaTrack *track;

     if (!quiet) {
          fprintf( stderr, "  Track %2d: %s - %s\n", id,
                   *desc.artist ? desc.artist : "Unknown",
                   *desc.title  ? desc.title  : "Unknown" );
     }

     track = D_MALLOC( sizeof(MediaTrack) );
     if (!track) {
          D_OOM();
          return DFENUM_CANCEL;
     }
     track->id = id;
     direct_list_append( (DirectLink**)&media->tracks, &track->link );
     
     return DFENUM_OK;
}

static int
playback_run( IFusionSoundMusicProvider *provider, Media *media )
{
     DFBResult              ret;
     FSMusicProviderStatus  status = FMSTATE_UNKNOWN;
     FSStreamDescription    s_dsc;
     FSTrackDescription     t_dsc;
     MediaTrack            *track;

     for (track = media->tracks; track;) {
          MediaTrack *next = (MediaTrack*)track->link.next;
          double      len = 0, pos = 0;

          /* Select current track in playlist. */
          ret = provider->SelectTrack( provider, track->id );
          if (ret) {
               FusionSoundError( "IFusionSoundMusicProvider::SelectTrack", ret );
               track = next;
               continue;
          }

          provider->GetTrackDescription( provider, &t_dsc );
     
          provider->GetStreamDescription( provider, &s_dsc );
          if (format)
               s_dsc.sampleformat = format;
     
          if (stream) {
               FSStreamDescription dsc;
               /* Check whether stream format changed. */
               stream->GetDescription( stream, &dsc ); 
               if (dsc.samplerate   != s_dsc.samplerate ||
                   dsc.channels     != s_dsc.channels   ||
                   dsc.sampleformat != s_dsc.sampleformat)
               {
                    if (playback) {
                         playback->Release( playback ); 
                         playback = NULL;
                    }
                    if (pitch)
                         stream->Wait( stream, 0 );
                    stream->Release( stream );
                    stream = NULL;
               }
          }
          if (!stream) {
               /* Create the sound stream and feed it. */
               ret = sound->CreateStream( sound, &s_dsc, &stream );
               if (ret) {
                    FusionSoundError( "IFusionSound::CreateStream", ret );
                    break;
               }
               stream->GetDescription( stream, &s_dsc );
               stream->GetPlayback( stream, &playback );
          }

          switch (gain) {
               case RPGAIN_TRACK:
                    if (t_dsc.replaygain > 0.0)
                         volume = t_dsc.replaygain;
                    break;
               case RPGAIN_ALBUM:
                    if (t_dsc.replaygain_album > 0.0)
                         volume = t_dsc.replaygain_album;
                    break;
               default:
                    break;
          }

          /* Reset volume level. */
          playback->SetVolume( playback, volume );
     
          /* Reset pitch. */
          playback->SetPitch( playback, pitch );

          /* Query provider for track length. */
          provider->GetLength( provider, &len );
          
          /* Let the provider play the music using our stream. */
          ret = provider->PlayToStream( provider, stream );
          if (ret) {
               FusionSoundError( "IFusionSoundMusicProvider::PlayTo", ret );
               break;
          }

          /* Print some information on the track. */
          if (!quiet) {
               fprintf( stderr,
                        "\nTrack %d.%d:\n"
                        "  Artist:     %s\n"
                        "  Title:      %s\n"
                        "  Album:      %s\n"
                        "  Year:       %d\n"
                        "  Genre:      %s\n"
                        "  Encoding:   %s\n"
                        "  Bitrate:    %d Kbits/s\n" 
                        "  ReplayGain: %.2f (track), %.2f (album)\n"
                        "  Output:     %d Hz, %d channel(s), %d bits\n\n\n",
                        media->id, track->id,
                        t_dsc.artist, t_dsc.title, t_dsc.album, t_dsc.year,
                        t_dsc.genre, t_dsc.encoding, t_dsc.bitrate/1000, 
                        t_dsc.replaygain, t_dsc.replaygain_album,
                        s_dsc.samplerate, s_dsc.channels,
                        FS_BITS_PER_SAMPLE(s_dsc.sampleformat) );
               fflush( stderr );
          }
          
          do {
               /* Query playback status. */
               provider->GetStatus( provider, &status );
          
               if (!quiet) {
                    int filled = 0, total = 0;
    
                    /* Query ring buffer status. */
                    stream->GetStatus( stream, &filled, &total, NULL, NULL, NULL );
                    /* Query elapsed seconds. */
                    provider->GetPos( provider, &pos );

                    /* Print playback status. */
                    fprintf( stderr, 
                         "\rTime: %02d:%02d,%02d of %02d:%02d,%02d  Ring Buffer: %02d%% ",
                         (int)pos/60, (int)pos%60, (int)(pos*100.0)%100,
                         (int)len/60, (int)len%60, (int)(len*100.0)%100,
                         filled * 100 / total );
                    fflush( stderr );
               }

               if (isatty( STDIN_FILENO )) {
                    struct timeval t = { 0, 40000 };
                    fd_set         s;
                    int            c;

                    FD_ZERO( &s );
                    FD_SET( STDIN_FILENO, &s );

                    select( STDIN_FILENO+1, &s, NULL, NULL, &t );

                    while ((c = getc( stdin )) > 0) {
                         switch (c) {
                              case 'p':
                                   provider->PlayToStream( provider, stream );
                                   break;
                              case 's':
                                   if (!pitch) {
                                        playback->SetVolume( playback, 0 );
                                        playback->SetPitch( playback, 1 );
                                   }
                                   provider->Stop( provider );
                                   if (!pitch) { 
                                        playback->SetPitch( playback, pitch );
                                        playback->SetVolume( playback, volume );
                                   }
                                   break;
                              case 'f':
                                   provider->GetPos( provider, &pos );
                                   provider->SeekTo( provider, pos+15.0 );
                                   break;
                              case 'b':
                                   provider->GetPos( provider, &pos );
                                   provider->SeekTo( provider, pos-15.0 );
                                   break;
                              case '0' ... '9':
                                   if (len)
                                        provider->SeekTo( provider, len * (c-'0') / 10 );
                                   break;
                              case '<':
                                   if (track == media->tracks)
                                        return -1;
                                   next = (MediaTrack*)track->link.prev;
                              case '>': 
                                   status = FMSTATE_FINISHED;
                                   break;
                              case 'l':
                                   flags ^= FMPLAY_LOOPING;
                                   provider->SetPlaybackFlags( provider, flags );
                                   break;
                              case 'r':
                                   repeat = !repeat;
                                   break;
                              case '-':
                                   volume -= 0.1;
                                   if (volume < 0.0)
                                        volume = 0.0;
                                   playback->SetVolume( playback, volume );
                                   break;
                              case '+':
                                   volume += 0.1;
                                   if (volume > 64.0)
                                        volume = 64.0;
                                   playback->SetVolume( playback, volume );
                                   break;
                              case '/':
                                   pitch -= 0.1;
                                   if (pitch < 0.0)
                                        pitch = 0.0;
                                   playback->SetPitch( playback, pitch );
                                   break;
                              case '*':
                                   pitch += 0.1;
                                   if (pitch > 64.0)
                                        pitch = 64.0;
                                   playback->SetPitch( playback, pitch );
                                   break;
                              case 'q':
                              case 'Q':
                              case '\033': // Escape
                                   quit = 1;
                                   return 0;
                              default:
                                   break;
                         }
                    }
               }
               else {
                    usleep( 40000 );
               }
          } while (status != FMSTATE_FINISHED);
     
          if (!quiet)
               fprintf( stderr, "\n" );

          track = next;
     }

     return 0;
}     

int
main( int argc, char **argv )
{
     DFBResult  ret;
     Media     *media;

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundInit", ret );

     parse_options( argc, argv );
         
     /* Retrieve the main sound interface. */
     ret = FusionSoundCreate( &sound );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundCreate", ret );
     
     if (isatty( STDIN_FILENO )) {
          struct termios cur;
          /* Get terminal attributes. */
          tcgetattr( STDIN_FILENO, &term );
          /* Set terminal attributes */
          cur = term;
          cur.c_cc[VTIME] = 0;
          cur.c_cc[VMIN]  = 0;
          cur.c_lflag    &= ~(ICANON | ECHO);
          tcsetattr( STDIN_FILENO, TCSAFLUSH, &cur );
     }
     
     do {
          for (media = playlist; media && !quit;) {
               Media                     *next = (Media*)media->link.next;
               IFusionSoundMusicProvider *provider;

               /* Create a music provider for the specified file. */
               ret = sound->CreateMusicProvider( sound, media->url, &provider );
               if (ret) {
                    FusionSoundError( "IFusionSound::CreateMusicProvider", ret );
                    media = next;
                    continue;
               }
               
               /* Add contents. */
               if (!quiet)
                    fprintf( stderr, "\n%s:\n", media->url );
               provider->EnumTracks( provider, track_add_callback, media );
               if (!quiet)
                    fprintf( stderr, "\n" );
    
               /* Play tracks. */
               if (playback_run( provider, media ) < 0)
                    next = (Media*)media->link.prev;
         
               /* Release provider. */
               provider->Release( provider );

               /* Release media tracks. */
               while (media->tracks) {
                    MediaTrack *track = media->tracks;
                    media->tracks = (MediaTrack*)track->link.next;
                    D_FREE( track );
               }

               media = next;
          }
     } while (repeat && !quit);

     if (!quiet)
          fprintf( stderr, "\nQuit.\n" );
     
     if (playback)
          playback->Release( playback );
     
     if (stream)
          stream->Release( stream );
     
     sound->Release( sound );
     
     if (isatty( STDIN_FILENO ))
          tcsetattr( STDIN_FILENO, TCSADRAIN, &term );
     
     return 0;
}


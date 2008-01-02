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
#include <signal.h>
#include <termios.h>

#include <fusionsound.h>

#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>


typedef struct {
     DirectLink   link;    
     const char  *url;
} Media;


static IFusionSound              *sound    = NULL;
static IFusionSoundMusicProvider *provider = NULL;
static IFusionSoundStream        *stream   = NULL;
static IFusionSoundPlayback      *playback = NULL;
static DirectLink                *playlist = NULL;
static struct termios             term;

static int                        quit     = 0;
static int                        quiet    = 0;
static float                      volume   = 1.0;
static int                        flags    = FMPLAY_NOFX;
static int                        repeat   = 0;
static int                        gain     = 0;
#define RPGAIN_TRACK 1
#define RPGAIN_ALBUM 2


static void
usage( const char *progname )
{
     fprintf( stderr, "\nUsage: %s [options] <file1 file2 ...>\n", progname );
     fprintf( stderr, "\nOptions:\n" );
     fprintf( stderr, "  -h, --help      Show this help\n" );
     fprintf( stderr, "  -v, --version   Print version and quit\n" );
     fprintf( stderr, "  -q, --quiet     Suppress messages\n" );
     fprintf( stderr, "  -r, --repeat    Repeat entire playlist\n" );
     fprintf( stderr, "  -g, --gain <n>  Select replay gain (0:none, 1:track, 2:album)\n" );
     fprintf( stderr, "\nPlayback Control:\n" );
     fprintf( stderr, "  [p] start playback\n" );
     fprintf( stderr, "  [s] stop playback\n" );
     fprintf( stderr, "  [f] seek forward (+15s)\n" );
     fprintf( stderr, "  [b] seek backward (-15s)\n" );
     fprintf( stderr, "  [ ] switch to next track\n" );
     fprintf( stderr, "  [l] toggle track looping\n" );
     fprintf( stderr, "  [r] toggle playlist repeating\n" );
     fprintf( stderr, "  [-] decrease volume level\n" );
     fprintf( stderr, "  [+] increase volume level\n" );
     fprintf( stderr, "  [q] quit\n\n" );
     exit( 1 );
}

static void
parse_options( int argc, char **argv )
{
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
          else if (!strcmp( opt, "-g" ) || !strcmp( opt, "--gain" )) {
               if (++i < argc)
                    gain = atoi( argv[i] );
          }               
          else {
               Media *media;
               media = D_CALLOC( 1, sizeof(Media) );
               if (!media)
                    exit( D_OOM() );
               media->url = opt;
               direct_list_append( &playlist, &media->link );
          }
     }
     
     if (!playlist)
          usage( argv[0] );
}                   
 
static void
do_quit( void )
{
     if (!quiet)
          fprintf( stderr, "\nQuit.\n" );
     
     if (provider)
          provider->Release( provider );
     if (playback)
          playback->Release( playback );
     if (stream)
          stream->Release( stream );
     if (sound)
          sound->Release( sound );
     
     if (isatty( STDIN_FILENO ))
          tcsetattr( STDIN_FILENO, TCSADRAIN, &term );
}

static void
handle_sig( int s )
{
     quit = 1;
}

static DFBEnumerationResult
track_display_callback( FSTrackID id, FSTrackDescription desc, void *ctx )
{
     fprintf( stderr, "  Track %2d: %s - %s\n", id,
              *desc.artist ? desc.artist : "Unknown",
              *desc.title  ? desc.title  : "Unknown" );
     
     return DFENUM_OK;
}

static DFBEnumerationResult
track_playback_callback( FSTrackID id, FSTrackDescription desc, void *ctx )
{
     DFBResult             ret;
     FSMusicProviderStatus status = FMSTATE_UNKNOWN;
     double                len    = 0;
     FSStreamDescription   s_dsc;
          
     /* Select current track in playlist. */
     ret = provider->SelectTrack( provider, id );
     if (ret) {
          FusionSoundError( "IFusionSoundMusicProvider::SelectTrack", ret );
          return DFENUM_OK;
     }
     
     provider->GetStreamDescription( provider, &s_dsc );
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
               return DFENUM_CANCEL;
          }
          stream->GetDescription( stream, &s_dsc );
          stream->GetPlayback( stream, &playback );
     }

     switch (gain) {
          case RPGAIN_TRACK:
               if (desc.replaygain > 0.0)
                    volume = desc.replaygain;
               break;
          case RPGAIN_ALBUM:
               if (desc.replaygain_album > 0.0)
                    volume = desc.replaygain_album;
               break;
          default:
               break;
     }

     /* Reset volume level. */
     playback->SetVolume( playback, volume );
          
     /* Query provider for track length. */
     provider->GetLength( provider, &len );
          
     /* Let the provider play the music using our stream. */
     ret = provider->PlayToStream( provider, stream );
     if (ret) {
          FusionSoundError( "IFusionSoundMusicProvider::PlayTo", ret );
          return DFENUM_CANCEL;
     }
     
     /* Update track's description. */
     provider->GetTrackDescription( provider, &desc );
          
     /* Print some informations about the track. */
     if (!quiet) {
          fprintf( stderr,
              "\nTrack %d:\n"
              "  Artist:     %s\n"
              "  Title:      %s\n"
              "  Album:      %s\n"
              "  Year:       %d\n"
              "  Genre:      %s\n"
              "  Encoding:   %s\n"
              "  Bitrate:    %d Kbits/s\n" 
              "  ReplayGain: %.2f (track), %.2f (album)\n"
              "  Output:     %d Hz, %d channel(s), %d bits\n\n\n",
              id, desc.artist, desc.title, desc.album, (int)desc.year, 
              desc.genre, desc.encoding, desc.bitrate/1000, 
              desc.replaygain, desc.replaygain_album,
              s_dsc.samplerate, s_dsc.channels,
              FS_BITS_PER_SAMPLE(s_dsc.sampleformat) );
          fflush( stderr );
     }

     do {
          double pos = 0;
          
          if (!quiet) {
               int filled = 0, total = 0;
    
               /* Query ring buffer status. */
               stream->GetStatus( stream, &filled, &total, NULL, NULL, NULL );
               /* Query elapsed seconds. */
               provider->GetPos( provider, &pos );
               /* Query playback status. */
               provider->GetStatus( provider, &status );

               /* Print playback status. */
               fprintf( stderr, 
                   "\rTime: %02d:%02d,%02d of %02d:%02d,%02d  Ring Buffer: %02d%%",
                   (int)pos/60, (int)pos%60, (int)(pos*100.0)%100,
                   (int)len/60, (int)len%60, (int)(len*100.0)%100,
                   filled * 100 / total );
               fflush( stderr );
          }

          if (isatty( STDIN_FILENO )) {
               int c;

               while ((c = getc( stdin )) > 0) {
                    switch (c) {
                         case 's':
                              provider->Stop( provider );
                              break;
                         case 'p':
                              provider->PlayToStream( provider, stream );
                              break;
                         case 'f':
                              provider->GetPos( provider, &pos );
                              provider->SeekTo( provider, pos+15.0 );
                              break;
                         case 'b':
                              provider->GetPos( provider, &pos );
                              provider->SeekTo( provider, pos-15.0 );
                              break;
                         case ' ':
                              provider->Stop( provider );
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
                         case 'q':
                         case 'Q':
                         case '\033': // Escape
                              exit( 0 );
                         default:
                              break;
                    }
               }
          }
               
          usleep( 30000 );
     } while (status != FMSTATE_FINISHED && !quit);
     
     if (!quiet)
          fprintf( stderr, "\n" );
     
     return DFENUM_OK;
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

     /* Register clean-up handlers. */
     atexit( do_quit );
     signal( SIGINT, handle_sig );
     signal( SIGTERM, handle_sig );
     
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
          direct_list_foreach (media, playlist) {
               if (quit)
                    break;

               /* Create a music provider for the specified file. */
               ret = sound->CreateMusicProvider( sound, media->url, &provider );
               if (ret) {
                    FusionSoundError( "IFusionSound::CreateMusicProvider", ret );
                    continue;
               }
               
               /* Show contents. */
               if (!quiet) {
                    fprintf( stderr, "\n%s:\n", media->url );
                    provider->EnumTracks( provider, track_display_callback, NULL );
                    fprintf( stderr, "\n" );
               }
     
               /* Play tracks. */
               provider->EnumTracks( provider, track_playback_callback, NULL );
          
               provider->Release( provider );
               provider = NULL;
          }
     } while (repeat && !quit);

     return 0;
}


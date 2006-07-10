#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include <fusionsound.h>

#include <direct/mem.h>
#include <direct/list.h>


typedef struct {
     DirectLink         link;
     FSTrackID          id;
     FSTrackDescription desc;
} PlaylistEntry;


IFusionSound              *sound    = NULL;
IFusionSoundMusicProvider *provider = NULL;
IFusionSoundStream        *stream   = NULL;
PlaylistEntry             *playlist = NULL;
struct termios             term;

static void
cleanup( int s )
{
     puts ("\nQuit.");
     
     if (provider)
          provider->Release (provider);
     if (stream)
          stream->Release (stream);
     if (sound)
          sound->Release (sound);
     
     tcsetattr( fileno(stdin), TCSADRAIN, &term );
     
     exit (s);
}

static DFBEnumerationResult
track_callback( FSTrackID id, FSTrackDescription desc, void *ctx )
{
     PlaylistEntry **list  = (PlaylistEntry**) ctx;
     PlaylistEntry  *entry;
     
     entry = D_CALLOC( 1, sizeof(PlaylistEntry) );
     if (!entry)
          return DFENUM_CANCEL;
          
     entry->id   = id;
     entry->desc = desc;
     
     direct_list_append( (DirectLink**)list, (DirectLink*)entry );
     
     return DFENUM_OK;
}     

int
main (int argc, char *argv[])
{
     DFBResult            ret;
     FSStreamDescription  s_dsc;
     PlaylistEntry       *entry = NULL;

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundInit", ret );

     if (argc != 2) {
          fprintf( stderr, "\nUsage: %s <filename>\n", argv[0] );
          return -1;
     }

     /* Don't catch SIGINT. */
     DirectFBSetOption( "dont-catch", "2" );

     /* Get terminal attributes. */
     tcgetattr( fileno(stdin), &term );
     
     /* Register clean-up handler for SIGINT. */
     signal( SIGINT, cleanup );

     /* Retrieve the main sound interface. */
     ret = FusionSoundCreate( &sound );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundCreate", ret );

     /* Create a music provider for the specified file. */
     ret = sound->CreateMusicProvider( sound, argv[1], &provider );
     if (ret)
          FusionSoundErrorFatal( "IFusionSound::CreateMusicProvider", ret );
     
     /* Create the playlist. */
     ret = provider->EnumTracks( provider, track_callback, (void*)&playlist );
     if (ret) {
          FusionSoundError( "IFusionSoundMusicProvider::EnumTracks", ret );
          cleanup( 1 );
     }
     
     /* Query provider for a suitable stream description. */
     ret = provider->GetStreamDescription( provider, &s_dsc );
     if (ret) {
          FusionSoundError( "IFusionSoundMusicProvider::"
                            "GetStreamDescription", ret );
          cleanup( 1 );
     }

     /* Create the sound stream and feed it. */
     ret = sound->CreateStream( sound, &s_dsc, &stream );
     if (ret) {
          FusionSoundError( "IFusionSound::CreateStream", ret );
          cleanup (1);
     }

     /* Set terminal attributes */
     if (isatty( fileno(stdin) )) {
          struct termios cur;

          cur = term;
          cur.c_cc[VTIME] = 0;
          cur.c_cc[VMIN]  = 0;
          cur.c_lflag    &= ~(ICANON | ECHO);

          tcsetattr( fileno(stdin), TCSAFLUSH, &cur );
     }
     
     /* Iterate through playlist. */
     direct_list_foreach( entry, playlist ) {
          double len = 0;
          
          /* Select current track in playlist. */
          ret = provider->SelectTrack( provider, entry->id );
          if (ret) {
               FusionSoundError( "IFusionSoundMusicProvider::SelectTrack", ret );
               continue;
          }
          
          /* Query provider for track length. */
          provider->GetLength( provider, &len );
          
          /* Let the provider play the music using our stream. */
          ret = provider->PlayToStream( provider, stream );
          if (ret) {
               FusionSoundError( "IFusionSoundMusicProvider::PlayTo", ret );
               cleanup (1);
          }

          /* Print some informations about the track. */
          printf( "\nTrack %d:\n"
                  "  Artist:   %s\n"
                  "  Title:    %s\n"
                  "  Album:    %s\n"
                  "  Year:     %d\n"
                  "  Genre:    %s\n"
                  "  Encoding: %s\n"
                  "  Bitrate:  %d Kbits/s\n"
                  "  Output:   %d Hz, %d channel(s), %d bits\n\n\n",
                  entry->id, entry->desc.artist, entry->desc.title,
                  entry->desc.album, (int)entry->desc.year, 
                  entry->desc.genre, entry->desc.encoding,
                  entry->desc.bitrate/1000, s_dsc.samplerate, s_dsc.channels,
                  FS_BITS_PER_SAMPLE(s_dsc.sampleformat) );

          do {
               int    filled = 0;
               int    total  = 0;
               double pos    = 0;
          
               /* Query ring buffer status. */
               stream->GetStatus( stream, &filled, &total, NULL, NULL, NULL );
               /* Query elapsed seconds. */
               ret = provider->GetPos( provider, &pos );

               /* Print playback status. */
               printf( "\rTime: %02d:%02d,%02d of %02d:%02d,%02d  Ring Buffer: %02d%%",
                       (int)pos/60, (int)pos%60, (int)(pos*100.0)%100,
                       (int)len/60, (int)len%60, (int)(len*100.0)%100,
                       filled * 100 / total );
               fflush( stdout );

               if (isatty( fileno(stdin) )) {
                    int c;

                    while ((c = getc( stdin )) > 0) {
                         printf( "got: '%c'\n", c);
                         switch (c) {
                              case 's':
                                   provider->Stop( provider );
                                   break;
                              case 'p':
                                   provider->PlayToStream( provider, stream );
                                   break;
                              case '+':
                                   provider->GetPos( provider, &pos );
                                   provider->SeekTo( provider, pos+15.0 );
                                   break;
                              case '-':
                                   provider->GetPos( provider, &pos );
                                   provider->SeekTo( provider, pos-15.0 );
                                   break;
                              case ' ':
                                   ret = DFB_EOF;
                                   break;
                              case 'q':
                              case 'Q':
                              case '\033': // Escape
                                   cleanup( 0 );
                                   return 0;
                         }
                    }
               }
               
               usleep( 10000 );
          } while (ret != DFB_EOF);
     }

     puts( "\nFinished." );
     cleanup( 0 );

     return 0;
}


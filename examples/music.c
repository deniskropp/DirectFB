#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <fusionsound.h>

IFusionSound              *sound    = NULL;
IFusionSoundMusicProvider *provider = NULL;
IFusionSoundStream        *stream   = NULL;

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
     exit (s);
}

int
main (int argc, char *argv[])
{
     DFBResult                  ret;
     FSStreamDescription        s_dsc;
     FSTrackDescription         t_dsc;
     double                     len = 0.0;

     ret = FusionSoundInit (&argc, &argv);
     if (ret)
          DirectFBErrorFatal ("FusionSoundInit", ret);

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     /* Don't catch SIGINT. */
     DirectFBSetOption( "dont-catch", "2" );

     /* Register clean-up handler for SIGINT. */
     signal( SIGINT, cleanup );

     /* Retrieve the main sound interface. */
     ret = FusionSoundCreate (&sound);
     if (ret)
          DirectFBErrorFatal ("FusionSoundCreate", ret);

     /* Create a music provider for the specified file. */
     ret = sound->CreateMusicProvider (sound, argv[1], &provider);
     if (ret)
          DirectFBErrorFatal ("IFusionSound::CreateMusicProvider", ret);

     /* Query provider for a description of the current track. */
     provider->GetTrackDescription (provider, &t_dsc);

     /* Query provider for track length. */
     provider->GetLength (provider, &len);
     
     /* Query provider for a suitable stream description. */
     ret = provider->GetStreamDescription (provider, &s_dsc);
     if (ret) {
          DirectFBError ("IFusionSoundMusicProvider::"
                          "GetStreamDescription", ret);
          cleanup (1);
     }

     /* Create the sound stream and feed it. */
     ret = sound->CreateStream (sound, &s_dsc, &stream);
     if (ret) {
          DirectFBErrorFatal ("IFusionSound::CreateStream", ret);
          cleanup (1);
     }
     
     /* Let the provider play the music using our stream. */
     ret = provider->PlayToStream (provider, stream);
     if (ret) {
          DirectFBError ("IFusionSoundMusicProvider::PlayTo", ret);
          cleanup (1);
     }

     /* Print some informations about the track. */
     printf( "\nPlaying %s:\n"
             "  Artist:   %s\n"
             "  Title:    %s\n"
             "  Album:    %s\n"
             "  Year:     %d\n"
             "  Genre:    %s\n"
             "  Encoding: %s\n"
             "  Bitrate:  %d Kbits/s\n"
             "  Output:   %d Hz, %d channel(s)\n\n\n",
             basename (argv[1]),
             t_dsc.artist, t_dsc.title, t_dsc.album,
             (int)t_dsc.year, t_dsc.genre, t_dsc.encoding,
             t_dsc.bitrate/1000, s_dsc.samplerate, s_dsc.channels );

     while (1) {
          int    filled = 0;
          int    total  = 0;
          double pos    = 0;
          
          /* Query ring buffer status. */
          stream->GetStatus (stream, &filled, &total, NULL, NULL, NULL );
          /* Query elapsed seconds. */
          ret = provider->GetPos (provider, &pos);

          /* Print playback status. */
          printf( "\rTime: %02d:%02d,%02d of %02d:%02d,%02d  Ring Buffer: %02d%%",
                  (int)pos/60, (int)pos%60, (int)(pos*100.0)%100,
                  (int)len/60, (int)len%60, (int)(len*100.0)%100,
                  filled * 100 / total );
          fflush( stdout );

          /* Check if playback is finished. */
          if (ret == DFB_EOF)
               break;
               
          usleep( 100000 );
     }

     puts( "\nFinished." );
     cleanup (0);

     return 0;
}


#include <stdio.h>
#include <unistd.h>

#include <fusionsound.h>

int
main (int argc, char *argv[])
{
     DFBResult                  ret;
     IFusionSound              *sound;
     IFusionSoundMusicProvider *provider;
     IFusionSoundStream        *stream;
     FSStreamDescription        desc;

     ret = FusionSoundInit (&argc, &argv);
     if (ret)
          DirectFBErrorFatal ("FusionSoundInit", ret);

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     /* Retrieve the main sound interface. */
     ret = FusionSoundCreate (&sound);
     if (ret)
          DirectFBErrorFatal ("FusionSoundCreate", ret);

     /* Create a music provider for the specified file. */
     ret = sound->CreateMusicProvider (sound, argv[1], &provider);
     if (ret)
          DirectFBErrorFatal ("IFusionSound::CreateMusicProvider", ret);

     /* Query provider for a suitable stream description. */
     ret = provider->GetStreamDescription (provider, &desc);
     if (ret)
          DirectFBErrorFatal ("IFusionSoundMusicProvider::"
                              "GetStreamDescription", ret);

     /* Create the sound stream and feed it. */
     ret = sound->CreateStream (sound, &desc, &stream);
     if (ret) {
          DirectFBError ("IFusionSound::CreateStream", ret);
     }
     else {
          /* Let the provider play the music using our stream. */
          ret = provider->PlayTo (provider, stream);
          if (ret)
               DirectFBError ("IFusionSoundMusicProvider::PlayTo", ret);

          sleep (1);

          /* Wait for end of stream (ring buffer holds ~3/4 sec). */
          stream->Wait (stream, 0);

          stream->Release (stream);
     }

     provider->Release (provider);
     sound->Release (sound);

     return 0;
}


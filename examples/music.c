#include <stdio.h>
#include <unistd.h>

#include <fusionsound.h>

int
main (int argc, char *argv[])
{
     DFBResult                  ret;
     IDirectFB                 *dfb;
     IFusionSound              *sound;
     IFusionSoundMusicProvider *provider;
     IFusionSoundStream        *stream;
     FSStreamDescription        desc;

     ret = DirectFBInit (&argc, &argv);
     if (ret)
          DirectFBErrorFatal ("DirectFBInit", ret);

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     ret = DirectFBCreate (&dfb);
     if (ret)
          DirectFBErrorFatal ("DirectFBCreate", ret);

     /* Retrieve the main sound interface. */
     ret = dfb->GetInterface (dfb, "IFusionSound", NULL, NULL, (void**) &sound);
     if (ret)
          DirectFBErrorFatal ("IDirectFB::GetInterface", ret);

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
     dfb->Release (dfb);

     return 0;
}


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <fusionsound.h>

static void
feed_stream (IFusionSoundStream *stream)
{
     DFBResult ret;
     int       i;
     __s16     buf[16384];

     /* Generate woops ;) */
     for (i=0; i<16384; i++)
          buf[i] = (__s16)( sin(i*i/(16384.0*16384.0)*M_PI*200) * 10000 );

     /* Write eight times (~3 seconds of playback). */
     for (i=0; i<8; i++) {
          /*
           * Wait for a larger chunk of free space. Avoids a blocking write
           * with very small partially writes each time there's new space.
           *
           * This wait is for demonstrational purpose, in practice blocking
           * writes are no overhead and always keeping the buffer at the
           * highest fill level is more safe.
           */
          ret = stream->Wait (stream, 16384);
          if (ret) {
               DirectFBError ("IFusionSoundStream::Write", ret);
               return;
          }
          
          /* This write won't block anymore. */
          ret = stream->Write (stream, buf, 16384);
          if (ret) {
               DirectFBError ("IFusionSoundStream::Write", ret);
               return;
          }
     }
}

int
main (int argc, char *argv[])
{
     DFBResult            ret;
     IDirectFB           *dfb;
     IFusionSound        *sound;
     IFusionSoundStream  *stream;
     FSStreamDescription  desc;

     ret = DirectFBInit (&argc, &argv);
     if (ret)
          DirectFBErrorFatal ("DirectFBInit", ret);

     ret = DirectFBCreate (&dfb);
     if (ret)
          DirectFBErrorFatal ("DirectFBCreate", ret);

     /* Retrieve the main sound interface. */
     ret = dfb->GetInterface (dfb, "IFusionSound", NULL, NULL, (void**) &sound);
     if (ret)
          DirectFBErrorFatal ("IDirectFB::GetInterface", ret);

     /* Fill stream description (using defaults of 44kHz and 16bit). */
     desc.flags      = FSSDF_BUFFERSIZE | FSSDF_CHANNELS;
     desc.buffersize = 32768;
     desc.channels   = 1;

     /* Create the sound stream and feed it. */
     ret = sound->CreateStream (sound, &desc, &stream);
     if (ret) {
          DirectFBError ("IFusionSound::CreateStream", ret);
     }
     else {
          /* Fill the ring buffer with our generated data. */
          feed_stream (stream);

          /* Wait for end of stream (ring buffer holds ~3/4 sec). */
          sleep (1);

          stream->Release (stream);
     }

     sound->Release (sound);
     dfb->Release (dfb);

     return 0;
}


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#include <fusionsound.h>

#include "loader.h"

int main (int argc, char *argv[])
{
     DFBResult            ret;
     IDirectFB           *dfb;
     IFusionSound        *sound;
     IFusionSoundBuffer  *buffer;

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

     ret = dfb->GetInterface (dfb, "IFusionSound", NULL, NULL, (void**) &sound);
     if (ret)
          DirectFBErrorFatal ("IDirectFB::GetInterface", ret);

     buffer = load_sample (sound, argv[1]);
     if (buffer) {
          buffer->Play (buffer, FSPLAY_LOOPING);
          
          sleep (3);
          
          buffer->Release (buffer);
     }

     sound->Release (sound);
     dfb->Release (dfb);

     return 0;
}


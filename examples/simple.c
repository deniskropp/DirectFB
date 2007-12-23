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
     IFusionSound        *sound;
     IFusionSoundBuffer  *buffer;

     ret = FusionSoundInit (&argc, &argv);
     if (ret)
          FusionSoundErrorFatal ("FusionSoundInit", ret);

     if (argc != 2) {
          fprintf (stderr, "\nUsage: %s <filename>\n", argv[0]);
          return -1;
     }

     ret = FusionSoundCreate (&sound);
     if (ret)
          FusionSoundErrorFatal ("FusionSoundCreate", ret);

     buffer = load_sample (sound, argv[1]);
     if (buffer) {
          buffer->Play (buffer, FSPLAY_LOOPING);
          
          sleep (3);
          
          buffer->Release (buffer);
     }

     sound->Release (sound);

     return 0;
}

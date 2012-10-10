#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

#include <fusionsound.h>


int main (int argc, char *argv[])
{
     DirectResult         ret;
     IFusionSound        *sound;

     ret = FusionSoundInit (&argc, &argv);
     if (ret)
          FusionSoundErrorFatal ("FusionSoundInit", ret);

     ret = FusionSoundCreate (&sound);
     if (ret)
          FusionSoundErrorFatal ("FusionSoundCreate", ret);

     while (true) {
          float left, right;

          sound->GetMasterFeedback( sound, &left, &right );

          printf( "%f %f\n", left, right );

          sleep( 1 );
     }

     sound->Release (sound);

     return 0;
}

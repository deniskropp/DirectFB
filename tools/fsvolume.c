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

#include <fusionsound.h>


static void 
usage( const char *progname )
{
     fprintf( stderr, 
              "Usage:\n"
              "  %s\n"
              "      Print volume level and quit\n\n"
              "  %s [0.0 ... 1.0]\n"
              "      Set volume level to given value\n\n"
              "  %s +/-[0.0 ... 1.0]\n"
              "      Adjust volume level by given value\n\n",
              progname, progname, progname );
     
     exit( EXIT_FAILURE );
}

int
main( int argc, char **argv )
{
     DFBResult     ret;
     IFusionSound *sound;
     float         volume = 0.0f;

     if (argc > 1) {
          if (!strcmp( argv[1], "-h" ) || !strcmp( argv[1], "--help" ))
               usage( argv[0] );
     }

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundInit()", ret );
          
     ret = FusionSoundCreate( &sound );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundCreate()", ret );

     if (argc > 1) {
          if (argv[1][0] == '+' || argv[1][0] == '-') {
               ret = sound->GetMasterVolume( sound, &volume );
               if (ret)
                    FusionSoundError( "IFusiondSound::GetVolume()", ret );
          }                    

          ret = sound->SetMasterVolume( sound, volume + strtof(argv[1], NULL) );
          if (ret)
               FusionSoundError( "IFusiondSound::SetVolume()", ret );
     }
     else {
          ret = sound->GetMasterVolume( sound, &volume );
          if (ret)
               FusionSoundError( "IFusiondSound::GetVolume()", ret );
          
          printf( "%.3f\n", volume );
     }

     sound->Release( sound );

     return ret;
}


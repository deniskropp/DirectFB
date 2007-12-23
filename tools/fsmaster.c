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

#include <unistd.h>

#include <fusionsound.h>


int
main( int argc, char *argv[] )
{
     DFBResult     ret;
     IFusionSound *sound;

     ret = FusionSoundInit( &argc, &argv );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundInit", ret );

     ret = FusionSoundCreate( &sound );
     if (ret)
          FusionSoundErrorFatal( "FusionSoundCreate", ret );

     pause();

     sound->Release( sound );

     return 0;
}

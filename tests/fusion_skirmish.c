/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   All rights reserved.

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

#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>

#include <directfb.h>

#include <direct/build.h>
#include <direct/debug.h>
#include <direct/log.h>
#include <direct/messages.h>

#include <fusion/fusion.h>
#include <fusion/reactor.h>
#include <fusion/ref.h>


#if DIRECT_BUILD_DEBUGS
#define MSG(x...)                       \
do {                                    \
     direct_debug( x ); \
} while (0)
#else
#define MSG(x...)                       \
do {                                    \
} while (0)
#endif


static FusionWorld   *m_world;


int
main( int argc, char *argv[] )
{
     DirectResult   ret;
     FusionSkirmish skirmish;

     DirectFBInit( &argc, &argv );

     ret = fusion_enter( -1, 0, FER_MASTER, &m_world );
     if (ret) {
          D_DERROR( ret, "fusion_enter() failed" );
          return ret;
     }

     MSG( "Entered world %d as master (FusionID %lu, pid %d)\n",
          fusion_world_index( m_world ), fusion_id( m_world ), getpid() );


     MSG( "Initializing skirmish...\n" );

     ret = fusion_skirmish_init( &skirmish, "Test", m_world );
     if (ret) {
          D_DERROR( ret, "fusion_skirmish_init() failed\n" );
          return -1;
     }


     MSG( "Locking skirmish...\n" );

     ret = fusion_skirmish_prevail( &skirmish );
     if (ret) {
          D_DERROR( ret, "fusion_skirmish_prevail() failed!\n" );
          return -2;
     }


     MSG( "Waiting at skirmish...\n" );

     ret = fusion_skirmish_wait( &skirmish, 10 );
     if (ret != DFB_TIMEOUT) {
          D_DERROR( ret, "fusion_skirmish_wait() did not timeout!\n" );
          return -3;
     }


     MSG( "Unlocking skirmish...\n" );

     ret = fusion_skirmish_dismiss( &skirmish );
     if (ret) {
          D_DERROR( ret, "fusion_skirmish_dismiss() failed!\n" );
          return -4;
     }


     MSG( "Exiting from world %d (FusionID %lu, pid %d)...\n",
          fusion_world_index( m_world ), fusion_id( m_world ), getpid() );

     fusion_exit( m_world, false );

     return 0;
}


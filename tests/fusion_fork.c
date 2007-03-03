/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <direct/log.h>
#include <direct/messages.h>

#include <fusion/fusion.h>
#include <fusion/reactor.h>
#include <fusion/ref.h>


#define MSG(x...)                       \
do {                                    \
     direct_log_printf( NULL, "- " x ); \
} while (0)


typedef struct {
     unsigned int foo;
} TestMessage;


static FusionWorld   *m_world;
static FusionRef      m_ref;
static FusionReactor *m_reactor;
static Reaction       m_reaction;


static ReactionResult
reaction_callback( const void *msg_data,
                   void       *ctx )
{
     MSG( "Received message (FusionID %lu, pid %d)!\n", fusion_id( m_world ), getpid() );

     return RS_OK;
}


int
main( int argc, char *argv[] )
{
     DirectResult ret;
     pid_t        child_pid;
     TestMessage  message = {0};

     DirectFBInit( &argc, &argv );

     ret = fusion_enter( -1, 0, FER_MASTER, &m_world );
     if (ret) {
          D_DERROR( ret, "fusion_enter() failed" );
          return ret;
     }

     MSG( "Entered world %d as master (FusionID %lu, pid %d)\n",
          fusion_world_index( m_world ), fusion_id( m_world ), getpid() );

     fusion_world_set_fork_action( m_world, FFA_FORK );


     ret = fusion_ref_init( &m_ref, "Test", m_world );
     if (ret) {
          D_DERROR( ret, "fusion_ref_init() failed" );
          return -1;
     }


     MSG( "Adding local reference...\n" );

     fusion_ref_up( &m_ref, false );


     m_reactor = fusion_reactor_new( sizeof(TestMessage), "Test", m_world );
     if (!m_reactor) {
          D_ERROR( "fusion_reactor_new() failed\n" );
          return -1;
     }


     MSG( "Attaching to reactor...\n" );

     ret = fusion_reactor_attach( m_reactor, reaction_callback, NULL, &m_reaction );
     if (ret) {
          D_DERROR( ret, "fusion_reactor_attach() failed" );
          return ret;
     }


     MSG( ".........FORKING NOW.........\n" );

     child_pid = fork();

     switch (child_pid) {
          case -1:
               D_PERROR( "fork() failed" );
               break;

          case 0:
               setsid();
               MSG( "...arrived after fork() in child (pid %d)..\n", getpid() );
               MSG( "..child (FusionID %lu).\n", fusion_id( m_world ) );
               break;

          default:
               usleep( 200000 );
               MSG( "...returned from fork() in parent, child pid %d.\n", child_pid );
               break;
     }


     MSG( "Sending message via reactor...\n" );

     fusion_reactor_dispatch( m_reactor, &message, true, NULL );

     usleep( 200000 );


     MSG( "Removing local reference...\n" );

     fusion_ref_down( &m_ref, false );

     usleep( 200000 );


     MSG( "Exiting from world %d (FusionID %lu, pid %d)...\n",
          fusion_world_index( m_world ), fusion_id( m_world ), getpid() );

     fusion_exit( m_world, false );

     return 0;
}


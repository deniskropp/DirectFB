/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <signal.h>

#include <stdlib.h>
#include <stdio.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/core.h>
#include <core/sig.h>

#include <misc/conf.h>
#include <misc/debug.h>

typedef struct {
     int              signum;
     struct sigaction old_action;
} SigHandled;

static int sigs_to_handle[] = { /*SIGALRM,*/ SIGHUP, SIGINT, /*SIGPIPE,*/ /*SIGPOLL,*/
                                SIGTERM, /*SIGUSR1, SIGUSR2,*/ /*SIGVTALRM,*/
                                /*SIGSTKFLT,*/ SIGABRT, SIGFPE, SIGILL, SIGQUIT,
                                SIGSEGV, SIGTRAP, /*SIGSYS, SIGEMT,*/ SIGBUS,
                                SIGXCPU, SIGXFSZ };

#define NUM_SIGS_TO_HANDLE ((int)(sizeof(sigs_to_handle)/sizeof(sigs_to_handle[0])))

static CoreDFB    *core_dfb = NULL;
static SigHandled  sigs_handled[NUM_SIGS_TO_HANDLE];


static bool
show_segv( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef SEGV_MAPERR
          case SEGV_MAPERR:
               fprintf( stderr, " (at %p, invalid address) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef SEGV_ACCERR
          case SEGV_ACCERR:
               fprintf( stderr, " (at %p, invalid permissions) <--\n",
                        info->si_addr );
               return true;
#endif
     }
     return false;
}

static bool
show_bus( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef BUG_ADRALN
          case BUS_ADRALN:
               fprintf( stderr, " (at %p, invalid address alignment) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef BUS_ADRERR
          case BUS_ADRERR:
               fprintf( stderr, " (at %p, non-existent physical address) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef BUS_OBJERR
          case BUS_OBJERR:
               fprintf( stderr, " (at %p, object specific hardware error) <--\n",
                        info->si_addr );
               return true;
#endif
     }

     return false;
}

static bool
show_ill( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef ILL_ILLOPC
          case ILL_ILLOPC:
               fprintf( stderr, " (at %p, illegal opcode) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_ILLOPN
          case ILL_ILLOPN:
               fprintf( stderr, " (at %p, illegal operand) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_ILLADR
          case ILL_ILLADR:
               fprintf( stderr, " (at %p, illegal addressing mode) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_ILLTRP
          case ILL_ILLTRP:
               fprintf( stderr, " (at %p, illegal trap) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_PRVOPC
          case ILL_PRVOPC:
               fprintf( stderr, " (at %p, privileged opcode) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_PRVREG
          case ILL_PRVREG:
               fprintf( stderr, " (at %p, privileged register) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_COPROC
          case ILL_COPROC:
               fprintf( stderr, " (at %p, coprocessor error) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef ILL_BADSTK
          case ILL_BADSTK:
               fprintf( stderr, " (at %p, internal stack error) <--\n",
                        info->si_addr );
               return true;
#endif
     }

     return false;
}

static bool
show_fpe( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef FPE_INTDIV
          case FPE_INTDIV:
               fprintf( stderr, " (at %p, integer divide by zero) <--\n",
                        info->si_addr );
               return true;
#endif
#ifdef FPE_FLTDIV
          case FPE_FLTDIV:
               fprintf( stderr, " (at %p, floating point divide by zero) <--\n",
                        info->si_addr );
               return true;
#endif
     }

     fprintf( stderr, " (at %p) <--\n", info->si_addr );

     return true;
}

static bool
show_any( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef SI_USER
          case SI_USER:
               fprintf( stderr, " (sent by pid %d, uid %d) <--\n",
                        info->si_pid, info->si_uid );
               return true;
#endif
#ifdef SI_KERNEL
          case SI_KERNEL:
               fprintf( stderr, " (sent by the kernel) <--\n" );
               return true;
#endif
     }
     return false;
}

static void
dfb_sig_action( int num, siginfo_t *info, void *foo )
{
     int       pid    = gettid();
     long long millis = fusion_get_millis();

     fprintf( stderr, "(!) [%5d: %4lld.%03lld] --> Caught signal %d",
              pid, millis/1000, millis%1000, num );

     if (info) {
          bool shown = false;

          if (info->si_code > 0 && info->si_code < 0x80) {
               switch (num) {
                    case SIGSEGV:
                         shown = show_segv( info );

                         if (shown && fusion_shmalloc_cure( info->si_addr )) {
                              millis = fusion_get_millis();

                              fprintf( stderr,
                                       "(!) [%5d: %4lld.%03lld]      -> cured!"
                                       "\n", pid, millis/1000, millis%1000 );
                              fflush( stderr );
                              return;
                         }

                         break;

                    case SIGBUS:
                         shown = show_bus( info );
                         break;

                    case SIGILL:
                         shown = show_ill( info );
                         break;

                    case SIGFPE:
                         shown = show_fpe( info );
                         break;

                    default:
                         fprintf( stderr, " <--\n" );
                         shown = true;
                         break;
               }
          }
          else
               shown = show_any( info );

          if (!shown)
               fprintf( stderr, " (unknown origin) <--\n" );
     }
     else
          fprintf( stderr, ", no siginfo available <--\n" );

     fflush( stderr );

     dfb_trace_print_stacks();

     dfb_core_destroy( core_dfb, true );

     killpg( 0, num );

     _exit( -1 );
}

void
dfb_sig_install_handlers( CoreDFB *core )
{
     int i;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core_dfb == NULL );

     core_dfb = core;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (dfb_config->sighandler &&
              !sigismember( &dfb_config->dont_catch, sigs_to_handle[i] ))
          {
               struct sigaction action;
               int              signum = sigs_to_handle[i];

               action.sa_sigaction = dfb_sig_action;
               action.sa_flags     = SA_RESTART | SA_SIGINFO;

               if (signum != SIGSEGV)
                    action.sa_flags |= SA_RESETHAND;

               sigfillset( &action.sa_mask );

               if (sigaction( signum, &action, &sigs_handled[i].old_action )) {
                    PERRORMSG("DirectFB/core/sig: Unable to install signal "
                              "handler for signal %d!\n", signum);
                    continue;
               }

               sigs_handled[i].signum = signum;
          }
     }
}

void
dfb_sig_remove_handlers( CoreDFB *core )
{
     int i;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core_dfb == core );

     core_dfb = NULL;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum != -1) {
               int signum = sigs_handled[i].signum;

               if (sigaction( signum, &sigs_handled[i].old_action, NULL )) {
                    PERRORMSG("DirectFB/core/sig: Unable to restore previous "
                              "handler for signal %d!\n", signum);
               }

               sigs_handled[i].signum = -1;
          }
     }
}

void
dfb_sig_block_all()
{
     sigset_t signals;

     sigfillset( &signals );

     if (pthread_sigmask( SIG_BLOCK, &signals, NULL ))
          PERRORMSG( "DirectFB/Core: Setting signal mask failed!\n" );
}

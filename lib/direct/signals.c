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

#include <pthread.h>

#include <signal.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <direct/clock.h>
#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/trace.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Signals, "Direct/Signals", "Signal handling" );


struct __D_DirectSignalHandler {
     DirectLink               link;

     int                      magic;

     int                      num;
     DirectSignalHandlerFunc  func;
     void                    *ctx;
};

/**************************************************************************************************/

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

static SigHandled sigs_handled[NUM_SIGS_TO_HANDLE];

static DirectLink      *handlers = NULL;
static pthread_mutex_t  handlers_lock;

/**************************************************************************************************/

static void install_handlers();
static void remove_handlers();

/**************************************************************************************************/

DirectResult
direct_signals_initialize()
{
     D_DEBUG_AT( Direct_Signals, "Initializing...\n" );

     direct_util_recursive_pthread_mutex_init( &handlers_lock );

     install_handlers();

     return DFB_OK;
}

DirectResult
direct_signals_shutdown()
{
     D_DEBUG_AT( Direct_Signals, "Shutting down...\n" );

     remove_handlers();

     pthread_mutex_destroy( &handlers_lock );

     return DFB_OK;
}

void
direct_signals_block_all()
{
     sigset_t signals;

     D_DEBUG_AT( Direct_Signals, "Blocking all signals from now on!\n" );

     sigfillset( &signals );

     if (pthread_sigmask( SIG_BLOCK, &signals, NULL ))
          D_PERROR( "Direct/Signals: Setting signal mask failed!\n" );
}

DirectResult
direct_signal_handler_add( int                       num,
                           DirectSignalHandlerFunc   func,
                           void                     *ctx,
                           DirectSignalHandler     **ret_handler )
{
     DirectSignalHandler *handler;

     D_ASSERT( func != NULL );
     D_ASSERT( ret_handler != NULL );

     D_DEBUG_AT( Direct_Signals,
                 "Adding handler %p for signal %d with context %p...\n", func, num, ctx );

     handler = D_CALLOC( 1, sizeof(DirectSignalHandler) );
     if (!handler) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     handler->num  = num;
     handler->func = func;
     handler->ctx  = ctx;

     D_MAGIC_SET( handler, DirectSignalHandler );

     pthread_mutex_lock( &handlers_lock );
     direct_list_append( &handlers, &handler->link );
     pthread_mutex_unlock( &handlers_lock );

     *ret_handler = handler;

     return DFB_OK;
}

DirectResult
direct_signal_handler_remove( DirectSignalHandler *handler )
{
     D_MAGIC_ASSERT( handler, DirectSignalHandler );

     D_DEBUG_AT( Direct_Signals, "Removing handler %p for signal %d with context %p...\n",
                 handler->func, handler->num, handler->ctx );

     pthread_mutex_lock( &handlers_lock );
     direct_list_remove( &handlers, &handler->link );
     pthread_mutex_unlock( &handlers_lock );

     D_MAGIC_CLEAR( handler );

     D_FREE( handler );

     return DFB_OK;
}

/**************************************************************************************************/

static bool
show_segv( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef SEGV_MAPERR
          case SEGV_MAPERR:
               direct_log_printf( NULL, " (at %p, invalid address) <--\n", info->si_addr );
               return true;
#endif
#ifdef SEGV_ACCERR
          case SEGV_ACCERR:
               direct_log_printf( NULL, " (at %p, invalid permissions) <--\n", info->si_addr );
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
               direct_log_printf( NULL, " (at %p, invalid address alignment) <--\n", info->si_addr );
               return true;
#endif
#ifdef BUS_ADRERR
          case BUS_ADRERR:
               direct_log_printf( NULL, " (at %p, non-existent physical address) <--\n", info->si_addr );
               return true;
#endif
#ifdef BUS_OBJERR
          case BUS_OBJERR:
               direct_log_printf( NULL, " (at %p, object specific hardware error) <--\n", info->si_addr );
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
               direct_log_printf( NULL, " (at %p, illegal opcode) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_ILLOPN
          case ILL_ILLOPN:
               direct_log_printf( NULL, " (at %p, illegal operand) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_ILLADR
          case ILL_ILLADR:
               direct_log_printf( NULL, " (at %p, illegal addressing mode) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_ILLTRP
          case ILL_ILLTRP:
               direct_log_printf( NULL, " (at %p, illegal trap) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_PRVOPC
          case ILL_PRVOPC:
               direct_log_printf( NULL, " (at %p, privileged opcode) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_PRVREG
          case ILL_PRVREG:
               direct_log_printf( NULL, " (at %p, privileged register) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_COPROC
          case ILL_COPROC:
               direct_log_printf( NULL, " (at %p, coprocessor error) <--\n", info->si_addr );
               return true;
#endif
#ifdef ILL_BADSTK
          case ILL_BADSTK:
               direct_log_printf( NULL, " (at %p, internal stack error) <--\n", info->si_addr );
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
               direct_log_printf( NULL, " (at %p, integer divide by zero) <--\n", info->si_addr );
               return true;
#endif
#ifdef FPE_FLTDIV
          case FPE_FLTDIV:
               direct_log_printf( NULL, " (at %p, floating point divide by zero) <--\n", info->si_addr );
               return true;
#endif
     }

     direct_log_printf( NULL, " (at %p) <--\n", info->si_addr );

     return true;
}

static bool
show_any( const siginfo_t *info )
{
     switch (info->si_code) {
#ifdef SI_USER
          case SI_USER:
               direct_log_printf( NULL, " (sent by pid %d, uid %d) <--\n", info->si_pid, info->si_uid );
               return true;
#endif
#ifdef SI_KERNEL
          case SI_KERNEL:
               direct_log_printf( NULL, " (sent by the kernel) <--\n" );
               return true;
#endif
     }
     return false;
}

static void
signal_handler( int num, siginfo_t *info, void *foo )
{
     DirectLink *l, *n;
     void       *addr   = NULL;
     int         pid    = direct_gettid();
     long long   millis = direct_clock_get_millis();

     direct_log_printf( NULL, "(!) [%5d: %4lld.%03lld] --> Caught signal %d",
                        pid, millis/1000, millis%1000, num );

     if (info && info > (siginfo_t*) 0x100) {
          bool shown = false;

          if (info->si_code > 0 && info->si_code < 0x80) {
               addr = info->si_addr;

               switch (num) {
                    case SIGSEGV:
                         shown = show_segv( info );
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
                         direct_log_printf( NULL, " <--\n" );
                         addr  = NULL;
                         shown = true;
                         break;
               }
          }
          else
               shown = show_any( info );

          if (!shown)
               direct_log_printf( NULL, " (unknown origin) <--\n" );
     }
     else
          direct_log_printf( NULL, ", no siginfo available <--\n" );

     direct_trace_print_stacks();

     /* Loop through all handlers. */
     pthread_mutex_lock( &handlers_lock );

     direct_list_foreach_safe (l, n, handlers) {
          DirectSignalHandler *handler = (DirectSignalHandler*) l;

          if (handler->num != num && handler->num != DIRECT_SIGNAL_ANY)
               continue;

          switch (handler->func( num, addr, handler->ctx )) {
               case DSHR_OK:
                    break;

               case DSHR_REMOVE:
                    direct_list_remove( &handlers, &handler->link );
                    D_MAGIC_CLEAR( handler );
                    D_FREE( handler );
                    break;

               case DSHR_RESUME:
                    millis = direct_clock_get_millis();

                    direct_log_printf( NULL, "(!) [%5d: %4lld.%03lld]      -> cured!\n",
                                       pid, millis / 1000, millis % 1000 );
                    pthread_mutex_unlock( &handlers_lock );
                    return;

               default:
                    D_BUG( "unknown result" );
                    break;
          }
     }

     pthread_mutex_unlock( &handlers_lock );

     remove_handlers();

     raise( num );
     
     abort();

     exit( -num );
}

/**************************************************************************************************/

static void
install_handlers()
{
     int i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (direct_config->sighandler && !sigismember( &direct_config->dont_catch,
                                                         sigs_to_handle[i] ))
          {
               struct sigaction action;
               int              signum = sigs_to_handle[i];

               action.sa_sigaction = signal_handler;
               action.sa_flags     = SA_SIGINFO;
                
               if (signum != SIGSEGV)
                    action.sa_flags |= SA_NODEFER;

               sigemptyset( &action.sa_mask );

               if (sigaction( signum, &action, &sigs_handled[i].old_action )) {
                    D_PERROR( "Direct/Signals: "
                              "Unable to install signal handler for signal %d!\n", signum );
                    continue;
               }

               sigs_handled[i].signum = signum;
          }
     }
}

static void
remove_handlers()
{
     int i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum != -1) {
               int signum = sigs_handled[i].signum;

               if (sigaction( signum, &sigs_handled[i].old_action, NULL )) {
                    D_PERROR( "Direct/Signals: "
                              "Unable to restore previous handler for signal %d!\n", signum );
               }

               sigs_handled[i].signum = -1;
          }
     }
}


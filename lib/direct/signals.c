/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <direct/clock.h>
#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Signals, "Direct/Signals", "Signal handling" );


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
     struct sigaction new_action;
} SigHandled;

static int sigs_to_handle[] = { /*SIGALRM,*/ SIGHUP, SIGINT, /*SIGPIPE,*/ /*SIGPOLL,*/
                                SIGTERM, /*SIGUSR1, SIGUSR2,*/ /*SIGVTALRM,*/
                                /*SIGSTKFLT,*/ SIGABRT, SIGFPE, SIGILL, SIGQUIT,
                                SIGSEGV, SIGTRAP, /*SIGSYS, SIGEMT,*/ SIGBUS,
                                SIGXCPU, SIGXFSZ };

#define NUM_SIGS_TO_HANDLE ((int)D_ARRAY_SIZE( sigs_to_handle ))

static SigHandled sigs_handled[NUM_SIGS_TO_HANDLE];

static DirectLink  *handlers = NULL;
static DirectMutex  handlers_lock;

/**************************************************************************************************/

static void install_handlers( void );
static void remove_handlers( void );

/**************************************************************************************************/

DirectResult
direct_signals_initialize( void )
{
     D_DEBUG_AT( Direct_Signals, "Initializing...\n" );

     direct_recursive_mutex_init( &handlers_lock );

     install_handlers();

     return DR_OK;
}

DirectResult
direct_signals_shutdown( void )
{
     D_DEBUG_AT( Direct_Signals, "Shutting down...\n" );

     remove_handlers();

     direct_mutex_deinit( &handlers_lock );

     return DR_OK;
}

void
direct_signals_block_all( void )
{
     sigset_t signals;

     D_DEBUG_AT( Direct_Signals, "Blocking all signals from now on!\n" );

     sigfillset( &signals );

     if (sigprocmask( SIG_BLOCK, &signals, NULL ))
          ;//D_PERROR( "Direct/Signals: Setting signal mask failed!\n" );
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
          return DR_NOLOCALMEMORY;
     }

     handler->num  = num;
     handler->func = func;
     handler->ctx  = ctx;

     D_MAGIC_SET( handler, DirectSignalHandler );

     direct_mutex_lock( &handlers_lock );
     direct_list_append( &handlers, &handler->link );
     direct_mutex_unlock( &handlers_lock );

     *ret_handler = handler;

     return DR_OK;
}

DirectResult
direct_signal_handler_remove( DirectSignalHandler *handler )
{
     D_MAGIC_ASSERT( handler, DirectSignalHandler );

     D_DEBUG_AT( Direct_Signals, "Removing handler %p for signal %d with context %p...\n",
                 handler->func, handler->num, handler->ctx );

     direct_mutex_lock( &handlers_lock );
     direct_list_remove( &handlers, &handler->link );
     direct_mutex_unlock( &handlers_lock );

     D_MAGIC_CLEAR( handler );

     D_FREE( handler );

     return DR_OK;
}

/**************************************************************************************************/

#ifdef SA_SIGINFO

#include <ucontext.h>

static bool
show_segv( const siginfo_t *info, ucontext_t *uctx )
{
     switch (info->si_code) {
#ifdef SEGV_MAPERR
          case SEGV_MAPERR:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, invalid address) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef SEGV_ACCERR
          case SEGV_ACCERR:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, invalid permissions) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
     }
     return false;
}

static bool
show_bus( const siginfo_t *info, ucontext_t *uctx )
{
     switch (info->si_code) {
#ifdef BUG_ADRALN
          case BUS_ADRALN:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, invalid address alignment) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef BUS_ADRERR
          case BUS_ADRERR:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, non-existent physical address) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef BUS_OBJERR
          case BUS_OBJERR:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, object specific hardware error) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
     }

     return false;
}

static bool
show_ill( const siginfo_t *info, ucontext_t *uctx )
{
     switch (info->si_code) {
#ifdef ILL_ILLOPC
          case ILL_ILLOPC:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, illegal opcode) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_ILLOPN
          case ILL_ILLOPN:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, illegal operand) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_ILLADR
          case ILL_ILLADR:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, illegal addressing mode) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_ILLTRP
          case ILL_ILLTRP:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, illegal trap) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_PRVOPC
          case ILL_PRVOPC:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, privileged opcode) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_PRVREG
          case ILL_PRVREG:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, privileged register) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_COPROC
          case ILL_COPROC:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, coprocessor error) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef ILL_BADSTK
          case ILL_BADSTK:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, internal stack error) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
     }

     return false;
}

static bool
show_fpe( const siginfo_t *info, ucontext_t *uctx )
{
     switch (info->si_code) {
#ifdef FPE_INTDIV
          case FPE_INTDIV:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, integer divide by zero) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
#ifdef FPE_FLTDIV
          case FPE_FLTDIV:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p, floating point divide by zero) <--\n", info->si_signo, info->si_addr );
               return true;
#endif
     }

     D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (at %p) <--\n", info->si_signo, info->si_addr );

     return true;
}

static bool
show_any( const siginfo_t *info, ucontext_t *uctx )
{
     switch (info->si_code) {
#ifdef SI_USER
          case SI_USER:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (sent by pid %d, uid %d) <--\n", info->si_signo, info->si_pid, info->si_uid );
               return true;
#endif
#ifdef SI_QUEUE
          case SI_QUEUE:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (queued by pid %d, uid %d, val %d, uctx %p) <--\n",
                      info->si_signo, info->si_pid, info->si_uid, info->si_int, uctx );
               return true;
#endif
#ifdef SI_KERNEL
          case SI_KERNEL:
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (sent by the kernel) <--\n", info->si_signo );
               return true;
#endif
     }
     return false;
}

#endif

#ifdef SA_SIGINFO
static void
signal_handler( int num, siginfo_t *info, void *foo )
{
     ucontext_t   *uctx = foo;
#else
static void
signal_handler( int num )
{
#endif
     DirectResult  ret;
     int           i;
     DirectLink   *l, *n;
     SigHandled   *sig  = NULL;
     void         *addr = NULL;

//     fflush(stdout);
//     fflush(stderr);

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum == num) {
               sig = &sigs_handled[i];

               /* Using SA_RESETHAND, so... */
               sig->signum = -1;
               break;
          }
     }

/*     ret = direct_sigaction( num, &sig->old_action, NULL );
     if (ret) {
          D_DERROR( ret, "Direct/Signals: Unable to restore previous handler for signal %d!\n", num );
          sig = NULL;
     }
*/

#ifndef SA_SIGINFO
     D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d <--\n", num );
#else
     if (info && info > (siginfo_t*) 0x100) {
          bool shown = false;

          /* Kernel genrated signal? */
          if (info->si_code > 0 && info->si_code < 0x80) {
               addr = info->si_addr;

               switch (num) {
                    case SIGSEGV:
                         shown = show_segv( info, uctx );
                         break;

                    case SIGBUS:
                         shown = show_bus( info, uctx );
                         break;

                    case SIGILL:
                         shown = show_ill( info, uctx );
                         break;

                    case SIGFPE:
                         shown = show_fpe( info, uctx );
                         break;

                    default:
                         D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d <--\n", info->si_signo );
                         addr  = NULL;
                         shown = true;
                         break;
               }
          }
          else
               shown = show_any( info, uctx );

          if (!shown)
               D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d (unknown origin) <--\n", info->si_signo );
     }
     else
          D_LOG( Direct_Signals, FATAL, "    --> Caught signal %d, no siginfo available <--\n", num );
#endif

     direct_log_lock( NULL );
     direct_trace_print_stacks();
     direct_log_unlock( NULL );


     /* Loop through all handlers. */
     direct_mutex_lock( &handlers_lock );

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
                    D_LOG( Direct_Signals, FATAL, "    '-> cured!\n" );

                    direct_mutex_unlock( &handlers_lock );

                    if (sig) {
                         ret = direct_sigaction( num, &sig->new_action, NULL );
                         if (ret)
                              D_DERROR( ret, "Direct/Signals: Unable to reinstall handler for signal %d!\n", num );
                         else
                              sig->signum = num;
                    }
                    return;

               default:
                    D_BUG( "unknown result" );
                    break;
          }
     }

     direct_mutex_unlock( &handlers_lock );

     remove_handlers();

     direct_trap( "SigHandler", num );
}

/**************************************************************************************************/

static void
install_handlers( void )
{
     DirectResult  ret;
     int           i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (direct_config->sighandler && !sigismember( &direct_config->dont_catch, sigs_to_handle[i] )) {
               struct sigaction action;
               int              signum = sigs_to_handle[i];

#ifdef SA_SIGINFO
               action.sa_sigaction = signal_handler;
               action.sa_flags     = SA_SIGINFO;
#else
               action.sa_handler   = signal_handler;
               action.sa_flags     = 0;
#endif

#ifdef SA_NODEFER
               if (signum != SIGSEGV)
                    action.sa_flags |= SA_NODEFER;
#endif

#ifdef SA_RESETHAND
               action.sa_flags |= SA_RESETHAND;
#endif

#ifdef SA_RESTART
               action.sa_flags |= SA_RESTART;
#endif

               sigemptyset( &action.sa_mask );

               ret = direct_sigaction( signum, &action, &sigs_handled[i].old_action );
               if (ret) {
                    D_DERROR( ret, "Direct/Signals: "
                              "Unable to install signal handler for signal %d!\n", signum );
                    continue;
               }

               sigs_handled[i].new_action = action;
               sigs_handled[i].signum     = signum;
          }
     }
}

static void
remove_handlers( void )
{
     DirectResult  ret;
     int           i;

     for (i=0; i<NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum != -1) {
               int signum = sigs_handled[i].signum;

               ret = direct_sigaction( signum, &sigs_handled[i].old_action, NULL );
               if (ret) {
                    D_DERROR( ret, "Direct/Signals: "
                              "Unable to restore previous handler for signal %d!\n", signum );
               }

               sigs_handled[i].signum = -1;
          }
     }
}


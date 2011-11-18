/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/param.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/build.h>
#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/shmalloc.h>

#include "fusion_internal.h"

D_DEBUG_DOMAIN( Fusion_Skirmish, "Fusion/Skirmish", "Fusion's Skirmish (Mutex)" );

#if FUSION_BUILD_MULTI

#if FUSION_BUILD_KERNEL

DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     FusionEntryInfo info;

     D_ASSERT( skirmish != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     
     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_init( %p, '%s' )\n", skirmish, name ? : "" );

     while (ioctl( world->fusion_fd, FUSION_SKIRMISH_NEW, &skirmish->multi.id )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "FUSION_SKIRMISH_NEW" );
          return DR_FUSION;
     }

     D_DEBUG_AT( Fusion_Skirmish, "  -> new skirmish %p [%d]\n", skirmish, skirmish->multi.id );
     
     info.type = FT_SKIRMISH;
     info.id   = skirmish->multi.id;

     direct_snputs( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     fusion_entry_add_permissions( world, FT_SKIRMISH, skirmish->multi.id, 0,
                                   FUSION_SKIRMISH_LOCK_COUNT,
                                   0 );

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_init2( FusionSkirmish    *skirmish,
                       const char        *name,
                       const FusionWorld *world,
                       bool               local )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_init2( %p, '%s', %s )\n", skirmish, name ? : "", local ? "local" : "shared" );

     if (!local)
          return fusion_skirmish_init( skirmish, name, world );


     skirmish->single = D_CALLOC( 1, sizeof(FusionSkirmishSingle) + strlen(name) + 1 );
     if (skirmish->single == 0)
          return DR_NOLOCALMEMORY;

     skirmish->single->name = (char*)(skirmish->single + 1);
     strcpy( skirmish->single->name, name );

     direct_util_recursive_pthread_mutex_init( &skirmish->single->lock );
     pthread_cond_init( &skirmish->single->cond, NULL );

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_prevail( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          if (pthread_mutex_lock( &skirmish->single->lock ))
               return errno2result( errno );

          skirmish->single->count++;

          return DR_OK;
     }

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_PREVAIL, &skirmish->multi.id)) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_PREVAIL");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          if (pthread_mutex_trylock( &skirmish->single->lock ))
               return errno2result( errno );

          skirmish->single->count++;

          return DR_OK;
     }

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_SWOOP, &skirmish->multi.id)) {
          switch (errno) {
               case EINTR:
                    continue;

               case EAGAIN:
                    return DR_BUSY;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_SWOOP");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish, int *lock_count )
{
     int data[2];

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          if (pthread_mutex_trylock( &skirmish->single->lock )) {
               *lock_count = 0;
               return errno2result( errno );
          }

          *lock_count = skirmish->single->count;

          pthread_mutex_unlock( &skirmish->single->lock );

          return DR_OK;
     }

     data[0] = skirmish->multi.id;
     data[1] = 0;

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_LOCK_COUNT, data)) {
           switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
           }

          D_PERROR ("FUSION_SKIRMISH_LOCK_COUNT");
          return DR_FUSION;
     }

     *lock_count = data[1];
     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          skirmish->single->count--;

          if (pthread_mutex_unlock( &skirmish->single->lock ))
               return errno2result( errno );

          return DR_OK;
     }

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_DISMISS, &skirmish->multi.id)) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_DISMISS");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_destroy (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_destroy( %p [%d] )\n", skirmish, skirmish->multi.id );
     
     if (skirmish->single) {
          int retval;

          pthread_cond_broadcast( &skirmish->single->cond );
          pthread_cond_destroy( &skirmish->single->cond );

          retval = pthread_mutex_destroy( &skirmish->single->lock );
          D_FREE( skirmish->single );

          return errno2result( retval );
     }

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_DESTROY, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;
                    
               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_DESTROY");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish, unsigned int timeout )
{
     FusionSkirmishWait wait;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          if (timeout) {
               struct timespec ts;
               struct timeval  tv;
               int             ret;

               gettimeofday( &tv, NULL );

               ts.tv_nsec = tv.tv_usec*1000 + (timeout%1000)*1000000;
               ts.tv_sec  = tv.tv_sec + timeout/1000 + ts.tv_nsec/1000000000;
               ts.tv_nsec = ts.tv_nsec % 1000000000;

               ret = pthread_cond_timedwait( &skirmish->single->cond, 
                                             &skirmish->single->lock, &ts );

               return (ret == ETIMEDOUT) ? DR_TIMEOUT : DR_OK;
          }

          return pthread_cond_wait( &skirmish->single->cond, &skirmish->single->lock );
     }

     wait.id         = skirmish->multi.id;
     wait.timeout    = timeout;
     wait.lock_count = 0;

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_WAIT, &wait)) {
          switch (errno) {
               case EINTR:
                    continue;

               case ETIMEDOUT:
                    return DR_TIMEOUT;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_WAIT");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          pthread_cond_broadcast( &skirmish->single->cond );

          return DR_OK;
     }

     while (ioctl (_fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_NOTIFY, &skirmish->multi.id)) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR ("Fusion/Lock: invalid skirmish\n");
                    return DR_DESTROYED;
          }

          D_PERROR ("FUSION_SKIRMISH_NOTIFY");
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_add_permissions( FusionSkirmish            *skirmish,
                                 FusionID                   fusion_id,
                                 FusionSkirmishPermissions  skirmish_permissions )
{
     FusionEntryPermissions permissions;

     if (skirmish->single)
          return DR_OK;

     permissions.type        = FT_SKIRMISH;
     permissions.id          = skirmish->multi.id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_PREVAIL)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_PREVAIL );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_SWOOP)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_SWOOP );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_DISMISS)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_DISMISS );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_LOCK_COUNT)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_LOCK_COUNT );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_WAIT)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_WAIT );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_NOTIFY)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_NOTIFY );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_DESTROY)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_DESTROY );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Reactor: FUSION_ENTRY_ADD_PERMISSIONS( id %d ) failed!\n", skirmish->multi.id );
               return DR_FAILURE;
          }
     }

     return DR_OK;
}

#else /* FUSION_BUILD_KERNEL */

#include <direct/clock.h>
#include <direct/list.h>
#include <direct/system.h>

typedef struct {
     DirectLink  link;
     
     pid_t       pid;
     bool        notified;
} WaitNode;


DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     D_ASSERT( skirmish != NULL );
     //D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     
     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_init( %p, '%s' )\n", 
                 skirmish, name ? : "" );
     
     skirmish->multi.id = ++world->shared->lock_ids;
     
     /* Set state to unlocked. */
     skirmish->multi.builtin.locked = 0;
     skirmish->multi.builtin.owner  = 0;

     skirmish->multi.builtin.waiting = NULL;
    
     skirmish->multi.builtin.requested = false;
     skirmish->multi.builtin.destroyed = false;
     
     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_prevail( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );
     
     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;
          
     asm( "" ::: "memory" );

     if (skirmish->multi.builtin.locked &&
         skirmish->multi.builtin.owner != direct_gettid())
     {
          int count = 0;
          
          while (skirmish->multi.builtin.locked) {
               /* Check whether owner exited without unlocking. */
               if (kill( skirmish->multi.builtin.owner, 0 ) < 0 && errno == ESRCH) { 
                    skirmish->multi.builtin.locked = 0;
                    skirmish->multi.builtin.requested = false; 
                    break;
               }

               skirmish->multi.builtin.requested = true;
               
               asm( "" ::: "memory" );
               
               if (++count > 1000) {
                    usleep( 10000 );
                    count = 0;
               }
               else {
                    direct_sched_yield();
               }
               
               if (skirmish->multi.builtin.destroyed)
                    return DR_DESTROYED;
          }
     }
     
     skirmish->multi.builtin.locked++;
     skirmish->multi.builtin.owner = direct_gettid();
     
     asm( "" ::: "memory" );

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );
     
     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;
          
     asm( "" ::: "memory" );
          
     if (skirmish->multi.builtin.locked &&
         skirmish->multi.builtin.owner != direct_gettid()) {
          /* Check whether owner exited without unlocking. */
          if (kill( skirmish->multi.builtin.owner, 0 ) < 0 && errno == ESRCH) { 
               skirmish->multi.builtin.locked = 0;
               skirmish->multi.builtin.requested = false;
          }
          else
               return DR_BUSY;
     }
          
     skirmish->multi.builtin.locked++;
     skirmish->multi.builtin.owner = direct_gettid();
     
     asm( "" ::: "memory" );

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish, int *lock_count )
{
     D_ASSERT( skirmish != NULL );
     
     if (skirmish->multi.builtin.destroyed) {
          *lock_count = 0;
          return DR_DESTROYED;
     }

     *lock_count = skirmish->multi.builtin.locked;
     
     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );
     
     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;
          
     asm( "" ::: "memory" );

     if (skirmish->multi.builtin.locked) {
          if (skirmish->multi.builtin.owner != direct_gettid()) {
               D_ERROR( "Fusion/Skirmish: "
                        "Tried to dismiss a skirmish not owned by current process!\n" );
               return DR_ACCESSDENIED;
          }
          
          if (--skirmish->multi.builtin.locked == 0) {
               skirmish->multi.builtin.owner = 0;

               if (skirmish->multi.builtin.requested) {
                    skirmish->multi.builtin.requested = false;
                    direct_sched_yield();
               }
          }
     }
     
     asm( "" ::: "memory" );
     
     return DR_OK;
}

DirectResult
fusion_skirmish_destroy (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_destroy( %p )\n", skirmish );
     
     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;
          
     if (skirmish->multi.builtin.waiting)
          fusion_skirmish_notify( skirmish );
          
     skirmish->multi.builtin.destroyed = true;

     return DR_OK;
}

#ifdef SIGRTMAX
# define SIGRESTART  SIGRTMAX
#else
# define SIGRESTART  SIGCONT
#endif

static void restart_handler( int s ) {}

DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish, unsigned int timeout )
{
     WaitNode         *node;
     long long         stop;
     struct sigaction  act, oldact;
     sigset_t          mask, set;
     DirectResult      ret = DR_OK;
     
     D_ASSERT( skirmish != NULL );
     
     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;
 
     /* Set timeout. */
     stop = direct_clock_get_micros() + timeout * 1000ll;
      
     /* Add ourself to the list of waiting processes. */    
     node = SHMALLOC( skirmish->multi.shared->main_pool, sizeof(WaitNode) );
     if (!node)
          return D_OOSHM();
     
     node->pid      = direct_gettid();
     node->notified = false;
     
     direct_list_append( &skirmish->multi.builtin.waiting, &node->link );
      
     /* Install a (fake) signal handler for SIGRESTART. */
     act.sa_handler = restart_handler;
     act.sa_flags   = SA_RESETHAND | SA_RESTART | SA_NOMASK;
     
     sigaction( SIGRESTART, &act, &oldact );
     
     /* Unblock SIGRESTART. */
     sigprocmask( SIG_SETMASK, NULL, &mask );
     sigdelset( &mask, SIGRESTART );

     fusion_skirmish_dismiss( skirmish );

     while (!node->notified) {
          if (timeout) {
               long long now = direct_clock_get_micros();

               if (now >= stop) {
                    /* Stop notifying us. */
                    node->notified = true;
                    ret = DR_TIMEOUT;
                    break;
               }
               
               sigprocmask( SIG_SETMASK, &mask, &set );
               usleep( stop - now );
               sigprocmask( SIG_SETMASK, &set, NULL );           
          }
          else {
               sigsuspend( &mask );
          }
     }

     /* Flush pending signals. */
     if (!sigpending( &set ) && sigismember( &set, SIGRESTART ) > 0)
          sigsuspend( &mask );
     
     if (fusion_skirmish_prevail( skirmish ))
          ret = DR_DESTROYED;
     
     direct_list_remove( &skirmish->multi.builtin.waiting, &node->link );

     SHFREE( skirmish->multi.shared->main_pool, node );
     
     sigaction( SIGRESTART, &oldact, NULL );

     return ret;
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     WaitNode *node, *temp;
     
     D_ASSERT( skirmish != NULL );

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     direct_list_foreach_safe (node, temp, skirmish->multi.builtin.waiting) {
          if (node->notified)
               continue;

          node->notified = true;

          if (kill( node->pid, SIGRESTART ) < 0) {
               if (errno == ESRCH) {
                    /* Remove dead process. */
                    direct_list_remove( &skirmish->multi.builtin.waiting, &node->link );
                    SHFREE( skirmish->multi.shared->main_pool, node );
               }
               else {
                    D_PERROR( "Fusion/Skirmish: Couldn't send notification signal!\n" );
               }
          }
     }

     return DR_OK;
}

#endif /* FUSION_BUILD_KERNEL */

#else  /* FUSION_BUILD_MULTI */

DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "fusion_skirmish_init( %p, '%s' )\n", skirmish, name ? : "" );

     skirmish->single = D_CALLOC( 1, sizeof(FusionSkirmishSingle) + strlen(name) + 1 );
     if (skirmish->single == 0)
          return DR_NOLOCALMEMORY;

     skirmish->single->name = (char*)(skirmish->single + 1);
     strcpy( skirmish->single->name, name );

     direct_util_recursive_pthread_mutex_init( &skirmish->single->lock );
     pthread_cond_init( &skirmish->single->cond, NULL );

     return DR_OK;
}

DirectResult
fusion_skirmish_init2( FusionSkirmish    *skirmish,
                       const char        *name,
                       const FusionWorld *world,
                       bool               local )
{
     /* By definition we are local. */
     return fusion_skirmish_init( skirmish, name, world );
}

DirectResult
fusion_skirmish_prevail (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     if (pthread_mutex_lock( &skirmish->single->lock ))
          return errno2result( errno );

     skirmish->single->count++;

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     if (pthread_mutex_trylock( &skirmish->single->lock ))
          return errno2result( errno );

     skirmish->single->count++;

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish, int *lock_count )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );
     D_ASSERT( lock_count != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     if (pthread_mutex_trylock( &skirmish->single->lock )) {
          *lock_count = 0;
          return errno2result( errno );
     }

     *lock_count = skirmish->single->count;

     pthread_mutex_unlock( &skirmish->single->lock );

     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss (FusionSkirmish *skirmish)
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     skirmish->single->count--;

     if (pthread_mutex_unlock( &skirmish->single->lock ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
fusion_skirmish_destroy (FusionSkirmish *skirmish)
{
     int retval;

     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     pthread_cond_broadcast( &skirmish->single->cond );
     pthread_cond_destroy( &skirmish->single->cond );

     retval = pthread_mutex_destroy( &skirmish->single->lock );
     D_FREE( skirmish->single );

     return errno2result( retval );
}


DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish, unsigned int timeout )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     if (timeout) {
          struct timespec ts;
          struct timeval  tv;
          int             ret;
          
          gettimeofday( &tv, NULL );
          
          ts.tv_nsec = tv.tv_usec*1000 + (timeout%1000)*1000000;
          ts.tv_sec  = tv.tv_sec + timeout/1000 + ts.tv_nsec/1000000000;
          ts.tv_nsec = ts.tv_nsec % 1000000000;
          
          ret = pthread_cond_timedwait( &skirmish->single->cond, 
                                        &skirmish->single->lock, &ts );
                                        
          return (ret == ETIMEDOUT) ? DR_TIMEOUT : DR_OK;
     }

     return pthread_cond_wait( &skirmish->single->cond, &skirmish->single->lock );
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( skirmish->single != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     pthread_cond_broadcast( &skirmish->single->cond );

     return DR_OK;
}

DirectResult
fusion_skirmish_add_permissions( FusionSkirmish            *skirmish,
                                 FusionID                   fusion_id,
                                 FusionSkirmishPermissions  skirmish_permissions )
{
     return DR_OK;
}

#endif


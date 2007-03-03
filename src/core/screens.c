/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <directfb.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/core_parts.h>

#include <core/screen.h>
#include <core/screens.h>
#include <core/screens_internal.h>

#include <core/layers.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>


typedef struct {
     int               num;
     CoreScreenShared *screens[MAX_SCREENS];
} CoreScreens;

static CoreScreens *core_screens = NULL;


DFB_CORE_PART( screens, 0, sizeof(CoreScreens) )


static int         num_screens          = 0;
static CoreScreen *screens[MAX_SCREENS] = { NULL };


static DFBResult
dfb_screens_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     int                  i;
     DFBResult            ret;
     FusionSHMPoolShared *pool;

     D_ASSERT( core_screens == NULL );
     D_ASSERT( data_shared  != NULL );

     core_screens = data_shared;

     pool = dfb_core_shmpool( core );

     /* Initialize all registered screens. */
     for (i=0; i<num_screens; i++) {
          char                  buf[24];
          CoreScreenShared     *shared;
          CoreScreen           *screen = screens[i];
          ScreenFuncs          *funcs  = screen->funcs;
          DFBScreenDescription  desc   = { 0 };

          /* Allocate shared data. */
          shared = SHCALLOC( pool, 1, sizeof(CoreScreenShared) );

          /* Assign ID (zero based index). */
          shared->screen_id = i;

          snprintf( buf, sizeof(buf), "Screen %d", i );

          /* Initialize the lock. */
          if (fusion_skirmish_init( &shared->lock, buf, dfb_core_world(core) )) {
               SHFREE( pool, shared );
               return DFB_FUSION;
          }

          /* Allocate driver's screen data. */
          if (funcs->ScreenDataSize) {
               int size = funcs->ScreenDataSize();

               if (size > 0) {
                    shared->screen_data = SHCALLOC( pool, 1, size );
                    if (!shared->screen_data) {
                         fusion_skirmish_destroy( &shared->lock );
                         SHFREE( pool, shared );
                         return D_OOSHM();
                    }
               }
          }

          /* Initialize the screen and get the screen description. */
          ret = funcs->InitScreen( screen,
                                   screen->device,
                                   screen->driver_data,
                                   shared->screen_data,
                                   &desc );
          if (ret) {
               D_ERROR("DirectFB/Core/screens: "
                        "Failed to initialize screen %d!\n", shared->screen_id);

               fusion_skirmish_destroy( &shared->lock );

               if (shared->screen_data)
                    SHFREE( pool, shared->screen_data );

               SHFREE( pool, shared );

               return ret;
          }

          D_ASSUME( desc.mixers  > 0 || !(desc.caps & DSCCAPS_MIXERS) );
          D_ASSUME( desc.mixers == 0 ||  (desc.caps & DSCCAPS_MIXERS) );

          D_ASSUME( desc.encoders  > 0 || !(desc.caps & DSCCAPS_ENCODERS) );
          D_ASSUME( desc.encoders == 0 ||  (desc.caps & DSCCAPS_ENCODERS) );

          D_ASSUME( desc.outputs  > 0 || !(desc.caps & DSCCAPS_OUTPUTS) );
          D_ASSUME( desc.outputs == 0 ||  (desc.caps & DSCCAPS_OUTPUTS) );

          D_ASSERT( desc.mixers >= 0 );
          D_ASSERT( desc.mixers <= 32 );
          D_ASSERT( desc.encoders >= 0 );
          D_ASSERT( desc.encoders <= 32 );
          D_ASSERT( desc.outputs >= 0 );
          D_ASSERT( desc.outputs <= 32 );

          /* Store description in shared memory. */
          shared->description = desc;

          /* Initialize mixers. */
          if (shared->description.mixers) {
               int i;

               D_ASSERT( funcs->InitMixer != NULL );
               D_ASSERT( funcs->SetMixerConfig != NULL );

               shared->mixers = SHCALLOC( pool, shared->description.mixers,
                                          sizeof(CoreScreenMixer) );
               for (i=0; i<shared->description.mixers; i++) {
                    funcs->InitMixer( screen,
                                      screen->driver_data,
                                      shared->screen_data, i,
                                      &shared->mixers[i].description,
                                      &shared->mixers[i].configuration );
                    funcs->SetMixerConfig( screen,
                                           screen->driver_data,
                                           shared->screen_data, i,
                                           &shared->mixers[i].configuration );
               }
          }

          /* Initialize encoders. */
          if (shared->description.encoders) {
               int i;

               D_ASSERT( funcs->InitEncoder != NULL );
               D_ASSERT( funcs->SetEncoderConfig != NULL );

               shared->encoders = SHCALLOC( pool, shared->description.encoders,
                                            sizeof(CoreScreenEncoder) );
               for (i=0; i<shared->description.encoders; i++) {
                    funcs->InitEncoder( screen,
                                        screen->driver_data,
                                        shared->screen_data, i,
                                        &shared->encoders[i].description,
                                        &shared->encoders[i].configuration );
                    funcs->SetEncoderConfig( screen,
                                             screen->driver_data,
                                             shared->screen_data, i,
                                             &shared->encoders[i].configuration );
               }
          }

          /* Initialize outputs. */
          if (shared->description.outputs) {
               int i;

               D_ASSERT( funcs->InitOutput != NULL );
               D_ASSERT( funcs->SetOutputConfig != NULL );

               shared->outputs = SHCALLOC( pool, shared->description.outputs,
                                           sizeof(CoreScreenOutput) );
               for (i=0; i<shared->description.outputs; i++) {
                    funcs->InitOutput( screen,
                                       screen->driver_data,
                                       shared->screen_data, i,
                                       &shared->outputs[i].description,
                                       &shared->outputs[i].configuration );
                    funcs->SetOutputConfig( screen,
                                            screen->driver_data,
                                            shared->screen_data, i,
                                            &shared->outputs[i].configuration );
               }
          }

          /* Make a copy for faster access. */
          screen->screen_data = shared->screen_data;

          /* Store pointer to shared data and core. */
          screen->shared = shared;
          screen->core   = core;

          /* Add the screen to the shared list. */
          core_screens->screens[ core_screens->num++ ] = shared;
     }

     return DFB_OK;
}

static DFBResult
dfb_screens_join( CoreDFB *core, void *data_local, void *data_shared )
{
     int i;

     D_ASSERT( core_screens == NULL );
     D_ASSERT( data_shared  != NULL );

     core_screens = data_shared;

     if (num_screens != core_screens->num) {
          D_ERROR("DirectFB/core/screens: Number of screens does not match!\n");
          return DFB_BUG;
     }

     for (i=0; i<num_screens; i++) {
          CoreScreen       *screen = screens[i];
          CoreScreenShared *shared = core_screens->screens[i];

          /* Make a copy for faster access. */
          screen->screen_data = shared->screen_data;

          /* Store pointer to shared data and core. */
          screen->shared = shared;
          screen->core   = core;
     }

     return DFB_OK;
}

static DFBResult
dfb_screens_shutdown( CoreDFB *core, bool emergency )
{
     int                  i;
     FusionSHMPoolShared *pool;

     D_ASSERT( core_screens != NULL );

     pool = dfb_core_shmpool( core );

     /* Begin with the most recently added screen. */
     for (i=num_screens-1; i>=0; i--) {
          CoreScreen       *screen = screens[i];
          CoreScreenShared *shared = screen->shared;

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &shared->lock );

          /* Free the driver's screen data. */
          if (shared->screen_data)
               SHFREE( pool, shared->screen_data );

          /* Free mixer data. */
          if (shared->mixers)
               SHFREE( pool, shared->mixers );

          /* Free encoder data. */
          if (shared->encoders)
               SHFREE( pool, shared->encoders );

          /* Free output data. */
          if (shared->outputs)
               SHFREE( pool, shared->outputs );

          /* Free the shared screen data. */
          SHFREE( pool, shared );

          /* Free the local screen data. */
          D_FREE( screen );
     }

     core_screens = NULL;
     num_screens  = 0;

     return DFB_OK;
}

static DFBResult
dfb_screens_leave( CoreDFB *core, bool emergency )
{
     int i;

     D_ASSERT( core_screens != NULL );

     /* Deinitialize all local stuff only. */
     for (i=0; i<num_screens; i++) {
          CoreScreen *screen = screens[i];

          /* Free local screen data. */
          D_FREE( screen );
     }

     core_screens = NULL;
     num_screens  = 0;

     return DFB_OK;
}

static DFBResult
dfb_screens_suspend( CoreDFB *core )
{
     int i;

     D_ASSERT( core_screens != NULL );

     D_DEBUG( "DirectFB/core/screens: suspending...\n" );

     for (i=num_screens-1; i>=0; i--)
          dfb_screen_suspend( screens[i] );

     D_DEBUG( "DirectFB/core/screens: suspended.\n" );

     return DFB_OK;
}

static DFBResult
dfb_screens_resume( CoreDFB *core )
{
     int i;

     D_ASSERT( core_screens != NULL );

     D_DEBUG( "DirectFB/core/screens: resuming...\n" );

     for (i=0; i<num_screens; i++)
          dfb_screen_resume( screens[i] );

     D_DEBUG( "DirectFB/core/screens: resumed.\n" );

     return DFB_OK;
}

CoreScreen *
dfb_screens_register( GraphicsDevice *device,
                      void           *driver_data,
                      ScreenFuncs    *funcs )
{
     CoreScreen *screen;

     D_ASSERT( funcs != NULL );

     if (num_screens == MAX_SCREENS) {
          D_ERROR( "DirectFB/Core/screen: "
                    "Maximum number of screens reached!\n" );
          return NULL;
     }

     /* allocate local data */
     screen = D_CALLOC( 1, sizeof(CoreScreen) );

     /* assign local pointers */
     screen->device      = device;
     screen->driver_data = driver_data;
     screen->funcs       = funcs;

     /* add it to the local list */
     screens[num_screens++] = screen;

     return screen;
}

typedef void (*AnyFunc)();

CoreScreen *
dfb_screens_hook_primary( GraphicsDevice  *device,
                          void            *driver_data,
                          ScreenFuncs     *funcs,
                          ScreenFuncs     *primary_funcs,
                          void           **primary_driver_data )
{
     int         i;
     int         entries;
     CoreScreen *primary = screens[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( funcs != NULL );

     /* copy content of original function table */
     if (primary_funcs)
          direct_memcpy( primary_funcs, primary->funcs, sizeof(ScreenFuncs) );

     /* copy pointer to original driver data */
     if (primary_driver_data)
          *primary_driver_data = primary->driver_data;

     /* replace all entries in the old table that aren't NULL in the new one */
     entries = sizeof(ScreenFuncs) / sizeof(void(*)());
     for (i=0; i<entries; i++) {
          AnyFunc *newfuncs = (AnyFunc*) funcs;
          AnyFunc *oldfuncs = (AnyFunc*) primary->funcs;

          if (newfuncs[i])
               oldfuncs[i] = newfuncs[i];
     }

     /* replace device and driver data pointer */
     primary->device      = device;
     primary->driver_data = driver_data;

     return primary;
}

CoreScreen *
dfb_screens_register_primary( GraphicsDevice *device,
                              void           *driver_data,
                              ScreenFuncs    *funcs )
{
     CoreScreen *primary = screens[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( funcs != NULL );
     D_ASSERT( num_screens > 0 );

     /* replace device, function table and driver data pointer */
     primary->device      = device;
     primary->funcs       = funcs;
     primary->driver_data = driver_data;

     return primary;
}

void
dfb_screens_enumerate( CoreScreenCallback  callback,
                       void               *ctx )
{
     int i;

     D_ASSERT( core_screens != NULL );
     D_ASSERT( callback != NULL );

     for (i=0; i<num_screens; i++) {
          if (callback( screens[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

CoreScreen *
dfb_screens_at( DFBScreenID screen_id )
{
     D_ASSERT( screen_id >= 0);
     D_ASSERT( screen_id < num_screens);

     return screens[screen_id];
}

CoreScreen *
dfb_screens_at_translated( DFBScreenID screen_id )
{
     CoreScreen *primary;
     
     D_ASSERT( screen_id >= 0);
     D_ASSERT( screen_id < num_screens);
     
     if (dfb_config->primary_layer > 0) {
          primary = dfb_layer_screen( dfb_layer_at_translated( DLID_PRIMARY ) );
          
          if (screen_id == DSCID_PRIMARY)
               return primary;
               
          if (screen_id == primary->shared->screen_id)
               return dfb_screens_at( DSCID_PRIMARY );
     }
     
     return dfb_screens_at( screen_id );
}

DFBScreenID
dfb_screen_id_translated( CoreScreen *screen )
{
     CoreScreenShared *shared;
     CoreScreen       *primary;
     
     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     
     shared = screen->shared;
     
     if (dfb_config->primary_layer > 0) {
          primary = dfb_layer_screen( dfb_layer_at_translated( DLID_PRIMARY ) );
          
          if (shared->screen_id == DSCID_PRIMARY)
               return primary->shared->screen_id;
               
          if (shared->screen_id == primary->shared->screen_id)
               return DSCID_PRIMARY;
     }
     
     return shared->screen_id;
}


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

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/core_parts.h>

#include <core/screen.h>
#include <core/screens.h>
#include <core/screens_internal.h>

#include <misc/mem.h>


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
     int       i;
     DFBResult ret;

     DFB_ASSERT( core_screens == NULL );
     DFB_ASSERT( data_shared  != NULL );

     core_screens = data_shared;

     /* Initialize all registered screens. */
     for (i=0; i<num_screens; i++) {
          CoreScreenShared  *shared;
          CoreScreen        *screen = screens[i];
          ScreenFuncs       *funcs  = screen->funcs;

          /* Allocate shared data. */
          shared = SHCALLOC( 1, sizeof(CoreScreenShared) );

          /* Assign ID (zero based index). */
          shared->screen_id = i;

          /* Initialize the lock. */
          if (fusion_skirmish_init( &shared->lock )) {
               SHFREE( shared );
               return DFB_FUSION;
          }

          /* Allocate driver's screen data. */
          if (funcs->ScreenDataSize) {
               int size = funcs->ScreenDataSize();

               if (size > 0) {
                    shared->screen_data = SHCALLOC( 1, size );
                    if (!shared->screen_data) {
                         fusion_skirmish_destroy( &shared->lock );
                         SHFREE( shared );
                         return DFB_NOSYSTEMMEMORY;
                    }
               }
          }

          /* Initialize the screen and query the screen description. */
          ret = funcs->InitScreen( screen,
                                   screen->device,
                                   screen->driver_data,
                                   shared->screen_data,
                                   &shared->description );
          if (ret) {
               ERRORMSG("DirectFB/Core/screens: "
                        "Failed to initialize screen %d!\n", shared->screen_id);

               fusion_skirmish_destroy( &shared->lock );

               if (shared->screen_data)
                    SHFREE( shared->screen_data );

               SHFREE( shared );

               return ret;
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

     DFB_ASSERT( core_screens == NULL );
     DFB_ASSERT( data_shared  != NULL );

     core_screens = data_shared;

     if (num_screens != core_screens->num) {
          ERRORMSG("DirectFB/core/screens: Number of screens does not match!\n");
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
     int i;

     DFB_ASSERT( core_screens != NULL );

     /* Begin with the most recently added screen. */
     for (i=num_screens-1; i>=0; i--) {
          CoreScreen       *screen = screens[i];
          CoreScreenShared *shared = screen->shared;

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &shared->lock );

          /* Free the driver's screen data. */
          if (shared->screen_data)
               SHFREE( shared->screen_data );

          /* Free the shared screen data. */
          SHFREE( shared );

          /* Free the local screen data. */
          DFBFREE( screen );
     }

     core_screens = NULL;
     num_screens  = 0;

     return DFB_OK;
}

static DFBResult
dfb_screens_leave( CoreDFB *core, bool emergency )
{
     int i;

     DFB_ASSERT( core_screens != NULL );

     /* Deinitialize all local stuff only. */
     for (i=0; i<num_screens; i++) {
          CoreScreen *screen = screens[i];

          /* Free local screen data. */
          DFBFREE( screen );
     }

     core_screens = NULL;
     num_screens  = 0;

     return DFB_OK;
}

static DFBResult
dfb_screens_suspend( CoreDFB *core )
{
     int i;

     DFB_ASSERT( core_screens != NULL );

     DEBUGMSG( "DirectFB/core/screens: suspending...\n" );

     for (i=num_screens-1; i>=0; i--)
          dfb_screen_suspend( screens[i] );

     DEBUGMSG( "DirectFB/core/screens: suspended.\n" );

     return DFB_OK;
}

static DFBResult
dfb_screens_resume( CoreDFB *core )
{
     int i;

     DFB_ASSERT( core_screens != NULL );

     DEBUGMSG( "DirectFB/core/screens: resuming...\n" );

     for (i=0; i<num_screens; i++)
          dfb_screen_resume( screens[i] );

     DEBUGMSG( "DirectFB/core/screens: resumed.\n" );

     return DFB_OK;
}

CoreScreen *
dfb_screens_register( GraphicsDevice *device,
                      void           *driver_data,
                      ScreenFuncs    *funcs )
{
     CoreScreen *screen;

     DFB_ASSERT( funcs != NULL );

     if (num_screens == MAX_SCREENS) {
          ERRORMSG( "DirectFB/Core/screen: "
                    "Maximum number of screens reached!\n" );
          return NULL;
     }

     /* allocate local data */
     screen = DFBCALLOC( 1, sizeof(CoreScreen) );

     /* assign local pointers */
     screen->device      = device;
     screen->driver_data = driver_data;
     screen->funcs       = funcs;

     /* add it to the local list */
     screens[num_screens++] = screen;

     return screen;
}

CoreScreen *
dfb_screens_register_primary( GraphicsDevice *device,
                              void           *driver_data,
                              ScreenFuncs    *funcs )
{
     CoreScreen *primary = screens[0];

     DFB_ASSERT( primary != NULL );
     DFB_ASSERT( funcs != NULL );

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

     DFB_ASSERT( core_screens != NULL );
     DFB_ASSERT( callback != NULL );

     for (i=0; i<num_screens; i++) {
          if (callback( screens[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

inline CoreScreen *
dfb_screens_at( DFBScreenID screen_id )
{
     DFB_ASSERT( screen_id >= 0);
     DFB_ASSERT( screen_id < num_screens);

     return screens[screen_id];
}


/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __SAWMAN_MANAGER_H__
#define __SAWMAN_MANAGER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <directfb_util.h>

#include <direct/list.h>
#include <direct/interface.h>

#include <fusion/call.h>
#include <fusion/lock.h>
#include <fusion/ref.h>
#include <fusion/vector.h>

#include <core/windows.h>
#include <core/layers_internal.h> /* FIXME */
#include <core/windows_internal.h> /* FIXME */

#include "sawman_types.h"


#define SAWMAN_MAX_UPDATE_REGIONS        8
#define SAWMAN_MAX_IMPLICIT_KEYGRABS    16


typedef enum {
     SWMPF_NONE     = 0x00000000,

     SWMPF_MASTER   = 0x00000001,
     SWMPF_MANAGER  = 0x00000002,

     SWMPF_EXITING  = 0x00000010,

     SWMPF_ALL      = 0x00000013
} SaWManProcessFlags;

typedef enum {
     SWMWF_NONE     = 0x00000000,

     SWMWF_INSERTED = 0x00000001,

     SWMWF_ALL      = 0x00000001
} SaWManWindowFlags;


struct __SaWMan_SaWManCallbacks {
     DirectResult (*Start)          ( void             *context,
                                      const char       *name,
                                      pid_t            *ret_pid );

     DirectResult (*Stop)           ( void             *context,
                                      pid_t             pid,
                                      FusionID          caller );


     DirectResult (*ProcessAdded)   ( void             *context,
                                      SaWManProcess    *process );

     DirectResult (*ProcessRemoved) ( void             *context,
                                      SaWManProcess    *process );



     DirectResult (*InputFilter)    ( void             *context,
                                      DFBInputEvent    *event );



     DirectResult (*WindowPreConfig)( void             *context,
                                      CoreWindow       *window );



     DirectResult (*WindowAdded)    ( void             *context,
                                      SaWManWindow     *window );

     DirectResult (*WindowRemoved)  ( void             *context,
                                      SaWManWindow     *window );


     DirectResult (*WindowConfig)   ( void             *context,
                                      SaWManWindow     *window );

     DirectResult (*WindowRestack)  ( void             *context,
                                      SaWManWindow     *window );

     DirectResult (*StackResized)   ( void             *context,
                                      DFBDimension     *size );

     DirectResult (*SwitchFocus)    ( void             *context,
                                      SaWManWindow     *window );


     /* To be extended... */
};

typedef enum {
     SWMCID_START,
     SWMCID_STOP,
     SWMCID_PROCESS_ADDED,
     SWMCID_PROCESS_REMOVED,
     SWMCID_INPUT_FILTER,
     SWMCID_WINDOW_PRECONFIG,
     SWMCID_WINDOW_ADDED,
     SWMCID_WINDOW_REMOVED,
     SWMCID_WINDOW_CONFIG,
     SWMCID_WINDOW_RESTACK,
     SWMCID_STACK_RESIZED,
     SWMCID_SWITCH_FOCUS
} SaWManCallID;

typedef enum {
     SWMSC_MIDDLE = (1 << DWSC_MIDDLE),
     SWMSC_UPPER  = (1 << DWSC_UPPER),
     SWMSC_LOWER  = (1 << DWSC_LOWER)
} SaWManStackingClasses;


struct __SaWMan_SaWMan {
     int                   magic;

     FusionSkirmish        lock;


     FusionVector          layout;

     DirectLink           *tiers;


     FusionSHMPoolShared  *shmpool;

     DirectLink           *processes;
     DirectLink           *windows;

     FusionCall            process_watch;

     SaWManScalingMode     scaling_mode;
     bool                  fast_mode;

     DFBInputDeviceButtonMask      buttons;
     DFBInputDeviceModifierMask    modifiers;
     DFBInputDeviceLockState       locks;

     SaWManWindow         *pointer_window;     /* window grabbing the pointer */
     SaWManWindow         *keyboard_window;    /* window grabbing the keyboard */
     SaWManWindow         *focused_window;     /* window having the focus */
     SaWManWindow         *entered_window;     /* window under the pointer */
     SaWManWindow         *unselkeys_window;   /* window grabbing unselected keys */

     DirectLink           *grabbed_keys;       /* List of currently grabbed keys. */

     struct {
          DFBInputDeviceKeySymbol      symbol;
          DFBInputDeviceKeyIdentifier  id;
          int                          code;
          SaWManWindow                *owner;
     } keys[SAWMAN_MAX_IMPLICIT_KEYGRABS];

     struct {
          bool                      present;

          FusionCall                call;

          SaWManCallbacks           callbacks;
          void                     *context;

          DFBInputEvent             event;
     }                     manager;
};

struct __SaWMan_SaWManTier {
     DirectLink              link;

     int                     magic;

     DFBDisplayLayerID       layer_id;
     SaWManStackingClasses   classes;

     CoreWindowStack        *stack;
     CoreLayerContext       *context;
     CoreLayerRegion        *region;

     FusionSkirmish          context_lock;

     DFBDisplayLayerConfig   config;
     DFBColor                key;

     DFBDimension            size;

     DFBUpdates              updates;
     DFBRegion               update_regions[SAWMAN_MAX_UPDATE_REGIONS];

     bool                    active;

     SaWManWindow           *single;
     int                     single_width;
     int                     single_height;
     DFBSurfacePixelFormat   single_format;
     DFBDisplayLayerOptions  single_options;
     DFBLocation             single_location;

     bool                    border_only;
     DFBDisplayLayerConfig   border_config;
};


struct __SaWMan_SaWManProcess {
     DirectLink             link;

     int                    magic;

     pid_t                  pid;
     FusionID               fusion_id;
     SaWManProcessFlags     flags;

     FusionRef              ref;
};

struct __SaWMan_SaWManWindow {
     DirectLink             link;

     int                    magic;

     SaWMan                *sawman;
     FusionSHMPoolShared   *shmpool;

     SaWManWindow          *parent;
     FusionVector           children;

     DFBRectangle           src;
     DFBRectangle           dst;

     SaWManProcess         *process;

     SaWManWindowFlags      flags;

     DFBWindowID            id;
     DFBWindowCapabilities  caps;
     CoreWindow            *window;

     CoreWindowStack       *stack;
     void                  *stack_data;

     int                    priority;           /* derived from stacking class */

     int                    border_normal;
     int                    border_fullscreen;

     struct {
          CoreWindowConfig          current;
          CoreWindowConfig          request;
          CoreWindowConfigFlags     flags;
     }                      config;
};

struct __SaWMan_SaWManGrabbedKey {
     DirectLink                    link;

     DFBInputDeviceKeySymbol       symbol;
     DFBInputDeviceModifierMask    modifiers;

     SaWManWindow                 *owner;
};


DirectResult sawman_initialize( SaWMan         *sawman,
                                FusionWorld    *world,
                                SaWManProcess **ret_process );

DirectResult sawman_join      ( SaWMan         *sawman,
                                FusionWorld    *world,
                                SaWManProcess **ret_process );

DirectResult sawman_shutdown  ( SaWMan         *sawman,
                                FusionWorld    *world );

DirectResult sawman_leave     ( SaWMan         *sawman,
                                FusionWorld    *world );


DirectResult sawman_call      ( SaWMan         *sawman,
                                SaWManCallID    call,
                                void           *ptr );


DirectResult sawman_register       ( SaWMan                *sawman,
                                     const SaWManCallbacks *callbacks,
                                     void                  *context );

DirectResult sawman_switch_focus   ( SaWMan                *sawman,
                                     SaWManWindow          *to );

DirectResult sawman_post_event     ( SaWMan                *sawman,
                                     SaWManWindow          *sawwin,
                                     DFBWindowEvent        *event );

DirectResult sawman_update_window  ( SaWMan                *sawman,
                                     SaWManWindow          *sawwin,
                                     const DFBRegion       *region,
                                     DFBSurfaceFlipFlags    flags,
                                     bool                   force_complete,
                                     bool                   force_invisible,
                                     bool                   scale_region );

DirectResult sawman_showing_window ( SaWMan                *sawman,
                                     SaWManWindow          *sawwin,
                                     bool                  *ret_showing );

DirectResult sawman_insert_window  ( SaWMan                *sawman,
                                     SaWManWindow          *sawwin,
                                     SaWManWindow          *relative,
                                     bool                   top );

DirectResult sawman_remove_window  ( SaWMan                *sawman,
                                     SaWManWindow          *sawwin );

DirectResult sawman_withdraw_window( SaWMan                *sawman,
                                     SaWManWindow          *sawwin );

DirectResult sawman_update_geometry( SaWManWindow          *sawwin );

static inline DirectResult
sawman_lock( SaWMan *sawman )
{
     D_MAGIC_ASSERT( sawman, SaWMan );

     return fusion_skirmish_prevail( &sawman->lock );
}

static inline DirectResult
sawman_unlock( SaWMan *sawman )
{
     D_MAGIC_ASSERT( sawman, SaWMan );

     return fusion_skirmish_dismiss( &sawman->lock );
}

static inline int
sawman_window_priority( const SaWManWindow *sawwin )
{
     const CoreWindow *window;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     switch (window->config.stacking) {
          case DWSC_UPPER:
               return 2;

          case DWSC_MIDDLE:
               return 1;

          case DWSC_LOWER:
               return 0;

          default:
               D_BUG( "unknown stacking class" );
               break;
     }

     return 0;
}

static inline int
sawman_window_index( const SaWMan       *sawman,
                     const SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_ASSERT( fusion_vector_contains( &sawman->layout, sawwin ) );

     return fusion_vector_index_of( &sawman->layout, sawwin );
}

static inline SaWManTier *
sawman_tier_by_class( SaWMan                 *sawman,
                      DFBWindowStackingClass  stacking )
{
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( (stacking & ~3) == 0 );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->classes & (1 << stacking))
               break;
     }

     D_ASSERT( tier != NULL );

     return tier;
}

static inline bool
sawman_tier_by_stack( SaWMan           *sawman,
                      CoreWindowStack  *stack,
                      SaWManTier      **ret_tier )
{
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );
     D_ASSERT( ret_tier != NULL );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->stack == stack) {
               *ret_tier = tier;
               return true;
          }
     }

     return false;
}

static inline bool
sawman_tier_by_layer( SaWMan             *sawman,
                      DFBDisplayLayerID   layer_id,
                      SaWManTier        **ret_tier )
{
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( ret_tier != NULL );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->layer_id == layer_id) {
               *ret_tier = tier;
               return true;
          }
     }

     return false;
}

int sawman_window_border( const SaWManWindow *sawwin );

#define SAWMAN_VISIBLE_WINDOW(w)     ((!((w)->caps & (DWCAPS_INPUTONLY)) ||\
                                       !((w)->caps & (DWCAPS_NODECORATION))) && \
                                      (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED((w)))

#define SAWMAN_TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                                      (w)->config.options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING) ||\
                                      (w)->config.dst_geometry.mode != DWGM_DEFAULT)

#ifdef __cplusplus
}
#endif

#endif


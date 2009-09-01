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
#include <directfb_version.h>

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

/**********************************************************************************************************************/

/* compatibility towards 1.2.x and 1.4.x DirectFB branches */
#if DIRECTFB_MAJOR_VERSION==1
#if DIRECTFB_MINOR_VERSION<=2
#define OLD_COREWINDOWS_STRUCTURE
#endif
#endif

#ifdef OLD_COREWINDOWS_STRUCTURE
#define D_MAGIC_COREWINDOW_ASSERT(window)
#define WINDOW_TOPLEVEL(window) (0)
#define DWOP_FOLLOW_BOUNDS (0x00400000)
#define WINDOW_SUBWINDOWS_COUNT(window) (0)
#else
#define D_MAGIC_COREWINDOW_ASSERT(window) D_MAGIC_ASSERT( (window), CoreWindow )
#define WINDOW_TOPLEVEL(window) ((window)->toplevel)
#define WINDOW_SUBWINDOWS_COUNT(window) ((window)->subwindows.count)
#endif

#define SAWMANWINDOWCONFIG_COPY( a, b )  {  \
     (a)->bounds       = (b)->bounds;       \
     (a)->opacity      = (b)->opacity;      \
     (a)->stacking     = (b)->stacking;     \
     (a)->options      = (b)->options;      \
     (a)->events       = (b)->events;       \
     (a)->color        = (b)->color;        \
     (a)->color_key    = (b)->color_key;    \
     (a)->opaque       = (b)->opaque;       \
     (a)->association  = (b)->association;  \
     (a)->src_geometry = (b)->src_geometry; \
     (a)->dst_geometry = (b)->dst_geometry; }

#define SAWMANWINDOWCONFIG_COPY_IF( a, b, f )  {  \
     if (f & CWCF_POSITION)   { (a)->bounds.x     = (b)->bounds.x;     \
                                (a)->bounds.y     = (b)->bounds.y; }   \
     if (f & CWCF_SIZE)       { (a)->bounds.w     = (b)->bounds.w;     \
                                (a)->bounds.h     = (b)->bounds.h; }   \
     if (f & CWCF_OPACITY)      (a)->opacity      = (b)->opacity;      \
     if (f & CWCF_STACKING)     (a)->stacking     = (b)->stacking;     \
     if (f & CWCF_OPTIONS)      (a)->options      = (b)->options;      \
     if (f & CWCF_EVENTS)       (a)->events       = (b)->events;       \
     if (f & CWCF_COLOR)        (a)->color        = (b)->color;        \
     if (f & CWCF_COLOR_KEY)    (a)->color_key    = (b)->color_key;    \
     if (f & CWCF_OPAQUE)       (a)->opaque       = (b)->opaque;       \
     if (f & CWCF_ASSOCIATION)  (a)->association  = (b)->association;  \
     if (f & CWCF_SRC_GEOMETRY) (a)->src_geometry = (b)->src_geometry; \
     if (f & CWCF_DST_GEOMETRY) (a)->dst_geometry = (b)->dst_geometry; }

/**********************************************************************************************************************/

#ifdef OLD_COREWINDOWS_STRUCTURE
#define SAWMAN_VISIBLE_WINDOW(w)     ((!((w)->caps & DWCAPS_INPUTONLY) || sawman_window_border((w)->window_data)) && \
                                      (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED(w))
#else
#define SAWMAN_VISIBLE_WINDOW(w)     ((!((w)->caps & DWCAPS_INPUTONLY) || sawman_window_border((w)->window_data)) && \
                                      (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED(w) && \
                                      (!(w)->toplevel || (w)->toplevel->config.opacity > 0))
#endif

#define SAWMAN_TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                                      (w)->config.options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING) ||\
                                      (w)->config.dst_geometry.mode != DWGM_DEFAULT ||\
                                      ((w)->caps & (DWCAPS_INPUTONLY)))

/**********************************************************************************************************************/

#define SAWMAN_MAX_UPDATE_REGIONS        8
#define SAWMAN_MAX_VISIBLE_REGIONS      10
#define SAWMAN_MAX_IMPLICIT_KEYGRABS    16

/**********************************************************************************************************************/

typedef enum {
     SWMCID_START,
     SWMCID_STOP,
     SWMCID_PROCESS_ADDED,
     SWMCID_PROCESS_REMOVED,
     SWMCID_INPUT_FILTER,
     SWMCID_WINDOW_PRECONFIG,
     SWMCID_WINDOW_ADDED,
     SWMCID_WINDOW_REMOVED,
     SWMCID_WINDOW_RECONFIG,
     SWMCID_WINDOW_RESTACK,
     SWMCID_STACK_RESIZED,
     SWMCID_SWITCH_FOCUS,
     SWMCID_LAYER_RECONFIG,
     SWMCID_APPLICATION_ID_CHANGED,
} SaWManCallID;

typedef enum {
     SWMSC_MIDDLE = (1 << DWSC_MIDDLE),
     SWMSC_UPPER  = (1 << DWSC_UPPER),
     SWMSC_LOWER  = (1 << DWSC_LOWER)
} SaWManStackingClasses;

typedef enum {
     SWMUF_NONE               = 0x0000,

     SWMUF_FORCE_COMPLETE     = 0x0001,
     SWMUF_FORCE_INVISIBLE    = 0x0002,
     SWMUF_SCALE_REGION       = 0x0004,
     SWMUF_UPDATE_BORDER      = 0x0008,

     SWMUF_ALL                = 0x000F
} SaWManUpdateFlags;

/**********************************************************************************************************************/

struct __SaWMan_SaWMan {
     int                   magic;

     FusionSkirmish        lock;

     FusionVector          layout;

     DirectLink           *tiers;

     CoreWindowStack      *stack;


     FusionSHMPoolShared  *shmpool;

     DirectLink           *processes;
     DirectLink           *windows;

     FusionCall            process_watch;

     SaWManScalingMode     scaling_mode;

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

     DFBWindowID           window_ids;
     
          
     /* reserved area for callback stuctures */
     struct {
          SaWManWindowInfo     info;
          SaWManWindowReconfig reconfig;
          DFBDimension         size;
          SaWManWindowHandle   handle;
          SaWManWindowHandle   relative;
          SaWManLayerReconfig  layer_reconfig;
     } callback;

     struct {
          DFBUpdates             visible;
          DFBRegion              visible_regions[SAWMAN_MAX_VISIBLE_REGIONS];
     } bg;
};

struct __SaWMan_SaWManTier {
     DirectLink              link;

     int                     magic;

     DFBDisplayLayerID       layer_id;
     SaWManStackingClasses   classes;

     CoreWindowStack        *stack;
     CoreLayerContext       *context;
     CoreLayerRegion        *region;

     DFBDisplayLayerConfig   config;
     DFBColorKey             key;

     DFBDimension            size;

     DFBUpdates              updates;
     DFBRegion               update_regions[SAWMAN_MAX_UPDATE_REGIONS];

     bool                    active;

     bool                    single_mode;
     SaWManWindow           *single_window;
     int                     single_width;
     int                     single_height;
     DFBRectangle            single_src;
     DFBRectangle            single_dst;
     DFBSurfacePixelFormat   single_format;
     DFBDisplayLayerOptions  single_options;
     DFBColorKey             single_key;

     bool                    border_only;
     DFBDisplayLayerConfig   border_config;

     CoreSurface            *cursor_bs;          /* backing store for region under cursor */
     bool                    cursor_bs_valid;
     DFBRegion               cursor_region;
     bool                    cursor_drawn;

     int                     cursor_dx;
     int                     cursor_dy;
};

# if 0
struct __SaWMan_SaWManProcess {
     DirectLink             link;

     int                    magic;

     pid_t                  pid;
     FusionID               fusion_id;
     SaWManProcessFlags     flags;

     FusionRef              ref;
};

#endif

struct __SaWMan_SaWManWindow {
     DirectLink             link;

     int                    magic;

     SaWMan                *sawman;
     FusionSHMPoolShared   *shmpool;

     SaWManWindow          *parent;
     CoreWindow            *parent_window;
     FusionVector           children;

     DFBRectangle           bounds;

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

     DFBUpdates             visible;
     DFBRegion              visible_regions[SAWMAN_MAX_VISIBLE_REGIONS];
};

struct __SaWMan_SaWManGrabbedKey {
     DirectLink                    link;

     DFBInputDeviceKeySymbol       symbol;
     DFBInputDeviceModifierMask    modifiers;

     SaWManWindow                 *owner;
};

/**********************************************************************************************************************/

DirectResult sawman_initialize( SaWMan                *sawman,
                                FusionWorld           *world,
                                SaWManProcess        **ret_process );

DirectResult sawman_join      ( SaWMan                *sawman,
                                FusionWorld           *world,
                                SaWManProcess        **ret_process );

DirectResult sawman_shutdown  ( SaWMan                *sawman,
                                FusionWorld           *world );

DirectResult sawman_leave     ( SaWMan                *sawman,
                                FusionWorld           *world );


DirectResult sawman_register  ( SaWMan                *sawman,
                                const SaWManCallbacks *callbacks,
                                void                  *context );


DirectResult sawman_call      ( SaWMan                *sawman,
                                SaWManCallID           call,
                                void                  *ptr );


/**********************************************************************************************************************/
                     
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
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

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
sawman_window_index( SaWMan       *sawman,
                     SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->layer_id == layer_id) {
               *ret_tier = tier;
               return true;
          }
     }

     return false;
}

#ifdef __cplusplus
}
#endif

#endif


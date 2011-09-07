/*
   (c) Copyright 2006-2010  directfb.org

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

#define VERSION_CODE( M, m, r )  (((M) * 1000) + ((m) * 100) + (r))
#define DIRECTFB_VERSION_CODE    VERSION_CODE( DIRECTFB_MAJOR_VERSION,\
                                               DIRECTFB_MINOR_VERSION,\
                                               DIRECTFB_MICRO_VERSION )

/* compatibility towards 1.2.x and 1.4.x DirectFB branches */
#if DIRECTFB_VERSION_CODE < VERSION_CODE(1,4,0)
#define OLD_COREWINDOWS_STRUCTURE
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

#define SAWMANWINDOWCONFIG_COPY( a, b )  do {  \
     (a)->bounds       = (b)->bounds;       \
     (a)->opacity      = (b)->opacity;      \
     (a)->stacking     = (b)->stacking;     \
     (a)->options      = (b)->options;      \
     (a)->events       = (b)->events;       \
     (a)->color        = (b)->color;        \
     (a)->color_key    = (b)->color_key;    \
     (a)->opaque       = (b)->opaque;       \
     (a)->association  = (b)->association;  \
     (a)->cursor_flags = (b)->cursor_flags; \
     (a)->cursor_resolution = (b)->cursor_resolution; \
     (a)->src_geometry = (b)->src_geometry; \
     (a)->dst_geometry = (b)->dst_geometry; \
     (a)->z            = (b)->z; } while (0)

#define SAWMANWINDOWCONFIG_COPY_IF( a, b, f )  do {  \
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
     if (f & CWCF_CURSOR_FLAGS) (a)->cursor_flags = (b)->cursor_flags; \
     if (f & CWCF_CURSOR_RESOLUTION) (a)->cursor_resolution = (b)->cursor_resolution; \
     if (f & CWCF_SRC_GEOMETRY) (a)->src_geometry = (b)->src_geometry; \
     if (f & CWCF_DST_GEOMETRY) (a)->dst_geometry = (b)->dst_geometry; \
     if (f & CWCF_STEREO_DEPTH) (a)->z            = (b)->z; } while (0)

/**********************************************************************************************************************/

#ifdef OLD_COREWINDOWS_STRUCTURE
#define SAWMAN_VISIBLE_WINDOW(w)    ( (     !(((w)->caps & DWCAPS_INPUTONLY) || ((w)->config.options & DWOP_INPUTONLY)) \
                                         || sawman_window_border((w)->window_data)) \
                                      && (w)->config.opacity > 0                    \
                                      && !DFB_WINDOW_DESTROYED(w) )
#else
#define SAWMAN_VISIBLE_WINDOW(w)    ( (     !(((w)->caps & DWCAPS_INPUTONLY) || ((w)->config.options & DWOP_INPUTONLY)) \
                                         || sawman_window_border((w)->window_data)) \
                                      && (w)->config.opacity > 0                    \
                                      && !DFB_WINDOW_DESTROYED(w)                   \
                                      && (!(w)->toplevel || (w)->toplevel->config.opacity > 0) )
#endif

#define SAWMAN_TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                                      (w)->config.options & (DWOP_INPUTONLY | DWOP_ALPHACHANNEL | DWOP_COLORKEYING) ||\
                                      (w)->config.dst_geometry.mode != DWGM_DEFAULT ||\
                                      ((w)->caps & (DWCAPS_INPUTONLY)))

/**********************************************************************************************************************/

#define SAWMAN_MAX_UPDATE_REGIONS        8   // dirty region on a tier
#define SAWMAN_MAX_UPDATING_REGIONS      8   // updated region on tier to be scheduled for display
#define SAWMAN_MAX_UPDATED_REGIONS       8   // updated region on tier scheduled for display
#define SAWMAN_MAX_VISIBLE_REGIONS      10   // for the visible window region detection
#define SAWMAN_MAX_UPDATES_REGIONS      10   // for the DSFLIP_QUEUE / DSFLIP_FLUSH implementation
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
     SWMUF_RIGHT_EYE          = 0x0010,

     SWMUF_ALL                = 0x001F
} SaWManUpdateFlags;

/**********************************************************************************************************************/

typedef enum {
     SWMLC_TIER_UPDATE        = 1,
     SWMLC_WINDOW_BLIT        = 2,
} SaWManListenerCall;

typedef struct {
     SaWManListenerCall       call;

     DFBSurfaceStereoEye      stereo_eye;

     DFBDisplayLayerID        layer_id;
     DFBRegion                updates[SAWMAN_MAX_UPDATE_REGIONS];
     unsigned int             num_updates;

     DFBWindowID              window_id;
     u32                      resource_id;
     DFBRectangle             src;
     DFBRectangle             dst;
} SaWManListenerCallData;

/**********************************************************************************************************************/

// FIXME: temporary solution
typedef struct {
     FusionCall                call;

     SaWManCallbacks           callbacks;
     void                     *context;
} SaWManRegisterManagerData;


typedef struct {
     bool                      present;
     FusionCall                call_from;

     FusionCall                call;

     SaWManCallbacks           callbacks;
     void                     *context;

     DFBInputEvent             event;
} SaWManManager;

typedef struct {
     SaWManWindowHandle   handle;
     SaWManWindowHandle   relative;
     SaWManWindowRelation relation;
} SaWManRestackArgs;

struct __SaWMan_SaWMan {
     int                   magic;

     FusionSkirmish       *lock;

     FusionVector          layout;

     DirectLink           *tiers;

     CoreWindowStack      *stack;


     FusionSHMPoolShared  *shmpool;

     DirectLink           *processes;
     DirectLink           *windows;

     FusionCall            process_watch;

     SaWManScalingMode     scaling_mode;

     DFBDimension          resolution;

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

     SaWManManager         manager;

     DFBWindowID           window_ids;


     /* reserved area for callback stuctures */
     struct {
          SaWManWindowInfo     info;
          SaWManWindowReconfig reconfig;
          DFBDimension         size;
          SaWManLayerReconfig  layer_reconfig;
     } callback;

     struct {
          DFBUpdates             visible;
          DFBRegion              visible_regions[SAWMAN_MAX_VISIBLE_REGIONS];
     } bg;

     struct {
          CoreLayer           *layer;
          CoreLayerContext    *context;
          CoreLayerRegion     *region;


          /* Temporary motion delta until flushed by last event (without DIEF_FOLLOW) */
          int                  dx; // Device coordinates
          int                  dy;

          SaWManWindow        *confined;
     } cursor;

     FusionCall                call;

     FusionReactor            *reactor;
};

/*
 * Notifications
 */
typedef enum {
     SAWMAN_TIER_UPDATE
} SaWManTierChannels;

/*
 * Tier update notification
 */
typedef struct {
     DFBRegion              regions[SAWMAN_MAX_UPDATE_REGIONS];
     unsigned int           num_regions;
     SaWManStackingClasses  classes;
} SaWManTierUpdate;

/*
 * Per left/right tier update info
 */
typedef struct {
     DFBUpdates              updates;
     DFBRegion               update_regions[SAWMAN_MAX_UPDATE_REGIONS];

     DFBUpdates              updating;
     DFBRegion               updating_regions[SAWMAN_MAX_UPDATED_REGIONS];

     DFBUpdates              updated;
     DFBRegion               updated_regions[SAWMAN_MAX_UPDATED_REGIONS];
} SaWManTierLR;

/*
 * Per HW layer
 */
struct __SaWMan_SaWManTier {
     DirectLink              link;

     int                     magic;

     SaWMan                 *sawman;

     DFBDisplayLayerID       layer_id;
     SaWManStackingClasses   classes;

     CoreWindowStack        *stack;
     CoreLayerContext       *context;
     CoreLayerRegion        *region;
     CoreSurface            *surface;
     Reaction                surface_reaction;

     DFBDisplayLayerConfig   config;
     DFBColorKey             key;

     DFBDimension            size;

     bool                    update_once;

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

     FusionReactor          *reactor;

     SaWManTierLR            left;
     SaWManTierLR            right;
};

/*
 * Per left/right window update info
 */
typedef struct {
     DFBUpdates             visible;
     DFBRegion              visible_regions[SAWMAN_MAX_VISIBLE_REGIONS];

     DFBUpdates             updates;
     DFBRegion              updates_regions[SAWMAN_MAX_UPDATES_REGIONS];
} SaWManWindowLR;

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

     long long              update_ms;

     SaWManWindowLR         left;
     SaWManWindowLR         right;
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

typedef struct {
     int                           magic;

     bool                          active;

     SaWMan                       *sawman;

     CoreWindowStack              *stack;
} StackData;

DirectResult sawman_register  ( SaWMan                 *sawman,
                                const SaWManCallbacks  *callbacks,
                                void                   *context,
                                SaWManManager         **ret_manager );

DirectResult sawman_unregister( SaWMan                *sawman );

DirectResult sawman_call      ( SaWMan                *sawman,
                                SaWManCallID           call,
                                void                  *ptr,
                                unsigned int           len,
                                bool                   copy_back );

DirectResult sawman_post_init ( SaWMan         *sawman,
                                FusionWorld    *world );

DirectResult sawman_register_process( SaWMan                *sawman,
                                      SaWManProcessFlags     flags,
                                      pid_t                  pid,
                                      FusionID               fusion_id,
                                      FusionWorld           *world,
                                      SaWManProcess        **ret_process );

/**********************************************************************************************************************/

void sawman_dispatch_blit( SaWMan             *sawman,
                           SaWManWindow       *sawwin,
                           bool                right_eye,
                           const DFBRectangle *src,
                           const DFBRectangle *dst,
                           const DFBRegion    *clip );
                     
/**********************************************************************************************************************/

static inline DirectResult
sawman_lock( SaWMan *sawman )
{
     D_MAGIC_ASSERT( sawman, SaWMan );

     return fusion_skirmish_prevail( sawman->lock );
}

static inline DirectResult
sawman_unlock( SaWMan *sawman )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     return fusion_skirmish_dismiss( sawman->lock );
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
               D_BUG( "unknown stacking class %d", window->config.stacking );
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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     direct_list_foreach (tier, sawman->tiers) {
          D_MAGIC_ASSERT( tier, SaWManTier );

          if (tier->layer_id == layer_id) {
               *ret_tier = tier;
               return true;
          }
     }

     return false;
}


extern SaWMan *m_sawman;

#ifdef __cplusplus
}
#endif

#endif


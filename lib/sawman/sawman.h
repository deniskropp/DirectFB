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

#ifndef __SAWMAN_H__
#define __SAWMAN_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <directfb.h>
#include <directfb_windows.h>

#include <direct/list.h>
#include <fusion/ref.h>
#include <fusion/vector.h>

#include "sawman_types.h"

/*
 * Main SaWMan interface for all executables including start/stop of other applications.
 */
DECLARE_INTERFACE( ISaWMan )

/*
 * Manager interface to be created only once for the application manager executable.
 */
DECLARE_INTERFACE( ISaWManManager )


DirectResult SaWManInit  ( int       *argc,
                           char    ***argv );

DirectResult SaWManCreate( ISaWMan  **ret_sawman );

/*
 * Public SaWMan types
 */

typedef enum {
     SWMWF_NONE     = 0x00000000,

     SWMWF_INSERTED = 0x00000001,

     SWMWF_FOCUSED  = 0x00000002, /* only used for GetWindowInfo */
     SWMWF_ENTERED  = 0x00000004, /* only used for GetWindowInfo */

     SWMWF_UPDATING = 0x00000010,

     SWMWF_ALL      = 0x00000017
} SaWManWindowFlags;


typedef enum {
     SWMSM_SMOOTH_SW,    /* Smooth scaling algorithm in software */
     SWMSM_STANDARD      /* As provided by hardware, otherwise software (nearest neighbor) */
} SaWManScalingMode;


typedef enum {
     SWMPF_NONE     = 0x00000000,

     SWMPF_MASTER   = 0x00000001,
     SWMPF_MANAGER  = 0x00000002,

     SWMPF_EXITING  = 0x00000010,

     SWMPF_ALL      = 0x00000013
} SaWManProcessFlags;


/* defines stacking relation.
 * If 2 valid window handles are passed, defines relative order of 1st towards 2nd
 * If only 1 handle is valid (2nd is WINDOW_NONE), defines location in complete stack */
typedef enum {
     SWMWR_TOP,
     SWMWR_BOTTOM
} SaWManWindowRelation;


typedef struct {
     DirectLink             link;

     int                    magic;

     pid_t                  pid;
     FusionID               fusion_id;
     SaWManProcessFlags     flags;

     FusionRef              ref;

     FusionCall             call;
} SaWManProcess;


typedef unsigned long SaWManWindowHandle;
#define SAWMAN_WINDOW_NONE    ((SaWManWindowHandle) 0)


typedef enum {
     SWMCF_NONE          = DWCONF_NONE,

     SWMCF_POSITION      = DWCONF_POSITION,
     SWMCF_SIZE          = DWCONF_SIZE,
     SWMCF_OPACITY       = DWCONF_OPACITY,
     SWMCF_STACKING      = DWCONF_STACKING,

     SWMCF_OPTIONS       = DWCONF_OPTIONS,
     SWMCF_EVENTS        = DWCONF_EVENTS,

     SWMCF_COLOR_KEY     = DWCONF_COLOR_KEY,
     SWMCF_OPAQUE        = DWCONF_OPAQUE,
     SWMCF_COLOR         = DWCONF_COLOR,
     SWMCF_STEREO_DEPTH  = DWCONF_STEREO_DEPTH,

     SWMCF_KEY_SELECTION = DWCONF_KEY_SELECTION,

     SWMCF_ASSOCIATION   = DWCONF_ASSOCIATION,
     SWMCF_CURSOR_FLAGS  = DWCONF_CURSOR_FLAGS,
     SWMCF_CURSOR_RESOLUTION = DWCONF_CURSOR_RESOLUTION,

     SWMCF_SRC_GEOMETRY  = DWCONF_SRC_GEOMETRY,
     SWMCF_DST_GEOMETRY  = DWCONF_DST_GEOMETRY,

     SWMCF_ALL           = DWCONF_ALL
} SaWManWindowConfigFlags;

typedef struct {
     DFBRectangle             bounds;         /* position and size */
     int                      opacity;        /* global alpha factor */
     DFBWindowStackingClass   stacking;       /* level boundaries */
     DFBWindowOptions         options;        /* flags for appearance/behaviour */

     DFBWindowEventType       events;         /* mask of enabled events */
     DFBColor                 color;          /* constant color (no surface needed) */
     u32                      color_key;      /* transparent pixel */
     DFBRegion                opaque;         /* region of the window forced to be opaque */

     DFBWindowKeySelection    key_selection;  /* how to filter keys in focus */
     DFBInputDeviceKeySymbol *keys;           /* list of keys for DWKS_LIST */
     unsigned int             num_keys;       /* number of entries in key array */

     DFBWindowID              association;    /* ID of window which this is associated to */

     DFBWindowGeometry        src_geometry;   /* advanced source geometry */
     DFBWindowGeometry        dst_geometry;   /* advanced destination geometry */

     DFBWindowCursorFlags     cursor_flags;
     DFBDimension             cursor_resolution;

     int                      z;              /* stereoscopic offset used to establish perceived depth */
} SaWManWindowConfig;

typedef struct {
     SaWManWindowHandle       handle;

     DFBWindowCapabilities    caps;
     SaWManWindowConfig       config;

     unsigned long            resource_id;
     unsigned long            application_id;
     DFBWindowID              win_id;

     SaWManWindowFlags        flags;
} SaWManWindowInfo;

typedef struct {
     SaWManWindowHandle       handle;

     DFBWindowCapabilities    caps;         /* window capabilities, RO */

     SaWManWindowConfigFlags  flags;        /* applicability of below values */
     SaWManWindowConfig       current;
     SaWManWindowConfig       request;
} SaWManWindowReconfig;

typedef struct {
     DFBDisplayLayerID        layer_id;

     SaWManWindowHandle       single;
     DFBDisplayLayerConfig    config;
} SaWManLayerReconfig;

typedef enum {
     SWMCFR_APPLICATION,
     SWMCFR_MANAGER,
     SWMCFR_IMPLICIT
} SaWManChangeFocusReason;

/*
 * Callbacks, to be used together with the SaWMan Manager interface
 */

typedef struct {
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



     DirectResult (*WindowPreConfig)( void               *context,
                                      SaWManWindowConfig *config );



     DirectResult (*WindowAdded)    ( void               *context,
                                      SaWManWindowInfo   *info );

     DirectResult (*WindowRemoved)  ( void               *context,
                                      SaWManWindowInfo   *info );



     DirectResult (*WindowReconfig) ( void                 *context,
                                      SaWManWindowReconfig *reconfig );

     DirectResult (*WindowRestack)  ( void                 *context,
                                      SaWManWindowHandle    handle,
                                      SaWManWindowHandle    relative,
                                      SaWManWindowRelation  relation );

     DirectResult (*SwitchFocus)    ( void                 *context,
                                      SaWManWindowHandle    handle );



     DirectResult (*StackResized)   ( void               *context,
                                      const DFBDimension *size );

     DirectResult (*LayerReconfig)  ( void                *context,
                                      SaWManLayerReconfig *reconfig );

     DirectResult (*ApplicationIDChanged) ( void             *context,
                                            SaWManWindowInfo *info );


     /*
      * ChangeFocus, if present, overrides SwitchFocus, adding the reason for the switch.
      *
      * If not present, or if the implementation returns DFB_NOIMPL, SwitchFocus is called.
      */
     DirectResult (*ChangeFocus)    ( void                    *context,
                                      SaWManWindowHandle       handle,
                                      SaWManChangeFocusReason  reason );

} SaWManCallbacks;

/*
 * Listeners to be used together with the SaWMan Manager interface
 */

typedef struct {
     void (*TierUpdate)     ( void                 *context,
                              DFBSurfaceStereoEye   stereo_eye,
                              DFBDisplayLayerID     layer_id,
                              const DFBRegion      *updates,
                              unsigned int          num_updates );

     void (*WindowBlit)     ( void                 *context,
                              DFBSurfaceStereoEye   stereo_eye,
                              DFBWindowID           window_id,
                              u32                   resource_id,
                              const DFBRectangle   *src,
                              const DFBRectangle   *dst );
} SaWManListeners;

/***********
 * ISaWMan *
 ***********/

/*
 * Main entry point for clients of SaWMan.
 * Can be used to start/stop external applications and return not-wanted keys to the key collector.
 * Also used to create the singleton Window Manager interface.
 */
DEFINE_INTERFACE(   ISaWMan,

   /** Applications **/

     /*
      * Start an application.
      */
     DirectResult (*Start) (
          ISaWMan                  *thiz,
          const char               *name,
          pid_t                    *ret_pid
     );

     /*
      * Stop an application.
      *
      * Use 0 pid to indicate to kill all but me
      */
     DirectResult (*Stop) (
          ISaWMan                  *thiz,
          pid_t                     pid
     );


   /** Event handling **/

     /*
      * Returns a received key event.
      *
      * This sends the key event to the key collector.
      * In the flags field of the event structure DWEF_RETURNED will be set.
      */
     DirectResult (*ReturnKeyEvent) (
          ISaWMan                  *thiz,
          DFBWindowEvent           *event
     );


   /** Manager **/

     /*
      * Create the manager interface.
      *
      * This only works once and is called by the application manager executable.
      */
     DirectResult (*CreateManager) (
          ISaWMan                  *thiz,
          const SaWManCallbacks    *callbacks,
          void                     *context,
          ISaWManManager          **ret_manager
     );


   /** Updates **/

     /*
      * Get updates.
      */
     DirectResult (*GetUpdates) (
          ISaWMan                  *thiz,
          DFBWindowStackingClass    stacking_class,
          DFBRegion                *ret_updates,
          unsigned int             *ret_num
     );
 
 
   /** Listeners **/
 
     /*
      * Register listeners
      */
     DirectResult (*RegisterListeners) (
          ISaWMan                  *thiz,
          const SaWManListeners    *listeners,
          void                     *context
     );

     /*
      * Unregister listeners
      */
     DirectResult (*UnregisterListeners) (
          ISaWMan                  *thiz,
          void                     *context
     );


   /** Performance **/

     /*
      * Read performance counters.
      */
     DirectResult (*GetPerformance) (
          ISaWMan                  *thiz,
          DFBWindowStackingClass    stacking,
          DFBBoolean                reset,
          unsigned int             *ret_updates,
          unsigned long long       *ret_pixels,
          long long                *ret_duration
     );
)


/******************
 * ISaWManManager *
 ******************/

/*
 * Manages SaWMan.
 * used to request and deny regular window activties like close, remove, switch focus.
 * To be used together with the callbacks given to ISaWMan::CreateManager().
 */
DEFINE_INTERFACE(   ISaWManManager,

   /** Updates **/

     /*
      * Queue an update of the screen, e.g. due to layout changes.
      *
      * If <b>region</b> is NULL the whole screen will be updated.
      */
     DirectResult (*QueueUpdate) (
          ISaWManManager           *thiz,
          DFBWindowStackingClass    stacking,
          const DFBRegion          *region
     );

     /*
      * Process queued updates.
      */
     DirectResult (*ProcessUpdates) (
          ISaWManManager           *thiz,
          DFBSurfaceFlipFlags       flags
     );


   /** Windows **/

     /*
      * Send a close request to a window.
      */
     DirectResult (*CloseWindow) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle
     );

     /*
      * Show or hide a window.
      */
     DirectResult (*SetVisible) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          DFBBoolean                visible
     );

     /*
      * Switches focus to a window.
      */
     DirectResult (*SwitchFocus) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle
     );


   /** Stack **/

     /*
      * Get the dimensions of the stack.
      */
     DirectResult (*GetSize) (
          ISaWManManager           *thiz,
          DFBWindowStackingClass    stacking,
          DFBDimension             *ret_size
     );

     /*
      * Insert a window that has been added.
      *
      * If no <b>relative</b> is specified, the window will be inserted at the <b>top</b> or bottom most
      * position in its stacking class. Otherwise the window will be inserted on <b>top</b> of or below the
      * <b>relative</b>.
      */
     DirectResult (*InsertWindow) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          SaWManWindowHandle        relative,
          SaWManWindowRelation      relation
     );

     /*
      * Remove a window.
      */
     DirectResult (*RemoveWindow) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle
     );


   /** Configuration **/

     /*
      * Choose scaling quality.
      */
     DirectResult (*SetScalingMode) (
          ISaWManManager           *thiz,
          SaWManScalingMode         mode
     );

     DirectResult (*SetWindowConfig) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          SaWManWindowConfigFlags   flags,
          SaWManWindowConfig       *config
     );

   /** Event handling **/

     /*
      * Send an event to a window.
      *
      * This sends an event to the window specified by the <b>handle</b>.
      */
     DirectResult (*SendWindowEvent) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          const DFBWindowEvent     *event
     );


   /** Locking **/

     /*
      * Lock SaWMan for calls to this interface and access to data structures.
      */
     DirectResult (*Lock) (
          ISaWManManager           *thiz
     );

     /*
      * Unlock SaWMan.
      */
     DirectResult (*Unlock) (
          ISaWManManager           *thiz
     );

     /** Information retrieval **/

     /*
      * Returns window information of the requested window.
      * 
      * The window information will be copied into the provided structure,
      * except info->config.keys which will be a pointer to the internal table,
      * so do not change the content of this table with this function.
      */
     DirectResult (*GetWindowInfo) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          SaWManWindowInfo         *ret_info
     );

     /*
      * Returns process information of the requested window.
      * 
      * The process information will be copied into the provided structure.
      */
     DirectResult (*GetProcessInfo) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          SaWManProcess            *ret_process
     );

     /*
      * Determines visibility of a window.
      */
     DirectResult (*IsWindowShowing) (
          ISaWManManager           *thiz,
          SaWManWindowHandle        handle,
          DFBBoolean               *ret_showing
     );
)


#ifdef __cplusplus
}
#endif

#endif


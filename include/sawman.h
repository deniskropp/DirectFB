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

     SWMWF_ALL      = 0x00000001
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
} SaWManProcess;


typedef unsigned long SaWManWindowHandle;
#define SAWMAN_WINDOW_NONE    ((SaWManWindowHandle) 0)


typedef enum {
     SWMCF_NONE          = 0x00000000,

     SWMCF_POSITION      = 0x00000001,
     SWMCF_SIZE          = 0x00000002,
     SWMCF_OPACITY       = 0x00000004,
     SWMCF_STACKING      = 0x00000008,

     SWMCF_OPTIONS       = 0x00000010,
     SWMCF_EVENTS        = 0x00000020,

     SWMCF_COLOR_KEY     = 0x00000100,
     SWMCF_OPAQUE        = 0x00000200,

     //~ SWMCF_KEY_SELECTION = 0x00001000,

     SWMCF_SRC_GEOMETRY  = 0x00010000,
     SWMCF_DST_GEOMETRY  = 0x00020000,

     SWMCF_ALL           = 0x0003033F
} SaWManWindowConfigFlags;

typedef struct {
     DFBRectangle             bounds;         /* position and size */
     int                      opacity;        /* global alpha factor */
     DFBWindowStackingClass   stacking;       /* level boundaries */
     DFBWindowOptions         options;        /* flags for appearance/behaviour */

     DFBWindowEventType       events;         /* mask of enabled events */
     u32                      color_key;      /* transparent pixel */
     DFBRegion                opaque;         /* region of the window forced to be opaque */

     DFBWindowGeometry        src_geometry;   /* advanced source geometry */
     DFBWindowGeometry        dst_geometry;   /* advanced destination geometry */
} SaWManWindowConfig;

#define SAWMANWINDOWCONFIG_COPY( a, b )  {  \
     (a)->bounds       = (b)->bounds;       \
     (a)->opacity      = (b)->opacity;      \
     (a)->stacking     = (b)->stacking;     \
     (a)->options      = (b)->options;      \
     (a)->events       = (b)->events;       \
     (a)->color_key    = (b)->color_key;    \
     (a)->opaque       = (b)->opaque;       \
     (a)->src_geometry = (b)->src_geometry; \
     (a)->dst_geometry = (b)->dst_geometry; }

typedef struct {
     SaWManWindowHandle       handle;

     DFBWindowCapabilities    caps;

     SaWManWindowConfig       config;
} SaWManWindowInfo;


typedef struct {
     SaWManWindowHandle       handle;

     DFBWindowCapabilities    caps;         /* window capabilities, RO */

     SaWManWindowConfigFlags  flags;        /* applicability of below values */
     SaWManWindowConfig       current;
     SaWManWindowConfig       request;
} SaWManWindowReconfig;

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

} SaWManCallbacks;

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
)


#ifdef __cplusplus
}
#endif

#endif


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


#include <direct/interface.h>

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


/***********
 * ISaWMan *
 ***********/

/*
 * <i>No summary yet...</i>
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
      * In the flags field of the event structure DWET_RETURNED will be set.
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
 * <i>No summary yet...</i>
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
          SaWManWindow             *window
     );

     /*
      * Show or hide a window.
      */
     DirectResult (*SetVisible) (
          ISaWManManager           *thiz,
          SaWManWindow             *window,
          DFBBoolean                visible
     );

     /*
      * Switches focus to a window.
      */
     DirectResult (*SwitchFocus) (
          ISaWManManager           *thiz,
          SaWManWindow             *window
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
          SaWManWindow             *window,
          SaWManWindow             *relative,
          DFBBoolean                top
     );

     /*
      * Remove a window.
      */
     DirectResult (*RemoveWindow) (
          ISaWManManager           *thiz,
          SaWManWindow             *window
     );


   /** Configuration **/

     /*
      * Choose scaling quality.
      */
     DirectResult (*SetScalingMode) (
          ISaWManManager           *thiz,
          SaWManScalingMode         mode
     );


   /** Event handling **/

     /*
      * Send an event to a window.
      *
      * This sends an event to the window specified by the <b>window_id</b>.
      */
     DirectResult (*SendWindowEvent) (
          ISaWManManager           *thiz,
          const DFBWindowEvent     *event,
          DFBWindowID               window_id
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


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

#ifndef __FUSIONDALE_H__
#define __FUSIONDALE_H__

#include <direct/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Version handling.
 */
extern const unsigned int fusiondale_major_version;
extern const unsigned int fusiondale_minor_version;
extern const unsigned int fusiondale_micro_version;
extern const unsigned int fusiondale_binary_age;
extern const unsigned int fusiondale_interface_age;

/*
 * Check for a certain FusionDale version.
 * In case of an error a message is returned describing the mismatch.
 */
const char * FusionDaleCheckVersion( unsigned int required_major,
                                     unsigned int required_minor,
                                     unsigned int required_micro );

/*
 * Main FusionDale interface.
 */
DECLARE_INTERFACE( IFusionDale )

/*
 * Event manager.
 */
DECLARE_INTERFACE( IFusionDaleMessenger )

/*
 * Component manager.
 */
DECLARE_INTERFACE( IComa )

/*
 * Component.
 */
DECLARE_INTERFACE( IComaComponent )

/*
 * Parses the command-line and initializes some variables. You absolutely need to
 * call this before doing anything else. Removes all options used by FusionDale from argv.
 */
DFBResult FusionDaleInit(
                           int    *argc,   /* pointer to main()'s argc */
                           char *(*argv[]) /* pointer to main()'s argv */
                         );

/*
 * Sets configuration parameters supported on command line and in config file.
 * Can only be called before FusionDaleCreate but after FusionDaleInit.
 */
DFBResult FusionDaleSetOption(
                                const char *name,
                                const char *value
                              );

/*
 * Creates the super interface.
 */
DFBResult FusionDaleCreate(
                             IFusionDale **ret_interface  /* pointer to the created interface */
                           );

/*
 * Print a description of the result code along with an
 * optional message that is put in front with a colon.
 */
DFBResult FusionDaleError(
                            const char *msg,    /* optional message */
                            DFBResult   result  /* result code to interpret */
                          );
                          
/*
 * Behaves like FusionDaleError, but shuts down the calling application.
 */
DFBResult FusionDaleErrorFatal(
                            const char *msg,    /* optional message */
                            DFBResult   result  /* result code to interpret */
                          );

/*
 * Returns a string describing 'result'.
 */
const char *FusionDaleErrorString(
                                    DFBResult result
                                  );
                                  
/*
 * Retrieves information about supported command-line flags in the
 * form of a user-readable string formatted suitable to be printed
 * as usage information.
 */
const char *FusionDaleUsageString( void );


/*
 * <i><b>IFusionDale</b></i> is the main FusionDale interface.
 */
DEFINE_INTERFACE( IFusionDale,

   /** Events **/

     /*
      * Create a new event manager.
      */
     DFBResult (*CreateMessenger) (
          IFusionDale           *thiz,
          IFusionDaleMessenger **ret_messenger
     );

     /*
      * Get an interface to an existing event manager.
      */
     DFBResult (*GetMessenger) (
          IFusionDale           *thiz,
          IFusionDaleMessenger **ret_messenger
     );


   /** Component Manager **/

     /*
      * Get an interface to a component manager.
      *
      * The <b>name</b> is a unique identifier.
      * The component manager will be created if it doesn't exist.
      */
     DFBResult (*EnterComa) (
          IFusionDale           *thiz,
          const char            *name,
          IComa                **ret_coma
     );
)



typedef unsigned long FDMessengerEventID;
typedef unsigned long FDMessengerListenerID;

#define FDM_EVENT_ID_NONE     ((unsigned long)0)
#define FDM_LISTENER_ID_NONE  ((unsigned long)0)


typedef void (*FDMessengerEventCallback)( FDMessengerEventID  event_id,
                                          int                 param,
                                          void               *data,
                                          int                 data_size,
                                          void               *context );


/*
 * <i><b>IFusionDaleMessenger</b></i> is an event manager.
 */
DEFINE_INTERFACE( IFusionDaleMessenger,

   /** Events **/

     DFBResult (*RegisterEvent) (
          IFusionDaleMessenger     *thiz,
          const char               *name,
          FDMessengerEventID       *ret_id
     );

     DFBResult (*UnregisterEvent) (
          IFusionDaleMessenger     *thiz,
          FDMessengerEventID        event_id
     );

     DFBResult (*IsEventRegistered) (
          IFusionDaleMessenger     *thiz,
          const char               *name
     );


   /** Listeners **/

     DFBResult (*RegisterListener) (
          IFusionDaleMessenger     *thiz,
          FDMessengerEventID        event_id,
          FDMessengerEventCallback  listener,
          void                     *context,
          FDMessengerListenerID    *ret_id
     );

     DFBResult (*UnregisterListener) (
          IFusionDaleMessenger     *thiz,
          FDMessengerListenerID     listener_id
     );


   /** Dispatch **/

     DFBResult (*SendSimpleEvent) (
          IFusionDaleMessenger     *thiz,
          FDMessengerEventID        event_id,
          int                       param
     );

     DFBResult (*SendEvent) (
          IFusionDaleMessenger     *thiz,
          FDMessengerEventID        event_id,
          int                       param,
          void                     *data,
          unsigned int              data_size
     );


   /** Message data **/

     DFBResult (*AllocateData) (
          IFusionDaleMessenger     *thiz,
          unsigned int              data_size,
          void                    **ret_data
     );
)



typedef unsigned long ComaMethodID;
typedef unsigned long ComaNotificationID;

typedef void (*ComaMethodFunc)  ( void               *ctx,
                                  ComaMethodID        method,
                                  void               *arg,
                                  unsigned int        magic );

typedef void (*ComaNotifyFunc)  ( void               *ctx,
                                  ComaNotificationID  notification,
                                  void               *arg );

typedef void (*ComaListenerFunc)( void               *ctx,
                                  void               *arg );

typedef struct {
     ComaNotificationID  id;
     ComaNotifyFunc      func;
     void               *ctx;
} ComaNotificationInit;

typedef struct {
     ComaNotificationID  id;
     ComaListenerFunc    func;
     void               *ctx;
} ComaListenerInit;


/*
 * <i><b>IComa</b></i> is a component manager.
 */
DEFINE_INTERFACE( IComa,

   /** Components **/

     DFBResult (*CreateComponent) (
          IComa                    *thiz,
          const char               *name,
          ComaMethodFunc            func,
          int                       num_notifications,
          void                     *ctx,
          IComaComponent          **ret_component
     );

     DFBResult (*GetComponent) (
          IComa                    *thiz,
          const char               *name,
          unsigned int              timeout,
          IComaComponent          **ret_component
     );


   /** Shared memory **/

     DFBResult (*Allocate) (
          IComa                    *thiz,
          unsigned int              bytes,
          void                    **ret_ptr
     );

     DFBResult (*Deallocate) (
          IComa                    *thiz,
          void                     *ptr
     );


   /** Thread local SHM **/

     DFBResult (*GetLocal) (
          IComa                    *thiz,
          unsigned int              bytes,
          void                    **ret_ptr
     );

     DFBResult (*FreeLocal) (
          IComa                    *thiz
     );
)

/*
 * <i><b>IComaComponent</b></i> is a component.
 */
DEFINE_INTERFACE( IComaComponent,

   /** Initialization **/

     DFBResult (*InitNotification) (
          IComaComponent           *thiz,
          ComaNotificationID        id,
          ComaNotifyFunc            func,
          void                     *ctx
     );

     DFBResult (*InitNotifications) (
          IComaComponent                *thiz,
          const ComaNotificationInit    *inits,
          int                            num_inits,
          void                          *ctx
     );


   /** Methods **/

     DFBResult (*Call) (
          IComaComponent           *thiz,
          ComaMethodID              method,
          void                     *arg,
          int                      *ret_val
     );

     DFBResult (*Return) (
          IComaComponent           *thiz,
          int                       val,
          unsigned int              magic
     );


   /** Notifications **/

     DFBResult (*Notify) (
          IComaComponent           *thiz,
          ComaNotificationID        id,
          void                     *arg
     );

     DFBResult (*Listen) (
          IComaComponent           *thiz,
          ComaNotificationID        id,
          ComaListenerFunc          func,
          void                     *ctx
     );

     DFBResult (*InitListeners) (
          IComaComponent           *thiz,
          const ComaListenerInit   *inits,
          int                       num_inits,
          void                     *ctx
     );

     DFBResult (*Unlisten) (
          IComaComponent           *thiz,
          ComaNotificationID        id
     );
)

#ifdef __cplusplus
}
#endif

#endif


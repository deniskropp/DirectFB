/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>


#include "android_system.h"

/**********************************************************************************************************************/

extern int main( int argc, char **argv );

static void *
dfb_main_thread( DirectThread *thread,
                 void         *arg )
{
     int   ret;
     char *argv[] = { "android-native-dfb-app", "--dfb:debug" };

     LOGI( "Running main()..." );

     ret = main( 1, argv );

     LOGI( "main() has returned %d!", ret );

     return NULL;
}

/**
 * Process the next input event.
 */
static int32_t
native_handle_input( struct android_app* app, AInputEvent* event )
{
     AndroidNativeData* native_data = (AndroidNativeData*)app->userData;
     if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
//          native_data->state.x = AMotionEvent_getX(event, 0);
//          native_data->state.y = AMotionEvent_getY(event, 0);
//          LOGI("input: x=%d y=%d",
//               AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0));
          return 1;
     }
     return 0;
}

/**
 * Process the next main command.
 */
static void
native_handle_cmd( struct android_app* app, int32_t cmd )
{
     AndroidNativeData* native_data = (AndroidNativeData*)app->userData;

     switch (cmd) {
          case APP_CMD_SAVE_STATE:
               // The system has asked us to save our current state.  Do so.
//               native_data->app->savedState = malloc(sizeof(struct saved_state));
//               *((struct saved_state*)native_data->app->savedState) = native_data->state;
//               native_data->app->savedStateSize = sizeof(struct saved_state);
               break;
          case APP_CMD_INIT_WINDOW:
               // The window is being shown, get it ready.
               if (native_data->app->window != NULL) {
//                    native_init_display(native_data);
//                    native_draw_frame(native_data);

                    native_data->main_thread = direct_thread_create( DTT_DEFAULT, dfb_main_thread, native_data, "dfb-main" );
               }
               break;
          case APP_CMD_TERM_WINDOW:
               // The window is being hidden or closed, clean it up.
//               native_term_display(native_data);

               direct_thread_join( native_data->main_thread );
               direct_thread_destroy( native_data->main_thread );
               break;
          case APP_CMD_GAINED_FOCUS:
               // When our app gains focus, we start monitoring the accelerometer.
               if (native_data->accelerometerSensor != NULL) {
                    ASensorEventQueue_enableSensor(native_data->sensorEventQueue,
                                                   native_data->accelerometerSensor);
                    // We'd like to get 60 events per second (in us).
                    ASensorEventQueue_setEventRate(native_data->sensorEventQueue,
                                                   native_data->accelerometerSensor, (1000L/60)*1000);
               }
               break;
          case APP_CMD_LOST_FOCUS:
               // When our app loses focus, we stop monitoring the accelerometer.
               // This is to avoid consuming battery while not being used.
               if (native_data->accelerometerSensor != NULL) {
                    ASensorEventQueue_disableSensor(native_data->sensorEventQueue,
                                                    native_data->accelerometerSensor);
               }
//               native_draw_frame(native_data);
               break;
     }
}


// FIXME
AndroidNativeData native_data;
     
/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void
android_main( struct android_app* state )
{
     // Make sure glue isn't stripped.
     app_dummy();

     memset(&native_data, 0, sizeof(native_data));
     state->userData = &native_data;
     state->onAppCmd = native_handle_cmd;
     state->onInputEvent = native_handle_input;
     native_data.app = state;

     // Prepare to monitor accelerometer
     native_data.sensorManager = ASensorManager_getInstance();
     native_data.accelerometerSensor = ASensorManager_getDefaultSensor(native_data.sensorManager,
                                                                       ASENSOR_TYPE_ACCELEROMETER);
     native_data.sensorEventQueue = ASensorManager_createEventQueue(native_data.sensorManager,
                                                                    state->looper, LOOPER_ID_USER, NULL, NULL);

     if (state->savedState != NULL) {
          // We are starting with a previous saved state; restore from it.
//          native_data.state = *(struct saved_state*)state->savedState;
     }

     // loop waiting for stuff to do.

     while (1) {
          // Read all pending events.
          int ident;
          int events;
          struct android_poll_source* source;

          // If not animating, we will block forever waiting for events.
          // If animating, we loop until all events are read, then continue
          // to draw the next frame of animation.
          while ((ident=ALooper_pollAll(-1, NULL, &events, (void**)&source)) >= 0) {

               // Process this event.
               if (source != NULL) {
                    source->process(state, source);
               }

               // If a sensor has data, process it now.
               if (ident == LOOPER_ID_USER) {
                    if (native_data.accelerometerSensor != NULL) {
                         ASensorEvent event;
                         while (ASensorEventQueue_getEvents(native_data.sensorEventQueue, &event, 1) > 0) {
                              LOGI("accelerometer: x=%f y=%f z=%f",
                                   event.acceleration.x, event.acceleration.y,
                                   event.acceleration.z);
                         }
                    }
               }

               // Check if we are exiting.
               if (state->destroyRequested != 0) {
//                    native_term_display(&native_data);
                    return;
               }
          }
     }
}


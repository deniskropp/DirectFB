/*
   (c) Copyright 2011  Denis Oliver Kropp

   All rights reserved.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <config.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <directfb.h>


static DFBResult
Test_Sensitivity( IDirectFB        *dfb,
                  DFBInputDeviceID  device_id )
{
     DFBResult             ret;
     IDirectFBInputDevice *device;
     IDirectFBEventBuffer *buffer;
     int                   i, n;
     int                   sensitivities[] = { 0x100, 0x200, 0x80 };

     D_INFO( "DFBTest/Input: Testing sensitivity with input device %u...\n", device_id );

     ret = dfb->GetInputDevice( dfb, device_id, &device );
     if (ret) {
          D_DERROR( ret, "DFBTest/Input: GetInputDevice( %u ) failed!\n", device_id );
          return ret;
     }

     ret = device->CreateEventBuffer( device, &buffer );
     if (ret) {
          D_DERROR( ret, "DFBTest/Input: CreateEventBuffer() failed!\n" );
          device->Release( device );
          return ret;
     }

     for (i=0; i<D_ARRAY_SIZE(sensitivities); i++) {
          DFBInputDeviceConfig config;
          unsigned int         move = 0;

          D_INFO( "DFBTest/Input: Setting sensitivity to 0x%x, please move mouse!\n", sensitivities[i] );

          config.flags       = DIDCONF_SENSITIVITY;
          config.sensitivity = sensitivities[i];

          ret = device->SetConfiguration( device, &config );
          if (ret) {
               D_DERROR( ret, "DFBTest/Input: SetConfiguration() failed!\n" );
               buffer->Release( buffer );
               device->Release( device );
               return ret;
          }

          buffer->Reset( buffer );

          for (n=0; n<500; n++) {
               DFBInputEvent event;

               buffer->WaitForEvent( buffer );

               buffer->GetEvent( buffer, DFB_EVENT(&event) );

               switch (event.type) {
                    case DIET_AXISMOTION:
                         if (event.flags & DIEF_AXISREL) {
                              //D_INFO( "DFBTest/Input: Motion (axis %d) by %d\n", event.axis, event.axisrel );

                              if (event.axisrel > 0)
                                   move += event.axisrel;
                              else
                                   move -= event.axisrel;
                         }

                         break;

                    default:
                         break;
               }
          }

          D_INFO( "DFBTest/Input: Average movement %d.\n", move / n );

          direct_thread_sleep( 1000000 );
     }

     buffer->Release( buffer );
     device->Release( device );

     return DFB_OK;
}

/**********************************************************************************************************************/

static int
print_usage( const char *prg )
{
     fprintf (stderr, "\n");
     fprintf (stderr, "== DirectFB Input Test (version %s) ==\n", DIRECTFB_VERSION);
     fprintf (stderr, "\n");
     fprintf (stderr, "Usage: %s [options]\n", prg);
     fprintf (stderr, "\n");
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "  -h, --help                        Show this help message\n");
     fprintf (stderr, "  -v, --version                     Print version information\n");
     fprintf (stderr, "  -d, --device    <id>              Input device ID, default is 0\n");

     return -1;
}

static DFBBoolean
parse_id( const char *arg, unsigned int *_id )
{
     if (sscanf( arg, "%u", _id ) != 1) {
          fprintf (stderr, "\nInvalid ID specified!\n\n" );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     DFBResult         ret;
     int               i;
     IDirectFB        *dfb;
     DFBInputDeviceID  device_id = 0;

     /* Initialize DirectFB. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          D_DERROR( ret, "DFBTest/Input: DirectFBInit() failed!\n" );
          return ret;
     }

     /* Parse arguments. */
     for (i=1; i<argc; i++) {
          const char *arg = argv[i];

          if (strcmp( arg, "-h" ) == 0 || strcmp (arg, "--help") == 0)
               return print_usage( argv[0] );
          else if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "dfbtest_input version %s\n", DIRECTFB_VERSION);
               return false;
          }
          else if (strcmp (arg, "-d") == 0 || strcmp (arg, "--device") == 0) {
               if (++i == argc) {
                    print_usage (argv[0]);
                    return false;
               }

               if (!parse_id( argv[i], &device_id ))
                    return false;
          }
          else
               return print_usage( argv[0] );
     }

     /* Create super interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Input: DirectFBCreate() failed!\n" );
          return ret;
     }

     Test_Sensitivity( dfb, device_id );

     /* Shutdown DirectFB. */
     ret = dfb->Release( dfb );
     if (ret) {
          D_DERROR( ret, "DFBTest/Input: IDirectFB::Release() failed!\n" );
          return ret;
     }

     return 0;
}


/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <sawman.h>


#include <unistd.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/messages.h>

#include <core/windows_internal.h>

#include <sawman_manager.h>


#define CHECK(x)                                  \
     do {                                         \
          DFBResult ret = (x);                    \
          if (ret != DFB_OK) {                    \
               DirectFBError(#x,ret);             \
               goto out;                          \
          }                                       \
     } while (0)


static DirectResult
start_request( void       *context,
               const char *name,
               pid_t      *ret_pid )
{
     D_INFO( "SaWMan/Sample1: Start request for '%s'!\n", name );

     return DFB_UNIMPLEMENTED;
}

static DirectResult
stop_request( void     *context,
              pid_t     pid,
              FusionID  caller )
{
     D_INFO( "SaWMan/Sample1: Stop request from Fusion ID 0x%lx for pid %d!\n", caller, pid );

     return DFB_OK;
}

static DirectResult
process_added( void          *context,
               SaWManProcess *process )
{
     D_INFO( "SaWMan/Sample1: Process added (%d) [%lu]!\n", process->pid, process->fusion_id );

     return DFB_OK;
}

static DirectResult
process_removed( void          *context,
                 SaWManProcess *process )
{
     D_INFO( "SaWMan/Sample1: Process removed (%d) [%lu]!\n", process->pid, process->fusion_id );

     return DFB_OK;
}


static const SaWManCallbacks callbacks = {
     Start:          start_request,
     Stop:           stop_request,
     ProcessAdded:   process_added,
     ProcessRemoved: process_removed
};



int
main( int argc, char** argv )
{
     IDirectFB      *dfb     = NULL; 
     ISaWMan        *saw     = NULL; 
     ISaWManManager *manager = NULL; 

     D_INFO( "SaWMan/Sample1: Initializing...\n" );

     CHECK( DirectFBInit( &argc, &argv ) );

     CHECK( DirectFBCreate( &dfb ) );

     CHECK( SaWManCreate( &saw ) );

     CHECK( saw->CreateManager( saw, &callbacks, NULL, &manager ) );

     pause();


out:
     D_INFO( "SaWMan/Sample1: Shutting down...\n" );

     if (manager)
          manager->Release( manager );

     if (saw)
          saw->Release( saw );

     if (dfb)
          dfb->Release( dfb );

     return 0;
}


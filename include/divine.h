/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __DIVINE_H__
#define __DIVINE_H__

#include <directfb.h>

#ifdef WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DIVINE_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DIVINE_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DIVINE_EXPORTS
#define DIVINE_API __declspec(dllexport)
#else
#define DIVINE_API __declspec(dllimport)
#endif
#else
#define DIVINE_API
#endif

/*
 * Main interface of DiVine, created by DiVineCreate().
 */
D_DECLARE_INTERFACE( IDiVine )

/*
 * Parses the command-line and initializes some variables. You
 * absolutely need to call this before doing anything else.
 * Removes all options used by DiVine from argv.
 */
DFBResult DIVINE_API DiVineInit( int *argc, char *(*argv[]) );

/*
 * Creates the super interface.
 */
DFBResult DIVINE_API DiVineCreate(
                                    IDiVine **interface_ptr  /* pointer to the created interface */
                                  );

/***********
 * IDiVine *
 ***********/

/*
 * <i>No summary yet...</i>
 */
D_DEFINE_INTERFACE(   IDiVine,

   /** Events **/

     /*
      * Sends an input event.
      */
     DFBResult (*SendEvent) (
          IDiVine                 *thiz,
          const DFBInputEvent     *event
     );

     /*
      * Sends an input key symbol.
      */
     DFBResult (*SendSymbol) (
          IDiVine                 *thiz,
          DFBInputDeviceKeySymbol  symbol
     );
);


/*
 * The DiVine struct represents the connection to the input driver.
 */
typedef struct _DiVine DiVine;

/*
 * Opens a connection to the input driver by opening the pipe
 * specified by 'path'.
 *
 * Returns the DiVine connection object.
 */
DiVine DIVINE_API *divine_open (const char *path);


/*
 * Sends an input event.
 */
void DIVINE_API divine_send_event (DiVine *divine, const DFBInputEvent *event);

/*
 * Sends a press and a release event for the specified symbol.
 */
void DIVINE_API divine_send_symbol (DiVine *divine, DFBInputDeviceKeySymbol symbol);

/*
 * Sends a press and a release event for the specified identifier.
 */
void DIVINE_API divine_send_identifier (DiVine *divine, DFBInputDeviceKeyIdentifier identifier);

/*
 * Sends a press and a release event for the specified ANSI string.
 * Use this to feed terminal input into a DirectFB application.
 */
void DIVINE_API divine_send_vt102 (DiVine *divine, int size, const char *ansistr);

/*
 *
 */
void DIVINE_API divine_send_motion_absolute( DiVine *divine, int x, int y );

/*
 *
 */
void DIVINE_API divine_send_button_press( DiVine *divine, DFBInputDeviceButtonIdentifier button );

/*
 *
 */
void DIVINE_API divine_send_button_release( DiVine *divine, DFBInputDeviceButtonIdentifier button );


/*
 * Closes the pipe and destroys the connection object.
 */
void DIVINE_API divine_close (DiVine *divine);

#endif

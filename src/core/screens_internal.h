/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __DFB__CORE__SCREENS_INTERNAL_H__
#define __DFB__CORE__SCREENS_INTERNAL_H__

#include <directfb.h>

#include <core/fusion/lock.h>

#include <core/coretypes.h>
#include <core/screens.h>

typedef struct {
     DFBScreenMixerDescription   description;
     DFBScreenMixerConfig        configuration;
} CoreScreenMixer;

typedef struct {
     DFBScreenEncoderDescription description;
     DFBScreenEncoderConfig      configuration;
} CoreScreenEncoder;

typedef struct {
     DFBScreenOutputDescription  description;
     DFBScreenOutputConfig       configuration;
} CoreScreenOutput;

typedef struct {
     FusionSkirmish        lock;

     DFBScreenID           screen_id;

     DFBScreenDescription  description;

     CoreScreenMixer      *mixers;
     CoreScreenEncoder    *encoders;
     CoreScreenOutput     *outputs;

     void                 *screen_data;
} CoreScreenShared;

struct __DFB_CoreScreen {
     CoreScreenShared     *shared;

     CoreDFB              *core;
     GraphicsDevice       *device;

     ScreenFuncs          *funcs;

     void                 *driver_data;
     void                 *screen_data;   /* copy of shared->screen_data */
};

#endif


/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#include "CoreScreen.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/screen.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreScreen, "DirectFB/CoreScreen", "DirectFB CoreScreen" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
IScreen_Real::SetPowerMode(
                    DFBScreenPowerMode                         mode
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    return dfb_screen_set_powermode( obj, mode );
}


DFBResult
IScreen_Real::WaitVSync(

)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    return dfb_screen_wait_vsync( obj );
}


DFBResult
IScreen_Real::GetVSyncCount(
                    u64                                       *ret_count
)
{
    DFBResult     ret;
    unsigned long count;

    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    ret = dfb_screen_get_vsync_count( obj, &count );
    if (ret)
        return ret;

    *ret_count = count;

    return DFB_OK;
}


DFBResult
IScreen_Real::TestMixerConfig(
                    u32                                        mixer,
                    const DFBScreenMixerConfig                *config,
                    DFBScreenMixerConfigFlags                 *ret_failed
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );
    D_ASSERT( ret_failed != NULL );

    return dfb_screen_test_mixer_config( obj, mixer, config, ret_failed );
}


DFBResult
IScreen_Real::SetMixerConfig(
                    u32                                        mixer,
                    const DFBScreenMixerConfig                *config
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );

    return dfb_screen_set_mixer_config( obj, mixer, config );
}


DFBResult
IScreen_Real::TestEncoderConfig(
                    u32                                        encoder,
                    const DFBScreenEncoderConfig              *config,
                    DFBScreenEncoderConfigFlags               *ret_failed
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );
    D_ASSERT( ret_failed != NULL );

    return dfb_screen_test_encoder_config( obj, encoder, config, ret_failed );
}


DFBResult
IScreen_Real::SetEncoderConfig(
                    u32                                        encoder,
                    const DFBScreenEncoderConfig              *config
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );

    return dfb_screen_set_encoder_config( obj, encoder, config );
}


DFBResult
IScreen_Real::TestOutputConfig(
                    u32                                        output,
                    const DFBScreenOutputConfig               *config,
                    DFBScreenOutputConfigFlags                *ret_failed
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );
    D_ASSERT( ret_failed != NULL );

    return dfb_screen_test_output_config( obj, output, config, ret_failed );
}


DFBResult
IScreen_Real::SetOutputConfig(
                    u32                                        output,
                    const DFBScreenOutputConfig               *config
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );

    return dfb_screen_set_output_config( obj, output, config );
}


DFBResult
IScreen_Real::GetScreenSize(
                    DFBDimension                              *ret_size
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( ret_size != NULL );

    return dfb_screen_get_screen_size( obj, &ret_size->w, &ret_size->h );
}


DFBResult
IScreen_Real::GetLayerDimension(
                    CoreLayer                                 *layer,
                    DFBDimension                              *ret_size
)
{
    D_DEBUG_AT( DirectFB_CoreScreen, "IScreen_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( layer != NULL );
    D_ASSERT( ret_size != NULL );

    return dfb_screen_get_layer_dimension( obj, layer, &ret_size->w, &ret_size->h );
}


}

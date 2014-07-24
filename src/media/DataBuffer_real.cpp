/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <direct/Types++.h>

#include "DataBuffer.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_DataBuffer, "DirectFB/DataBuffer", "DirectFB DataBuffer" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
IDataBuffer_Real::Flush(

)
{
    return obj->Flush( obj );
}


DFBResult
IDataBuffer_Real::Finish(

)
{
    return obj->Finish( obj );
}


DFBResult
IDataBuffer_Real::SeekTo(
                    u64                                        offset
)
{
    return obj->SeekTo( obj, offset );
}


DFBResult
IDataBuffer_Real::GetPosition(
                    u64                                       *ret_offset
)
{
    DFBResult    ret;
    unsigned int offset;

    ret = obj->GetPosition( obj, &offset );
    if (ret == DFB_OK)
        *ret_offset = offset;

    return ret;
}


DFBResult
IDataBuffer_Real::GetLength(
                    u64                                       *ret_length
)
{
    DFBResult    ret;
    unsigned int length;

    ret = obj->GetLength( obj, &length );
    if (ret == DFB_OK)
        *ret_length = length;

    return ret;
}


DFBResult
IDataBuffer_Real::WaitForData(
                    u64                                        length
)
{
    return obj->WaitForData( obj, length );
}


DFBResult
IDataBuffer_Real::WaitForDataWithTimeout(
                    u64                                        length,
                    u64                                        timeout_ms
)
{
    return obj->WaitForDataWithTimeout( obj, length, timeout_ms / 1000ULL, timeout_ms % 1000ULL );
}


DFBResult
IDataBuffer_Real::GetData(
                    u32                                        length,
                    u8                                        *ret_data,
                    u32                                       *ret_read
)
{
    return obj->GetData( obj, length, ret_data, ret_read );
}


DFBResult
IDataBuffer_Real::PeekData(
                    u32                                        length,
                    s64                                        offset,
                    u8                                        *ret_data,
                    u32                                       *ret_read
)
{
    return obj->PeekData( obj, length, offset, ret_data, ret_read );
}


DFBResult
IDataBuffer_Real::HasData(

)
{
    return obj->HasData( obj );
}


DFBResult
IDataBuffer_Real::PutData(
                    const u8                                  *data,
                    u32                                        length
)
{
    return obj->PutData( obj, data, length );
}


}

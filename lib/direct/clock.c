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

#include <config.h>

#include <sys/time.h>
#include <time.h>

#include <direct/clock.h>


static struct timeval start_time = { 0, 0 };

long long
direct_clock_get_micros()
{
     struct timeval tv;

     if (start_time.tv_sec == 0) {
          gettimeofday( &start_time, NULL );
          return 0;
     }

     gettimeofday( &tv, NULL );

     return (long long)(tv.tv_sec - start_time.tv_sec) * 1000000LL +
            (long long)(tv.tv_usec - start_time.tv_usec);
}

long long
direct_clock_get_millis()
{
     return direct_clock_get_micros() / 1000LL;
}


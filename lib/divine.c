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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <divine.h>

struct _DiVine {
  int fd; /* The file descriptor of the connection (pipe) */
};

DiVine *
divine_open (const char *path)
{
  int     fd;
  DiVine *divine;

  /* Open the pipe specified by 'path' */
  fd = open (path, O_WRONLY);
  if (fd < 0)
    {
      perror (path);
      return NULL;
    }

  /* Allocate connection object */
  divine = calloc (1, sizeof(DiVine));
  if (!divine)
    {
      fprintf (stderr, "Out of memory!!!\n");
      return NULL;
    }

  /* Fill out connection information */
  divine->fd = fd;

  /* Return connection object */
  return divine;
}

void
divine_send_symbol (DiVine *divine, DFBInputDeviceKeySymbol symbol)
{
  DFBInputEvent event;

  /* Construct 'press' event */
  event.flags      = DIEF_KEYSYMBOL;
  event.type       = DIET_KEYPRESS;
  event.key_symbol = symbol;

  /* Write 'press' event to pipe */
  write (divine->fd, &event, sizeof(DFBInputEvent));

  /* Turn into 'release' event */
  event.type = DIET_KEYRELEASE;

  /* Write 'release' event to pipe */
  write (divine->fd, &event, sizeof(DFBInputEvent));
}

void
divine_close (DiVine *divine)
{
  /* Close the pipe */
  close (divine->fd);

  /* Free connection object */
  free (divine);
}

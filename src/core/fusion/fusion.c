/*
   (c) Copyright 2001  Denis Oliver Kropp <dok@directfb.org>
   All rights reserved.

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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "fusion_types.h"

#include "fusion_internal.h"


#ifndef FUSION_FAKE

/***************************
 *  Internal declarations  *
 ***************************/

/*
 *
 */
typedef struct {
  long  next_fid;
  void *next_shmat_addr;
} FusionShared;

/*
 *
 */
typedef struct {
  int  fid;

  int  shared_shm;
  int  shared_sem;

  FusionShared *shared;
} Fusion;


static void _fusion_check_limits();

/*******************
 *  Internal data  *
 *******************/

/*
 *
 */
static Fusion *fusion = NULL;


/****************
 *  Public API  *
 ****************/

int fusion_init()
{
  AcquisitionStatus as;

  /* check against multiple initialization */
  if (fusion)
    return -1;

  _fusion_check_limits();

  /* allocate local Fusion data */
  fusion = (Fusion*)calloc (1, sizeof(Fusion));

  /* acquire shared Fusion data */
  as = _shm_acquire (FUSION_KEY_PREFIX,
                     sizeof(FusionShared), &fusion->shared_shm);

  /* on failure free local Fusion data and return */
  if (as == AS_Failure)
    {
      free (fusion);
      fusion = NULL;
      return -1;
    }

  fusion->shared = shmat (fusion->shared_shm, NULL, 0);

  /* initialize shared Fusion data? */
  if (as == AS_Initialize)
    {
      fusion->shared->next_fid = 1;
      fusion->shared->next_shmat_addr = (void*)0x70000000;
    }

  /* set local Fusion ID */
  fusion->fid = fusion->shared->next_fid++;

  /* initialize shmalloc part */
  if (_shmalloc_init())
    {
      /* destroy shared Fusion data if we initialized it */
      if (as == AS_Initialize)
    shmctl (fusion->shared_shm, IPC_RMID, NULL);

      free (fusion);
      fusion = NULL;
      return -1;
    }

  return _fusion_id();
}

void fusion_exit()
{
  if (!fusion)
    return;

  _shmalloc_exit();

  switch (_shm_abolish (fusion->shared_shm, fusion->shared))
    {
    case AB_Destroyed:
      FDEBUG ("I'VE BEEN THE LAST\n");
      break;

    case AB_Detached:
      FDEBUG ("OTHERS LEFT\n");
      break;

    case AB_Failure:
      FDEBUG ("UUUUUUUUH\n");
      break;
    }

  free (fusion);
  fusion = NULL;
}


/*******************************
 *  Fusion internal functions  *
 *******************************/

int _fusion_id()
{
  if (fusion)
    return fusion->fid;

  FERROR ("called without prior init!\n");

  return -1;
}

void *_fusion_shmat (int shmid)
{
  void *ret;

  struct shmid_ds buf;

  if (!fusion)
    {
      FERROR ("called without prior init!\n");
      return NULL;
    }

  if (shmctl (shmid, IPC_STAT, &buf) < 0)
    {
      FPERROR ("shmctl failed");
      return NULL;
    }

  FDEBUG ("attaching at %p\n", fusion->shared->next_shmat_addr);

  ret = shmat (shmid, fusion->shared->next_shmat_addr, 0);
  if (ret == (void*)(-1))
    {
      FPERROR ("shmat failed");
      return NULL;
    }

  FDEBUG ("attached at %p\n", ret);

  fusion->shared->next_shmat_addr = ret + buf.shm_segsz;

  return ret;
}


/*******************************
 *  File internal functions  *
 *******************************/

static void _fusion_check_msgmni()
{
  int     fd;
  int     msgmni;
  ssize_t len;
  char    buf[20];

  fd = open ("/proc/sys/kernel/msgmni", O_RDWR);
  if (fd < 0)
    {
      perror ("opening /proc/sys/kernel/msgmni");
      return;
    }

  len = read (fd, buf, 19);
  if (len < 1)
    {
      perror ("reading /proc/sys/kernel/msgmni");
      close (fd);
      return;
    }

  if (sscanf (buf, "%d", &msgmni))
    {
      if (msgmni >= FUSION_MSGMNI)
        {
          close (fd);
          return;
        }
    }

  snprintf (buf, 19, "%d\n", FUSION_MSGMNI);

  if (write (fd, buf, strlen (buf) + 1) < 0)
    perror ("writing /proc/sys/kernel/msgmni");

  close (fd);
}

static void _fusion_check_sem()
{
  int     fd;
  int     per, sys, ops, num;
  ssize_t len;
  char    buf[40];

  fd = open ("/proc/sys/kernel/sem", O_RDWR);
  if (fd < 0)
    {
      perror ("opening /proc/sys/kernel/sem");
      return;
    }

  len = read (fd, buf, 39);
  if (len < 1)
    {
      perror ("reading /proc/sys/kernel/sem");
      close (fd);
      return;
    }

  if (sscanf (buf, "%d %d %d %d", &per, &sys, &ops, &num) < 4)
    {
      fprintf (stderr, "could not parse /proc/sys/kernel/sem\n");
      close (fd);
      return;
    }

  if (num >= FUSION_SEM_ARRAYS)
    {
      close (fd);
      return;
    }
  else
    num = FUSION_SEM_ARRAYS;

  snprintf (buf, 39, "%d %d %d %d\n", per, per * num, ops, num);

  if (write (fd, buf, strlen (buf) + 1) < 0)
    perror ("writing /proc/sys/kernel/sem");

  close (fd);
}

static void _fusion_check_limits()
{
     _fusion_check_msgmni();
     _fusion_check_sem();
}

#endif /* !FUSION_FAKE */


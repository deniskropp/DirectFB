/* systems.c, systems-specific routines                                 */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "global.h"

/* initialize buffer, call once before first getbits or showbits */

void Flush_Buffer32()
{
  int Incnt;

  ld->Bfr = 0;

  Incnt = ld->Incnt;
  Incnt -= 32;

  while (Incnt <= 24)
    {
      if (ld->Rdptr >= ld->Rdbfr+2048)
        Fill_Buffer();
      ld->Bfr |= *ld->Rdptr++ << (24 - Incnt);
      Incnt += 8;
    }

  ld->Incnt = Incnt;
}


unsigned int Get_Bits32()
{
  unsigned int l;

  l = Show_Bits(32);
  Flush_Buffer32();

  return l;
}

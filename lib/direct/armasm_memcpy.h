/*
 * ARM memcpy asm replacement.
 *
 * Copyright (C) 2009 Bluush Dev Team.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ARMASM_MEMCPY_H__
#define __ARMASM_MEMCPY_H__

#if USE_ARMASM && !WORDS_BIGENDIAN

void *direct_armasm_memcpy          ( void *dest, const void *src, size_t n);

#endif /* USE_ARMASM && !WORDS_BIGENDIAN */

#endif /* __ARMASM_MEMCPY_H__ */
 

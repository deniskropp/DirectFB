/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

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

#ifndef __GLES2_GFXDRIVER_H__
#define __GLES2_GFXDRIVER_H__

#ifdef GLES2_MESA
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define GLES2_USE_FBO

#else
#include <GLES2/gl2.h>
#endif

typedef enum {
     GLES2BF_UPDATE_TARGET  = 0x00000001,
     GLES2BF_UPDATE_TEXTURE = 0x00000002,
} GLES2BufferFlags;

typedef enum {
     GLES2VA_POSITIONS      = 0,
     GLES2VA_TEXCOORDS      = 1,
} GLES2VertexAttribs;

typedef struct {
     int                      magic;

     // Update flags for OpenGLES2 driver
     GLES2BufferFlags         flags;

     // Texture object bound to buffer
     GLuint                   texture;
} GLES2BufferData;

/*
 * A GLES shader program object and locations of various uniform variables.
 */
typedef struct {
     GLuint obj;          // the program object
     GLint  dfbScale;     // location of scale factors to clip coordinates
     GLint  dfbROMatrix;  // location of DFB Render Options matrix
     GLint  dfbMVPMatrix; // location of model-view-projection matrix
     GLint  dfbColor;     // location of global RGBA color
     GLint  dfbColorkey;  // location of colorkey RGB color
     GLint  dfbTexScale;  // location of scale factors to normalized tex coords
     GLint  dfbSampler;   // location of 2D texture sampler
     char  *name;         // program object name for debugging
     int    v_flags;      // validation flags
} GLES2ProgramInfo;

/*
 * Shader program indices.  For now there are 8 programs: draw_program,
 * blit_program, blit_colorkey_program, and blit_premultiply_program, plus
 * variants that use a full matrix multiply to support DFB's Render Options
 * matrix.
 */
typedef enum {
     GLES2_DRAW                 =  0,
     GLES2_DRAW_MAT             =  1,
     GLES2_BLIT                 =  2,
     GLES2_BLIT_MAT             =  3,
     GLES2_BLIT_COLORKEY        =  4,
     GLES2_BLIT_COLORKEY_MAT    =  5,
     GLES2_BLIT_PREMULTIPLY     =  6,
     GLES2_BLIT_PREMULTIPLY_MAT =  7,
     GLES2_NUM_PROGRAMS         =  8,
     GLES2_INVALID_PROGRAM      =  9
} GLES2ProgramIndex;

typedef struct {
     /*
      * Program objects and uniform variable locations.  These are shared
      * across all EGL contexts.
      */
     GLES2ProgramInfo         progs[GLES2_NUM_PROGRAMS];

     // The current program in use.
     GLES2ProgramIndex        prog_index;

     // The last program used.
     GLES2ProgramIndex        prog_last;
} GLES2DeviceData;

typedef struct {
#ifdef GLES2_USE_FBO
     GLuint                   fbo;
#endif


     DFBSurfaceBlittingFlags  blittingflags;

     // Flush every bunch of commands
     unsigned int             calls;
} GLES2DriverData;

DFBResult
gles2_init_shader_programs(GLES2DeviceData *dev);

#endif

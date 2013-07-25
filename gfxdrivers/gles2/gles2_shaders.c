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

#define DIRECT_ENABLE_DEBUG

#include <string.h>

#include <directfb.h>
#include <direct/debug.h>
#include <direct/messages.h>

#include "gles2_2d.h"
#include "gles2_gfxdriver.h"
#include "gles2_shaders.h"

D_DEBUG_DOMAIN(GLES2__2D, "GLES2/2D", "OpenGL ES2 2D Acceleration");

/*
 * Create program shader objects for sharing across all EGL contexts.
 */

static DFBBoolean
init_shader(GLuint prog_obj, const char *prog_src, GLenum type)
{
     char  *log;
     GLuint shader;
     GLint  status, log_length, char_count;
     GLint  sourceLen;

     sourceLen = strlen( prog_src );

     shader = glCreateShader(type);
     glShaderSource(shader, 1, (const char**)&prog_src, &sourceLen);
     glCompileShader(shader);

     glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
     if (status) {
          glAttachShader(prog_obj, shader);
          glDeleteShader(shader); // mark for deletion on detach
          return DFB_TRUE;
     }
     else {
          glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
          log = D_MALLOC(log_length);

          glGetShaderInfoLog(shader, log_length, &char_count, log);
          D_ERROR("GLES2/Driver: shader compilation failure:\n%s\n", log);
          D_FREE(log);

          glDeleteShader(shader);
          return DFB_FALSE;
     }
}

static DFBBoolean
init_program(GLuint prog_obj,
             char *vert_prog_name, const char *vert_prog_src,
             char *frag_prog_name, const char *frag_prog_src,
             DFBBoolean texcoords)
{
     char *log;
     GLint status, log_length, char_count;

     if (!init_shader(prog_obj, vert_prog_src, GL_VERTEX_SHADER)) {
          D_ERROR("GLES2/Driver: %s failed to compile!\n", vert_prog_name);
          return DFB_FALSE;
     }

     if (!init_shader(prog_obj, frag_prog_src, GL_FRAGMENT_SHADER)) {
          D_ERROR("GLES2/Driver: %s failed to compile!\n", frag_prog_name);
          return DFB_FALSE;
     }

     // Bind vertex positions to "dfbPos" vertex attribute slot.
     glBindAttribLocation(prog_obj, GLES2VA_POSITIONS, "dfbPos");

     if (texcoords)
          // Bind vertex texture coords to "dfbUV" vertex attribute slot.
          glBindAttribLocation(prog_obj, GLES2VA_TEXCOORDS, "dfbUV");

     // Link the program object and check for errors.
     glLinkProgram(prog_obj);
     glValidateProgram(prog_obj);
     glGetProgramiv(prog_obj, GL_LINK_STATUS, &status);

     if (status) {
          // Don't need the shader objects anymore.
          GLuint  shaders[2];
          GLsizei shader_count;

          glGetAttachedShaders(prog_obj, 2, &shader_count, shaders);

          glDetachShader(prog_obj, shaders[0]);
          glDetachShader(prog_obj, shaders[1]);

          return DFB_TRUE;
     }
     else {
          // Report errors.  Shader objects detached when program is deleted.
          glGetProgramiv(prog_obj, GL_INFO_LOG_LENGTH, &log_length);
          log = D_MALLOC(log_length);

          glGetProgramInfoLog(prog_obj, log_length, &char_count, log);
          D_ERROR("GLES2/Driver: shader program link failure:\n%s\n", log);
          D_FREE(log);

          return DFB_FALSE;
     }

     glUseProgram( prog_obj );

     return DFB_TRUE;
}

#define GET_UNIFORM_LOCATION(dev, index, name)				\
     do {								\
	  dev->progs[index].name =					\
	       glGetUniformLocation(dev->progs[index].obj, #name);	\
	  /*D_ASSERT(dev->progs[index].name != -1);*/			\
     } while (0)


DFBResult
gles2_init_shader_programs(GLES2DeviceData *dev)
{
     int i;
     GLuint prog;
     DFBBoolean status;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     /*
      * First initialize program info slots to invalid values.
      */
     for (i = 0; i < GLES2_NUM_PROGRAMS; i++) {
          dev->progs[i].obj          =  0;
          dev->progs[i].dfbScale     = -1;
          dev->progs[i].dfbROMatrix  = -1;
          dev->progs[i].dfbMVPMatrix = -1;
          dev->progs[i].dfbColor     = -1;
          dev->progs[i].dfbColorkey  = -1;
          dev->progs[i].dfbTexScale  = -1;
          dev->progs[i].dfbSampler   = -1;
          dev->progs[i].v_flags      =  0;
          dev->progs[i].name         = "invalid program";
     }

     /*
      * draw_program transforms a vertex by the current model-view-projection
      * matrix and applies a constant color to all fragments.
      */
     prog = glCreateProgram();
     status = init_program(prog, "draw_vert", draw_vert_src,
                           "draw_frag", draw_frag_src, DFB_FALSE);
     if (status) {
          dev->progs[GLES2_DRAW].obj = prog;
          dev->progs[GLES2_DRAW].name = "draw";

          GET_UNIFORM_LOCATION(dev, GLES2_DRAW, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_DRAW, dfbScale);

          D_DEBUG_AT(GLES2__2D, "-> created draw_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: draw_program init failed!\n");
          goto fail;
     }

     prog = glCreateProgram();
     status = init_program(prog, "draw_vert_mat", draw_vert_mat_src,
                           "draw_frag", draw_frag_src, DFB_FALSE);
     if (status) {
          dev->progs[GLES2_DRAW_MAT].obj = prog;
          dev->progs[GLES2_DRAW_MAT].name = "draw_mat";

          GET_UNIFORM_LOCATION(dev, GLES2_DRAW_MAT, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_DRAW_MAT, dfbROMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_DRAW_MAT, dfbMVPMatrix);

          D_DEBUG_AT(GLES2__2D, "-> created draw_mat_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: draw_mat_program init failed!\n");
          goto fail;
     }

     /*
      * blit_program transforms a vertex by the current model-view-projection
      * matrix, applies texture sample colors to fragments.
      */
     prog = glCreateProgram();
     status = init_program(prog, "blit_vert", blit_vert_src,
                           "blit_frag", blit_frag_src, DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT].obj = prog;
          dev->progs[GLES2_BLIT].name = "blit";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT, dfbScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_program init failed!\n");
          goto fail;
     }

     prog = glCreateProgram();
     status = init_program(prog, "blit_vert_mat", blit_vert_mat_src,
                           "blit_frag", blit_frag_src, DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_MAT].obj = prog;
          dev->progs[GLES2_BLIT_MAT].name = "blit_mat";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_MAT, dfbROMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_MAT, dfbMVPMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_MAT, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_MAT, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_MAT].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_mat_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_mat_program init failed!\n");
          goto fail;
     }

     /*
      * blit_color_program transforms a vertex by the current model-view-projection
      * matrix, applies texture sample colors to fragments, and modulates the
      * colors with a static RGBA color.  Modulation is effectively disabled
      * by setting static color components to 1.0.
      */
     prog = glCreateProgram();
     status = init_program(prog, "blit_color_vert", blit_vert_src,
                           "blit_color_frag", blit_color_frag_src, DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_COLOR].obj = prog;
          dev->progs[GLES2_BLIT_COLOR].name = "blit_color";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR, dfbScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_COLOR].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_color_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_color_program init failed!\n");
          goto fail;
     }

     prog = glCreateProgram();
     status = init_program(prog, "blit_color_vert_mat", blit_vert_mat_src,
                           "blit_color_frag", blit_color_frag_src, DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_COLOR_MAT].obj = prog;
          dev->progs[GLES2_BLIT_COLOR_MAT].name = "blit_color_mat";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR_MAT, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR_MAT, dfbROMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR_MAT, dfbMVPMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR_MAT, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLOR_MAT, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_COLOR_MAT].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_color_mat_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_color_mat_program init failed!\n");
          goto fail;
     }

     /*
      * blit_colorkey_program does the same as blit_program with the addition
      * of source color keying.  Shaders don't have access to destination
      * pixels so color keying can be on the source only.
      */
     prog = glCreateProgram();
     status = init_program(prog, "blit_vert", blit_vert_src,
                           "blit_colorkey_frag", blit_colorkey_frag_src,
                           DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_COLORKEY].obj = prog;
          dev->progs[GLES2_BLIT_COLORKEY].name = "blit_colorkey";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY, dfbScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY, dfbSampler);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY, dfbColorkey);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_COLORKEY].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_colorkey_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_colorkey_program init failed!\n");
          goto fail;
     }

     prog = glCreateProgram();
     status = init_program(prog, "blit_vert_mat", blit_vert_mat_src,
                           "blit_colorkey_frag", blit_colorkey_frag_src,
                           DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_COLORKEY_MAT].obj = prog;
          dev->progs[GLES2_BLIT_COLORKEY_MAT].name = "blit_colorkey_mat";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbROMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbMVPMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbSampler);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_COLORKEY_MAT, dfbColorkey);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_COLORKEY_MAT].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_colorkey_mat_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_colorkey_mat_program init failed!\n");
          goto fail;
     }

     /*
      * blit_premultiply_program does the same as blit_program with the
      * addition of pre-multiplication of the source frag color by the source
      * frag alpha.  Shaders don't have access to destination pixels so
      * pre-multiplication can be on the source only.
      */
     prog = glCreateProgram();
     status = init_program(prog, "blit_vert", blit_vert_src,
                           "blit_premultiply_frag", blit_premultiply_frag_src,
                           DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_PREMULTIPLY].obj = prog;
          dev->progs[GLES2_BLIT_PREMULTIPLY].name = "blit_premultiply";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY, dfbScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_PREMULTIPLY].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_premultiply_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_premultiply_program init failed!\n");
          goto fail;
     }

     prog = glCreateProgram();
     status = init_program(prog, "blit_vert_mat", blit_vert_mat_src,
                           "blit_premultiply_frag", blit_premultiply_frag_src,
                           DFB_TRUE);
     if (status) {
          dev->progs[GLES2_BLIT_PREMULTIPLY_MAT].obj = prog;
          dev->progs[GLES2_BLIT_PREMULTIPLY_MAT].name = "blit_premultiply_mat";

          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY_MAT, dfbColor);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY_MAT, dfbROMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY_MAT, dfbMVPMatrix);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY_MAT, dfbTexScale);
          GET_UNIFORM_LOCATION(dev, GLES2_BLIT_PREMULTIPLY_MAT, dfbSampler);

          // For now we always use texture unit 0 (GL_TEXTURE0).
          glUniform1i(dev->progs[GLES2_BLIT_PREMULTIPLY_MAT].dfbSampler, 0);

          D_DEBUG_AT(GLES2__2D, "-> created blit_premultiply_mat_program\n");
     }
     else {
          D_ERROR("GLES2/Driver: blit_premultiply_mat_program init failed!\n");
          goto fail;
     }

     // No program is yet in use.
     dev->prog_index = GLES2_INVALID_PROGRAM;
     dev->prog_last  = GLES2_INVALID_PROGRAM;

     return DFB_OK;

     fail:
     // Delete all program objects.  glDeleteProgram() will ignore object id 0.
     for (i = 0; i < GLES2_NUM_PROGRAMS; i++)
          glDeleteProgram(dev->progs[i].obj);

     return DFB_INIT;
}

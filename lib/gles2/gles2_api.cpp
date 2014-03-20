/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdio.h>
#include <string.h>


#define DFBEGL_ENABLE_MANGLE

#include <egl/dfbegl.h>

//#include <GLES2/gles2dfbunmangle.h>
#include <GLES2/gl2.h>

#include <direct/ToString.h>




extern "C" {

void
glActiveTexture (GLenum texture)
{
}

void
glAttachShader (GLuint program, GLuint shader)
{
}

void
glBindAttribLocation (GLuint program, GLuint index, const GLchar* name)
{
}

void
glBindBuffer (GLenum target, GLuint buffer)
{
}

void
glBindFramebuffer (GLenum target, GLuint framebuffer)
{
}

void
glBindRenderbuffer (GLenum target, GLuint renderbuffer)
{
}

void
glBindTexture (GLenum target, GLuint texture)
{
}

void
glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
glBlendEquation ( GLenum mode )
{
}

void
glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha)
{
}

void
glBlendFunc (GLenum sfactor, GLenum dfactor)
{
}

void
glBlendFuncSeparate (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
}

void
glBufferData (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
}

void
glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
}

GLenum
glCheckFramebufferStatus (GLenum target)
{
     return GL_FRAMEBUFFER_UNSUPPORTED;
}

void
glClear (GLbitfield mask)
{
}

void
glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
glClearDepthf (GLclampf depth)
{
}

void
glClearStencil (GLint s)
{
}

void
glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
}

void
glCompileShader (GLuint shader)
{
}

void
glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data)
{
}

void
glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* data)
{
}

void
glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
}

void
glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
}

GLuint
glCreateProgram (void)
{
     return GL_NONE;
}

GLuint
glCreateShader (GLenum type)
{
     return GL_NONE;
}

void
glCullFace (GLenum mode)
{
}

void
glDeleteBuffers (GLsizei n, const GLuint* buffers)
{
}

void
glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers)
{
}

void
glDeleteProgram (GLuint program)
{
}

void
glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers)
{
}

void
glDeleteShader (GLuint shader)
{
}

void
glDeleteTextures (GLsizei n, const GLuint* textures)
{
}

void
glDepthFunc (GLenum func)
{
}

void
glDepthMask (GLboolean flag)
{
}

void
glDepthRangef (GLclampf zNear, GLclampf zFar)
{
}

void
glDetachShader (GLuint program, GLuint shader)
{
}

void
glDisable (GLenum cap)
{
}

void
glDisableVertexAttribArray (GLuint index)
{
}

void
glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
}

void
glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices)
{
}

void
glEnable (GLenum cap)
{
}

void
glEnableVertexAttribArray (GLuint index)
{
}

void
glFinish (void)
{
}

void
glFlush (void)
{
}

void
glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
}

void
glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
}

void
glFrontFace (GLenum mode)
{
}

void
glGenBuffers (GLsizei n, GLuint* buffers)
{
}

void
glGenerateMipmap (GLenum target)
{
}

void
glGenFramebuffers (GLsizei n, GLuint* framebuffers)
{
}

void
glGenRenderbuffers (GLsizei n, GLuint* renderbuffers)
{
}

void
glGenTextures (GLsizei n, GLuint* textures)
{
}

void
glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
}

void
glGetActiveUniform (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
}

void
glGetAttachedShaders (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
}

int
glGetAttribLocation (GLuint program, const GLchar* name)
{
     return -1;
}

void
glGetBooleanv (GLenum pname, GLboolean* params)
{
}

void
glGetBufferParameteriv (GLenum target, GLenum pname, GLint* params)
{
}

GLenum
glGetError (void)
{
     return GL_INVALID_OPERATION;
}

void
glGetFloatv (GLenum pname, GLfloat* params)
{
}

void
glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
}

void
glGetIntegerv (GLenum pname, GLint* params)
{
}

void
glGetProgramiv (GLuint program, GLenum pname, GLint* params)
{
}

void
glGetProgramInfoLog (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
}

void
glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint* params)
{
}

void
glGetShaderiv (GLuint shader, GLenum pname, GLint* params)
{
}

void
glGetShaderInfoLog (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
}

void
glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
}

void
glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
}

const GLubyte*
glGetString (GLenum name)
{
     return NULL;
}

void
glGetTexParameterfv (GLenum target, GLenum pname, GLfloat* params)
{
}

void
glGetTexParameteriv (GLenum target, GLenum pname, GLint* params)
{
}

void
glGetUniformfv (GLuint program, GLint location, GLfloat* params)
{
}

void
glGetUniformiv (GLuint program, GLint location, GLint* params)
{
}

int
glGetUniformLocation (GLuint program, const GLchar* name)
{
     return -1;
}

void
glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params)
{
}

void
glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params)
{
}

void
glGetVertexAttribPointerv (GLuint index, GLenum pname, GLvoid** pointer)
{
}

void
glHint (GLenum target, GLenum mode)
{
}

GLboolean
glIsBuffer (GLuint buffer)
{
     return false;
}

GLboolean
glIsEnabled (GLenum cap)
{
     return false;
}

GLboolean
glIsFramebuffer (GLuint framebuffer)
{
     return false;
}

GLboolean
glIsProgram (GLuint program)
{
     return false;
}

GLboolean
glIsRenderbuffer (GLuint renderbuffer)
{
     return false;
}

GLboolean
glIsShader (GLuint shader)
{
     return false;
}

GLboolean
glIsTexture (GLuint texture)
{
     return false;
}

void
glLineWidth (GLfloat width)
{
}

void
glLinkProgram (GLuint program)
{
}

void
glPixelStorei (GLenum pname, GLint param)
{
}

void
glPolygonOffset (GLfloat factor, GLfloat units)
{
}

void
glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels)
{
}

void
glReleaseShaderCompiler (void)
{
}

void
glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
}

void
glSampleCoverage (GLclampf value, GLboolean invert)
{
}

void
glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
glShaderBinary (GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
}

void
glShaderSource (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)
{
}

void
glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
}

void
glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask)
{
}

void
glStencilMask (GLuint mask)
{
}

void
glStencilMaskSeparate (GLenum face, GLuint mask)
{
}

void
glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
}

void
glStencilOpSeparate (GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
}

void
glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels)
{
}

void
glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
}

void
glTexParameterfv (GLenum target, GLenum pname, const GLfloat* params)
{
}

void
glTexParameteri (GLenum target, GLenum pname, GLint param)
{
}

void
glTexParameteriv (GLenum target, GLenum pname, const GLint* params)
{
}

void
glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
{
}

void
glUniform1f (GLint location, GLfloat x)
{
}

void
glUniform1fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
glUniform1i (GLint location, GLint x)
{
}

void
glUniform1iv (GLint location, GLsizei count, const GLint* v)
{
}

void
glUniform2f (GLint location, GLfloat x, GLfloat y)
{
}

void
glUniform2fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
glUniform2i (GLint location, GLint x, GLint y)
{
}

void
glUniform2iv (GLint location, GLsizei count, const GLint* v)
{
}

void
glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z)
{
}

void
glUniform3fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
glUniform3i (GLint location, GLint x, GLint y, GLint z)
{
}

void
glUniform3iv (GLint location, GLsizei count, const GLint* v)
{
}

void
glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
glUniform4fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w)
{
}

void
glUniform4iv (GLint location, GLsizei count, const GLint* v)
{
}

void
glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
glUseProgram (GLuint program)
{
}

void
glValidateProgram (GLuint program)
{
}

void
glVertexAttrib1f (GLuint indx, GLfloat x)
{
}

void
glVertexAttrib1fv (GLuint indx, const GLfloat* values)
{
}

void
glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y)
{
}

void
glVertexAttrib2fv (GLuint indx, const GLfloat* values)
{
}

void
glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
}

void
glVertexAttrib3fv (GLuint indx, const GLfloat* values)
{
}

void
glVertexAttrib4f (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
glVertexAttrib4fv (GLuint indx, const GLfloat* values)
{
}

void
glVertexAttribPointer (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
}

void
glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
}



}


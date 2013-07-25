/* -*- mode: c; tab-width: 8; -*- */
/* vi: set sw=4 ts=8: */
/* Reference version of egl.h for EGL 1.4.
 * $Revision: 9356 $ on $Date: 2009-10-21 02:52:25 -0700 (Wed, 21 Oct 2009) $
 */

/*
** Copyright (c) 2007-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#ifndef __egldfbunmangle_h_
#define __egldfbunmangle_h_

#undef eglGetError

#undef eglGetDisplay
#undef eglInitialize
#undef eglTerminate

#undef eglQueryString

#undef eglGetConfigs
#undef eglChooseConfig
#undef eglGetConfigAttrib

#undef eglCreateWindowSurface
#undef eglCreatePbufferSurface
#undef eglCreatePixmapSurface
#undef eglDestroySurface
#undef eglQuerySurface

#undef eglBindAPI
#undef eglQueryAPI

#undef eglWaitClient

#undef eglReleaseThread

#undef eglCreatePbufferFromClientBuffer

#undef eglSurfaceAttrib
#undef eglBindTexImage
#undef eglReleaseTexImage


#undef eglSwapInterval


#undef eglCreateContext
#undef eglDestroyContext
#undef eglMakeCurrent

#undef eglGetCurrentContext
#undef eglGetCurrentSurface
#undef eglGetCurrentDisplay
#undef eglQueryContext

#undef eglWaitGL
#undef eglWaitNative
#undef eglSwapBuffers
#undef eglCopyBuffers

#undef eglGetProcAddress


#endif /* __egldfbunmangle_h_ */

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

/*
 * Transform input `dfbPos' 2D positions by scale and offset to get GLES clip
 * coordinates.  This is the equivalent of the following calls in standard
 * OpenGL, where `w' and `h' are the width and height of the destination
 * DFB surface:
 *
 * glLoadIdentity();
 * glOrtho(0, w, h, 0, -1, 1);
 *
 * dfbScale.x and dfbScale.y are set to 2/w and -2/h before drawing.
 * TODO: is 2/(w-1) and -2/(h-1) correct?
 */
static const char *draw_vert_src = " \
attribute vec2 dfbPos; \
uniform vec2   dfbScale; \
void main (void) \
{ \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0; \
     gl_Position.y = dfbScale.y * dfbPos.y + 1.0; \
     gl_Position.z = 0.0; \
     gl_Position.w = 1.0; \
}";


/*
 * Transform input `dfbPos' 2D positions by the surface render options matrix
 * before transforming to GLES clip coordinates.  This is done with 3x3 matrix
 * concatenation since the RO matrix supports 2D non-affine transforms and the
 * x and y components need to be divided by the w component.
 */
static const char *draw_vert_mat_src = " \
attribute vec2 dfbPos; \
uniform mat3   dfbMVPMatrix; \
uniform mat3   dfbROMatrix; \
void main (void) \
{ \
     vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0); \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z); \
}";


/*
 * Draw fragment in a constant color.
 */
static const char *draw_frag_src = " \
uniform vec4 dfbColor; \
void main (void) \
{ \
     gl_FragColor = dfbColor; \
}";


/*
 * This is the same as draw_vert_src with the addition of texture coords.
 * The ARB_texture_rectangle extension isn't in GLES2, so we need to
 * normalize the texture coordinates to [0..1].  dfbTexScale.x and
 * dfbTexScale.y are set to 1/w and -1/h respectively, where `w' and `h'
 * are the width and height of the source DFB surface.
 */
static const char *blit_vert_src = " \
attribute vec2   dfbPos; \
attribute vec2 dfbUV; \
uniform vec2     dfbScale; \
uniform vec2   dfbTexScale; \
varying vec2   varTexCoord; \
void main (void) \
{ \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0; \
     gl_Position.y = dfbScale.y * dfbPos.y + 1.0; \
     gl_Position.z = 0.0; \
     gl_Position.w = 1.0; \
     varTexCoord.s = dfbTexScale.x * dfbUV.x; \
     varTexCoord.t = dfbTexScale.y * dfbUV.y + 1.0; \
}";


/*
 * This is the same as draw_vert_mat_src with the addition of texture coords.
 */
static const char *blit_vert_mat_src = " \
attribute vec2   dfbPos; \
attribute vec2 dfbUV; \
uniform mat3     dfbMVPMatrix; \
uniform mat3     dfbROMatrix; \
uniform vec2   dfbTexScale; \
varying vec2   varTexCoord; \
void main (void) \
{ \
     vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0); \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z); \
     varTexCoord.s = dfbTexScale.x * dfbUV.x; \
     varTexCoord.t = dfbTexScale.y * dfbUV.y + 1.0; \
}";


/*
 * Sample texture and modulate by static color.
 */
static const char *blit_frag_src = " \
uniform sampler2D    dfbSampler; \
uniform vec4    dfbColor; \
varying vec2 varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord); \
     gl_FragColor *= dfbColor; \
}";


/*
 * Using `discard' for colorkey can stall the pipe on some hardware.
 * Use an alpha of 0 with blending func (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
 * enabled instead.  This means that colorkey is incompatible with any existing
 * blend func as well as premultiply (which only makes sense with blending).
 * This shouldn't be a problem; if arbitrary blending is being used, then
 * colorkey is redundant.
 *
 * TODO: see if all(equal(gl_FragColor.rgb, dfbColorkey)) is a sufficient test.
 */
static const char *blit_colorkey_frag_src = " \
const float       e = 1.0 / 128.0; \
uniform sampler2D dfbSampler; \
uniform vec4      dfbColor; \
uniform vec3      dfbColorkey; \
varying vec2   varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord); \
     if (abs(gl_FragColor.r - dfbColorkey.r) < e && \
         abs(gl_FragColor.g - dfbColorkey.g) < e && \
	 abs(gl_FragColor.b - dfbColorkey.b) < e) { \
	  gl_FragColor.a = 0.0; \
     } else { \
	  gl_FragColor.a = 1.0; \
     } \
     gl_FragColor *= dfbColor; \
}";

/* 
 * Perform an alpha pre-multiply of source frag color with source alpha after
 * sampling and modulation.
 */
static const char *blit_premultiply_frag_src = " \
uniform sampler2D    dfbSampler; \
uniform vec4    dfbColor; \
varying vec2 varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord); \
     gl_FragColor *= dfbColor; \
     gl_FragColor.rgb *= gl_FragColor.a; \
}";

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

#ifdef GLES2_PVR2D
#define LOWP        "lowp"
#define MEDIUMP     "mediump"
#define HIGHP       "highp"
#define GL_POS_Y_OP "+"
#else
#define LOWP        ""
#define MEDIUMP     ""
#define HIGHP       ""
#define GL_POS_Y_OP "-"
#endif

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
attribute "HIGHP" vec2 dfbPos; \
uniform "HIGHP" vec3   dfbScale; \
void main (void) \
{ \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0; \
     gl_Position.y = dfbScale.y * dfbPos.y + dfbScale.z; \
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
attribute "HIGHP" vec2 dfbPos; \
uniform "HIGHP" mat3   dfbMVPMatrix; \
uniform "HIGHP" mat3   dfbROMatrix; \
void main (void) \
{ \
     "HIGHP" vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0); \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z); \
}";


/*
 * Draw fragment in a constant color.
 */
static const char *draw_frag_src = " \
uniform "LOWP" vec4 dfbColor; \
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
attribute "HIGHP" vec2   dfbPos; \
attribute "MEDIUMP" vec2 dfbUV; \
uniform "HIGHP"   vec3   dfbScale; \
uniform "MEDIUMP" vec2   dfbTexScale; \
varying "LOWP"    vec2   varTexCoord; \
void main (void) \
{ \
     gl_Position.x = dfbScale.x * dfbPos.x - 1.0; \
     gl_Position.y = dfbScale.y * dfbPos.y + dfbScale.z; \
     gl_Position.z = 0.0; \
     gl_Position.w = 1.0; \
     varTexCoord.s = dfbTexScale.x * dfbUV.x; \
     varTexCoord.t = dfbTexScale.y * dfbUV.y; \
}";


/*
 * This is the same as draw_vert_mat_src with the addition of texture coords.
 */
static const char *blit_vert_mat_src = " \
attribute "HIGHP" vec2   dfbPos; \
attribute "MEDIUMP" vec2 dfbUV; \
uniform "HIGHP"   mat3   dfbMVPMatrix; \
uniform "HIGHP"   mat3   dfbROMatrix; \
uniform "MEDIUMP" vec2   dfbTexScale; \
varying "LOWP"    vec2   varTexCoord; \
void main (void) \
{ \
     "HIGHP" vec3 pos = dfbMVPMatrix * dfbROMatrix * vec3(dfbPos, 1.0); \
     gl_Position = vec4(pos.x, pos.y, 0.0, pos.z); \
     varTexCoord.s = dfbTexScale.x * dfbUV.x; \
     varTexCoord.t = dfbTexScale.y * dfbUV.y; \
}";


/*
 * Sample texture and modulate by static color.
 */
static const char *blit_frag_src = " \
uniform sampler2D    dfbSampler; \
varying "LOWP" vec2  varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord); \
}";


/*
 * Sample texture and modulate by static color.
 */
static const char *blit_color_frag_src = " \
uniform sampler2D    dfbSampler; \
uniform "LOWP" vec4  dfbColor; \
varying "LOWP" vec2  varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord) * dfbColor; \
}";


/*
 * Source Color Keying.
 */
static const char *blit_colorkey_frag_src = " \
uniform sampler2D dfbSampler; \
uniform "LOWP" vec4   dfbColor; \
uniform        ivec3  dfbColorkey; \
varying "LOWP" vec2   varTexCoord; \
void main (void) \
{ \
     "HIGHP" vec4 value = texture2D(dfbSampler, varTexCoord); \
\
     int r = int(value.r*255.0+0.5); \
     int g = int(value.g*255.0+0.5); \
     int b = int(value.b*255.0+0.5); \
\
     if (r == dfbColorkey.x && g == dfbColorkey.y && b == dfbColorkey.z) \
        discard; \
\
     gl_FragColor = value * dfbColor; \
}";

/* 
 * Perform an alpha pre-multiply of source frag color with source alpha after
 * sampling and modulation.
 */
static const char *blit_premultiply_frag_src = " \
uniform sampler2D    dfbSampler; \
uniform "LOWP" vec4  dfbColor; \
varying "LOWP" vec2  varTexCoord; \
void main (void) \
{ \
     gl_FragColor = texture2D(dfbSampler, varTexCoord); \
     gl_FragColor *= dfbColor; \
     gl_FragColor.rgb *= gl_FragColor.a; \
}";

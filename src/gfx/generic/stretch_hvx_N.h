#if UPDOWN == 1
#define FUNC_NAME_(K,P,F)      FUNC_NAME(up,K,P,F)
#else
#define FUNC_NAME_(K,P,F)      FUNC_NAME(down,K,P,F)
#endif


/* NONE */
static void FUNC_NAME_(_,_,DST_FORMAT)
#include STRETCH_HVX_N_H

/* SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,_,DST_FORMAT)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* PROTECT */
#define KEY_PROTECT      ctx->protect
static void FUNC_NAME_(_,P,DST_FORMAT)
#include STRETCH_HVX_N_H

/* PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,P,DST_FORMAT)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT


/* INDEXED */
#define SOURCE_LOOKUP(x) ((uN*)ctx->colors)[x]
#define SOURCE_TYPE      u8
static void FUNC_NAME_(_,_,DSPF_LUT8)
#include STRETCH_HVX_N_H

/* INDEXED SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,_,DSPF_LUT8)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* INDEXED PROTECT */
#define KEY_PROTECT      ctx->protect
static void FUNC_NAME_(_,P,DSPF_LUT8)
#include STRETCH_HVX_N_H

/* INDEXED PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,P,DSPF_LUT8)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE


/* FIXME: DST_FORMAT == DSPF_RGB16 doesn't work */
#ifdef FORMAT_RGB16
/* ARGB4444 */
#define SOURCE_LOOKUP(x)   PIXEL_RGB16( (((x) & 0x0f00) >> 4) | (((x) & 0x0f00) >> 8),    \
                                        (((x) & 0x00f0)     ) | (((x) & 0x00f0) >> 4),    \
                                        (((x) & 0x000f) << 4) | (((x) & 0x000f)     ) )
static void FUNC_NAME_(_,_,DSPF_ARGB4444)
#include STRETCH_HVX_N_H

/* ARGB4444 SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,_,DSPF_ARGB4444)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* ARGB4444 PROTECT */
#define KEY_PROTECT      ctx->protect
static void FUNC_NAME_(_,P,DSPF_ARGB4444)
#include STRETCH_HVX_N_H

/* ARGB4444 PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,P,DSPF_ARGB4444)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
#endif


/* FIXME: DST_FORMAT == DSPF_ARGB4444 doesn't work */
#ifdef FORMAT_ARGB4444
/* RGB16 */
#define SOURCE_LOOKUP(x)   PIXEL_ARGB4444( 0xff,                   \
                                           (((x) & 0xf800) >> 8),  \
                                           (((x) & 0x07e0) >> 3),  \
                                           (((x) & 0x001f) << 3) )
static void FUNC_NAME_(_,_,DSPF_RGB16)
#include STRETCH_HVX_N_H

/* RGB16 SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,_,DSPF_RGB16)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* RGB16 PROTECT */
#define KEY_PROTECT      ctx->protect
static void FUNC_NAME_(_,P,DSPF_RGB16)
#include STRETCH_HVX_N_H

/* RGB16 PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void FUNC_NAME_(K,P,DSPF_RGB16)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
#endif

#undef FUNC_NAME_


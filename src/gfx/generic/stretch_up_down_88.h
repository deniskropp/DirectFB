#define POINT_0               hfraq
#define LINE_0                vfraq
#define POINT_TO_RATIO(p,ps)  ( (((((p)) & 0x3ffff) ? : 0x40000) << 8) / (ps) )
#define LINE_TO_RATIO(l,ls)   ( (((((l)) & 0x3ffff) ? : 0x40000) << 8) / (ls) )

#define POINT_L(p,ps)  ( (((p)-1) >> 18) - 1 )
#define POINT_R(p,ps)  ( (((p)-1) >> 18) )

#define LINE_T(l,ls)  ( (((l)-1) >> 18) - 1 )
#define LINE_B(l,ls)  ( (((l)-1) >> 18) )

static void FUNC_NAME(down)( void       *dst,
                             int         dpitch,
                             const void *src,
                             int         spitch,
                             int         width,
                             int         height,
                             int         dst_width,
                             int         dst_height,
                             DFBRegion  *clip )
{
#include "stretch_hvx_88.h"
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#define POINT_0               0
#define LINE_0                0
#define POINT_TO_RATIO(p,ps)  ( ((p) & 0x3ffff) >> (18-8) )
#define LINE_TO_RATIO(l,ls)   ( ((l) & 0x3ffff) >> (18-8) )

#define POINT_L(p,ps)  ( (((p)) >> 18) )
#define POINT_R(p,ps)  ( (((p)) >> 18) + 1 )

#define LINE_T(l,ls)  ( (((l)) >> 18) )
#define LINE_B(l,ls)  ( (((l)) >> 18) + 1 )

static void FUNC_NAME(up)( void       *dst,
                           int         dpitch,
                           const void *src,
                           int         spitch,
                           int         width,
                           int         height,
                           int         dst_width,
                           int         dst_height,
                           DFBRegion  *clip )
{
#include "stretch_hvx_88.h"
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B


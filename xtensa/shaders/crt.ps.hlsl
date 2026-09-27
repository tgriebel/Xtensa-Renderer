// This is modified from the original code by Timothy Lottes. Original header as follows:
//
// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER
//
//   by Timothy Lottes
//
// This is more along the style of a really good CGA arcade monitor.
// With RGB inputs instead of NTSC.
// The shadow mask example has the mask rotated 90 degrees for less chromatic aberration.
//
// Left it unoptimized to show the theory behind the algorithm.
//
// It is an example what I personally would want as a display option for pixel art games.
// Please take and use, change, or whatever.
//

#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

#include "crtCore.h"


psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;

    const uint materialId = pushConstants.materialId;
    const uint textureId0 = materials[ materialId ].textureId[ 0 ];

    float4 outColor;
    outColor.rgb = CrtShade( globalTextures[ textureId0 ], bilinearSamplerClampEdge, input.uv0.xy );
    outColor.a = 1.0;

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( normalize( input.normal ) + float3( 1.0f, 1.0f, 1.0f ) );
    outColor1.a = 1.0f;

    output.outColor = outColor;
    output.outColor1 = outColor1;
#else
    output.outColor = outColor;
#endif

	return output;
}

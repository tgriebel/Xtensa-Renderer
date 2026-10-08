#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )


psOutput_t PSMain( vsToPsInterpolators input )
{
	const uint materialId = pushConstants.materialId;
	const gpuMaterial_t material = materials[ materialId ];

	const float2 uv[ 2 ] = { input.uv0, input.uv1 };
	const float4 sdfSample = SampleTexture( globalTextures, bilinearSamplerClampEdge, material, GGX_ALBEDO_MAP_SLOT, uv );

	float4 outColor;
	outColor.rgb = sdfSample.rrr;
	outColor.a = material.opacity;

	psOutput_t output = (psOutput_t)0;

#ifdef USE_MRT
	float4 outColor1;
	outColor1.rgb = float3( 0.0f, 0.0f, 1.0f );
	outColor1.a = 1.0f;

	output.outColor = ClampColorFp16( outColor );
	output.outColor1 = ClampColorFp16( outColor1 );
#else
	output.outColor = ClampColorFp16( outColor );
#endif

	return output;
}

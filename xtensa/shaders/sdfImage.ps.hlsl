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

	return OutputColor( outColor );
}

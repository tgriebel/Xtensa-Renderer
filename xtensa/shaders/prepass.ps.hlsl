#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

#ifdef USE_MRT
#define VELOCITY_IN_PREPASS
#define NORMAL_IN_PREPASS
#endif

psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    
    const uint materialId = pushConstants.materialId;
    const uint viewId = pushConstants.viewId;
    
	output.outColor = float4( 1.0f, 0.0f, 0.0f, 1.0f );
    
#ifdef VELOCITY_IN_PREPASS
    const float2 current = input.clipPosition.xy / input.clipPosition.w;
    const float2 previous = input.prevClipPosition.xy / input.prevClipPosition.w;

    const float2 velocity = ( current - previous ) * 0.5f;
    
    output.outColor1.rg = velocity.xy;
#endif
    
#ifdef NORMAL_IN_PREPASS
	const gpuMaterial_t material = materials[ materialId ];

	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );

	const float2 uv[ 2 ] = { input.uv0.xy, input.uv1.xy };

	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );

	if( isTextured ) {
		normalSample = SampleTextureNormal( globalTextures, bilinearSamplerWrap, material, GGX_NORMAL_MAP_SLOT, uv );
    }
    
    const float3 normalWS = ComputeNormalWS( normalSample, input.tangent, input.bitangent, input.TBN2 );
    const float3 normalVS = mul( views[ viewId ].viewMat, float4( normalWS, 0.0f ) ).xyz;
    
    output.outColor1.ba = OctEncode( normalVS );
#endif

#ifdef USE_MRT
    output.outColor2 = float4( input.worldPosition.xyz, 1.0f );
#endif

	return output;
}

#include "globals.h"
#include "util.h"

PS_LAYOUT_STANDARD( Texture2D )

#include "lighting.h"


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 pixelUV = 0.5f * ( input.clipPosition.xy / input.clipPosition.w ) + 0.5f;

    const uint materialId = pushConstants.materialId;
    const uint viewId = pushConstants.viewId;

    const gpuView_t view = views[ viewId ];
    const gpuMaterial_t material = materials[ materialId ];

    const surfaceInput_t surfaceInput = CalculateSurfaceInput( globals, view, material, input );

    const float F0 = pow( ( material.ior - 1.0f ) / ( material.ior + 1.0f ), 2.0f );
    const float fresnel = F0 + ( 1.0f - F0 ) * pow( 1.0f - saturate( surfaceInput.NoV ), 5.0f );

    // Perturb the screen UV for fake refraction
    const float3 viewNormal = normalize( mul( (float3x3)view.viewMat, surfaceInput.N ) );
    const float2 distortedUV = pixelUV + viewNormal.xy * material.transmissionFactor;

    Texture2D sceneColorMips = localTextures[ 3 ];
    Texture2D rtReflectionImage = localTextures[ 4 ];

    uint sceneWidth, sceneHeight, sceneMipCount;
    sceneColorMips.GetDimensions( 0, sceneWidth, sceneHeight, sceneMipCount );
    const float mipLevel = surfaceInput.roughness * ( sceneMipCount - 1 );

    const float3 transmitted = sceneColorMips.SampleLevel( bilinearSamplerClampEdge, distortedUV, mipLevel ).rgb * material.Tf;
    const float3 reflected = rtReflectionImage.Sample( bilinearSamplerClampEdge, pixelUV ).rgb;

    float4 outColor;
    outColor.rgb = lerp( transmitted, reflected, fresnel );
    outColor.a = material.opacity;

    psOutput_t output = (psOutput_t)0;
    output.outColor = ClampColorFp16( outColor );

    return output;
}

#include "globals.h"
#include "util.h"
#include "brdf.h"

PS_LAYOUT_STANDARD( Texture2D ) // Must come before lighting.h

#include "lighting.h"


float3 ApplyShadow( const uint shadowViewId, float3 worldPosition, const float3 Lo, Texture2D shadowMap, SamplerComparisonState samp )
{
	float shadowing = 1.0f; // Assumes spot-light, should be 0.0f for normal lights

	if ( shadowViewId != 0xFF )
	{
		const gpuView_t shadowView = views[shadowViewId];

		const float shadowBias = 0.001f;

        // Light Space Position
		float4 lsPosition = mul( mul( shadowView.projMat, shadowView.viewMat ), float4( worldPosition.xyz, 1.0f ) );
		lsPosition.xyz /= lsPosition.w;

		lsPosition.z -= shadowBias;

		const float2 ndc = 0.5f * lsPosition.xy + 0.5f;

		const float spotRadius = 0.3f;
		const bool withinSpotlight = ( length(ndc.xy - float2( 0.5f, 0.5f ) ) < spotRadius ); // Similar to an SDF. Distance from center below a threshold

		if ( withinSpotlight )
		{
            const float shadowMapSample = shadowMap.SampleCmpLevelZero( samp, ndc.xy, lsPosition.z );

			shadowing = shadowMapSample * globals.shadowParms.w;
		}
		else
		{
			shadowing = 0.0f;
		}
	}
    return ( shadowing * Lo );
}


float3 EvaluateDiffuseAmbient( TextureCube diffuseIBL, const surfaceInput_t surfaceInput )
{
    float3 kS = surfaceInput.F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - surfaceInput.metallic;

    float3 ambientHemisphere = AMBIENT.rgb * surfaceInput.albedo; // Overriden by IBL if enabled
    if ( globals.useDiffuseIBL )
    {
        const float3 irradiance = diffuseIBL.Sample( bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
        ambientHemisphere = irradiance * surfaceInput.albedo;
        
        // Based on CC IBL note in: https://google.github.io/filament/Filament.md.html
        if ( surfaceInput.useClearCoat )
        {
            const float3 clearcoatF0 = float3( 0.04f, 0.04f, 0.04f );
            const float ccNoV = saturate( dot( surfaceInput.ccNormal, surfaceInput.V ) );
            float3 clearCoatF = surfaceInput.ccStrength * F_SchlickRoughness( ccNoV, clearcoatF0, surfaceInput.ccRoughness );
            kD *= ( 1.0f - clearCoatF );
        }
        
        if ( surfaceInput.useSheen )
        {
            const float sheenMax = max( surfaceInput.sheenColor.r, max( surfaceInput.sheenColor.g, surfaceInput.sheenColor.b ) );
            const float sheenScaling = 1.0f - sheenMax * 0.157f; // TODO: replace magic number with another BRDF LUT
            kD *= sheenScaling;

            const float3 sheenIrradiance = diffuseIBL.Sample( bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
            kD += surfaceInput.sheenColor * sheenIrradiance;
        }
    }
    kD *= ambientHemisphere;
	
    return kD;
}


float3 EvaluateSpecularAmbient( TextureCube specularIBL, Texture2D brdfLUT, const surfaceInput_t surfaceInput )
{
    float3 specular = float3( 0.0f, 0.0f, 0.0f );
    if ( globals.useSpecularIBL )
    {
        const float3 R = reflect( -surfaceInput.V, surfaceInput.N );
        const int MipLevels = (int)GetTextureLevelsCube( specularIBL ) - 1;
        const float3 specIBL = specularIBL.SampleLevel( bilinearSamplerWrap, CubeVector( R ), surfaceInput.roughness * MipLevels ).rgb;

        const float2 envBRDF = brdfLUT.Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).xy;
        specular = specIBL * ( surfaceInput.F * envBRDF.x + envBRDF.y );
    
        // Based on CC IBL note in: https://google.github.io/filament/Filament.md.html
        if ( surfaceInput.useClearCoat )
        {
            const float ccNoV = saturate( dot( surfaceInput.ccNormal, surfaceInput.V ) );
        
            const float3 ccR = reflect( -surfaceInput.V, surfaceInput.ccNormal );          
            const float3 ccSpecIBL = specularIBL.SampleLevel( bilinearSamplerWrap, CubeVector( ccR ), surfaceInput.ccRoughness * MipLevels ).rgb;
    
            const float3 clearcoatF0 = float3( 0.04f, 0.04f, 0.04f );
            float3 clearCoatF = surfaceInput.ccStrength * F_SchlickRoughness( ccNoV, clearcoatF0, surfaceInput.ccRoughness );

            specular *= ( 1.0f - clearCoatF ) * ( 1.0f - clearCoatF );
            specular += clearCoatF * ccSpecIBL;
        }
    } 
    return specular;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 pixelUV = 0.5f * ( input.clipPosition.xy / input.clipPosition.w ) + 0.5f;
    
    const uint materialId = pushConstants.materialId;
    const uint viewlId = pushConstants.viewId;

	const gpuView_t view = views[ viewlId ];
	const gpuMaterial_t material = materials[ materialId ];

    const uint diffuseIBL = surfaces[ input.objectId ].diffuseIblCubeId;
    const uint specularIBL = surfaces[ input.objectId ].envCubeId;
    const uint brdfLutId = globals.brdfLutId;

    const surfaceInput_t surfaceInput = CalculateSurfaceInput( globals, view, material, input );
	
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );

#if 1
    for( int i = 0; i < (int)view.numLights; ++i )
    {
		const gpuLight_t light = lights[i];

		const lightingInput_t lightingInput = CalculateLightingInput( surfaceInput, light );

        brdfSample_t brdf;
        
        if ( surfaceInput.useAniso ) {
            brdf = EvaluateAnisoBrdf( surfaceInput, lightingInput );
        } else {
            brdf = EvaluateBaseBrdf( surfaceInput, lightingInput );
        }
	
        if ( surfaceInput.useClearCoat ) {
			ApplyClearcoatBrdf( surfaceInput, lightingInput, brdf );
        }
        if ( surfaceInput.useSheen ) {
            ApplySheenBrdf( surfaceInput, lightingInput, brdf );
        }
		
        float3 Lo_i = ( brdf.Fd + brdf.Fr ) * lightingInput.Li * lightingInput.NoL;

		const uint shadowMapTexId = ( light.shadowViewId != 0xFF ) ? light.shadowViewId : 0;
		const Texture2D shadowMap = localTextures[ shadowMapTexId ];
		Lo_i = ApplyShadow( light.shadowViewId, surfaceInput.position, Lo_i, shadowMap, depthShadowSampler );

        Lo += Lo_i;
    }
#endif
    
    Texture2D ssaoImage = localTextures[ 3 ]; // FIXME: Index should come from globals and determined CPU-side
    
    const float ssaoSample = ssaoImage.Sample( bilinearSamplerClampEdge, pixelUV ).r;

    const float3 kD = EvaluateDiffuseAmbient( globalCubemaps[ diffuseIBL ], surfaceInput );
    const float3 specularAmbient = EvaluateSpecularAmbient( globalCubemaps[ specularIBL ], globalTextures[ brdfLutId ], surfaceInput );

    const float3 ambient = ( kD * ssaoSample + specularAmbient ) * surfaceInput.ao;

    float4 outColor;
	outColor.rgb = Lo + ambient + surfaceInput.emissive;
    outColor.a = material.opacity;
#define DEBUG_LIGHTING
    
#ifdef DEBUG_LIGHTING
    
    switch ( globals.debugLightingMode )
    {
        case DEBUG_ALBEDO:
            outColor.rgb = surfaceInput.albedo;
            break;
        
        case DEBUG_ROUGHNESS:
            outColor.rgb = surfaceInput.roughness;
            break;
        
        case DEBUG_METALLIC:
            outColor.rgb = surfaceInput.metallic;
            break;
        
        case DEBUG_TBN_NORMAL:
            outColor.rgb = 0.5f * surfaceInput.tangentNormal + float3( 0.5f, 0.5f, 0.5f );
            break;
        
        case DEBUG_NORMAL:
            outColor.rgb = 0.5f * surfaceInput.N + float3( 0.5f, 0.5f, 0.5f );
            break;
        
        case DEBUG_INPUT_UV:
            outColor.rgb = float3( input.uv0.xy, 0.0f );
            break;
        
        case DEBUG_EMISSIVE:
            outColor.rgb = surfaceInput.emissive;
            break;
        
        case DEBUG_SHEENCOLOR:
            outColor.rgb = surfaceInput.sheenColor;
            break;
        
        case DEBUG_SHEENROUGHNESS:
            outColor.rgb = surfaceInput.sheenRoughness;
            break;
        
        case DEBUG_AO:
            outColor.rgb = surfaceInput.ao.rrr;
            break;
        
                
        case DEBUG_BRDF_LUT:
            outColor.rg = globalTextures[ brdfLutId ].Sample( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ) ).rg;
            outColor.b = 0.0f;
            break;
        
        default:
            break;
    }
#endif
    
    psOutput_t output = (psOutput_t)0;

#ifdef USE_MRT
    float4 outColor1;
    outColor1.rgb = 0.5f * ( surfaceInput.N + float3( 1.0f, 1.0f, 1.0f ) );
    //outColor1.rgb = float3( input.uv0.xy, 0.0f );
    outColor1.a = 1.0f;

    output.outColor = ClampColorFp16( outColor );
    output.outColor1 = ClampColorFp16( outColor1 );
#else
    output.outColor = ClampColorFp16( outColor );
#endif
    return output;
}

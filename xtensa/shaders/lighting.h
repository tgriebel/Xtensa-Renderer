#ifndef LIGHT_HLSL_H
#define LIGHT_HLSL_H

#include "globals.h"
#include "util.h"
#include "brdf.h"

// Three structs here used to represent data respective of the lighting equation
// surfaceInput_t: Data from the current surface/pixel sample (one per shader invocation)
// lightingInput_t: Incoming light data to the surface sample (multiple / shader)
// brdfSample_t: Data from what happens when light interacts with the surface sample

// Surface sample data
struct surfaceInput_t
{
	float3	N;
	float3	V;
	float3	F;
	float3	F0;
	float3	T;
	float3	B;
	float3	position;
	float3	albedo;
	float3	ccNormal;
	float3	emissive;
	float3	sheenColor;
	float3	tangentNormal;
	float	NoV;
	float	roughness;
	float	metallic;
	float	ccStrength;	// cc: clear-coat
	float	ccRoughness;
	float	ao;
	float	sheenRoughness;
	float	aniso;
	float	anisoRotation;
	bool	useClearCoat;
	bool	useSheen;
	bool	useAniso;
};


// Surface sample-to-light data
struct lightingInput_t
{
	float3	lightRay;
	float3	intensity;
	float3	L;
	float3	H;
	float3	Li;
	float	lightDistance;
	float	NoL;
	float	NoH;
	float	LoH;
	float	HoV;
};


// BRDF surface sample
struct brdfSample_t
{
	float3 Fd;	// Diffuse
	float3 Fr;	// Specular
	float3 F;	// Fresnel
};


// Generalized structure representing a sample from a given surface
// Can correspond 1:1 with pixel shader interpolators, a ray-intersection payload, or compute shader computation
struct sampleAttributes_t
{
	float3	worldPosition;
	float3	T;	// world-space tangent
	float3	B;	// world-space bitangent
	float3	N;	// world-space geometric normal ( pre-normal-map ), == vsToPsInterpolators.TBN2
	float2	uv0;
	float2	uv1;
};


const float2 PoissonDisk[ 16 ] =
{
	float2( -0.94201624, -0.39906216 ),
	float2( 0.94558609, -0.76890725 ),
	float2( -0.094184101, -0.92938870 ),
	float2( 0.34495938, 0.29387760 ),
	float2( -0.91588581, 0.45771432 ),
	float2( -0.81544232, -0.87912464 ),
	float2( -0.38277543, 0.27676845 ),
	float2( 0.97484398, 0.75648379 ),
	float2( 0.44323325, -0.97511554 ),
	float2( 0.53742981, -0.47373420 ),
	float2( -0.26496911, -0.41893023 ),
	float2( 0.79197514, 0.19090188 ),
	float2( -0.24188840, 0.99706507 ),
	float2( -0.81409955, 0.91437590 ),
	float2( 0.19984126, 0.78641367 ),
	float2( 0.14383161, -0.14100790 )
};


float ShadowPCF( Texture2D shadowMap, SamplerComparisonState samp,
	float3 shadowCoord, float radius, float2 seed )
{
	float angle = frac( sin( dot( seed, float2( 12.9898f, 78.233f ) ) ) * 43758.5453f ) * 6.28318f;
	float2x2 rot = float2x2( cos( angle ), -sin( angle ), sin( angle ), cos( angle ) );

	float texelSize = 1.0f / 2048.0f;
	float shadow = 0.0f;

	[unroll]
	for( int i = 0; i < 16; ++i )
	{
		float2 offset = mul( rot, PoissonDisk[ i ] ) * radius * texelSize;
		shadow += shadowMap.SampleCmpLevelZero( samp, shadowCoord.xy + offset, shadowCoord.z );
	}

	return shadow / 16.0f;
}


surfaceInput_t CalculateSurfaceInput( const gpuGlobals_t globals, const gpuView_t view, const gpuMaterial_t material, const sampleAttributes_t attribs )
{
	float3 albedoSample = material.albedo.rgb;
	float3 normalSample = float3( 0.0f, 0.0f, 1.0f );
	float roughnessSample = material.roughness;
	float metalnessSample = material.metalness;
	float3 emissiveSample = material.emissiveStrength * material.Ke;
	float aoSample = 1.0f;
	float ccSample = material.clearcoatWeight;
	float ccRoughnessSample = clamp( material.clearcoatRoughness, 0.089, 1.0 );
	float3 ccNormalSample = float3( 0.0f, 0.0f, 1.0f );
	float3 sheenSample = material.sheenColor;
	float sheenRoughnessSample = clamp( material.sheenRoughness, 0.07f, 1.0f );
	float anisotropySample = material.anisotropy;
	float transmissionSample = material.transmissionFactor;

	const float2 uv[ 2 ] = { attribs.uv0, attribs.uv1 };

	const bool isTextured = ( material.textured != 0 ) && ( globals.isTextured != 0 );

	if( isTextured )
	{
		albedoSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_ALBEDO_MAP_SLOT, uv ).rgb;

		normalSample = SampleTextureNormal( globalTextures, bilinearSamplerWrap, material, GGX_NORMAL_MAP_SLOT, uv ).xyz;

		roughnessSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_ROUGHNESS_MAP_SLOT, uv ).g;

		metalnessSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_METALLIC_MAP_SLOT, uv ).b;

		aoSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_AO_MAP_SLOT, uv ).r;

		emissiveSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_EMISSIVE_MAP_SLOT, uv ).rgb;

		ccSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_CC_MAP_SLOT, uv ).r;

		ccRoughnessSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_CC_ROUGHNESS_MAP_SLOT, uv ).r;

		ccNormalSample *= SampleTextureNormal( globalTextures, bilinearSamplerWrap, material, GGX_CC_NML_MAP_SLOT, uv ).xyz;

		sheenSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_SHEEN_COLOR_MAP_SLOT, uv ).rgb;

		sheenRoughnessSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_SHEEN_ROUGHNESS_MAP_SLOT, uv ).r;

		anisotropySample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_ANISOTROPY_MAP_SLOT, uv ).r;

		transmissionSample *= SampleTexture( globalTextures, bilinearSamplerWrap, material, GGX_TRANSMISSION_MAP_SLOT, uv ).r;
	}

	const float normalBlendFactor = 1.0f;
	const float3 normal = lerp( float3( 0.0f, 0.0f, 1.0f ), ComputeNormalWS( normalSample, attribs.T, attribs.B, attribs.N ), normalBlendFactor );

	surfaceInput_t surfaceInput = (surfaceInput_t)0;

	surfaceInput.albedo = albedoSample;
	surfaceInput.roughness = saturate( globals.generic.x * roughnessSample + globals.generic.y );
	surfaceInput.metallic = saturate( globals.generic.z * metalnessSample + globals.generic.w );
	surfaceInput.emissive = emissiveSample;
	surfaceInput.ao = saturate( aoSample );
	surfaceInput.sheenColor = saturate( sheenSample );
	surfaceInput.sheenRoughness = saturate( sheenRoughnessSample );
	surfaceInput.ccStrength = saturate( ccSample );
	surfaceInput.ccRoughness = saturate( ccRoughnessSample );
	surfaceInput.ccNormal = normalize( ComputeNormalWS( ccNormalSample, attribs.T, attribs.B, attribs.N ) );
	surfaceInput.useClearCoat = ( ccSample > 0.0f );
	surfaceInput.useSheen = any( sheenSample > 0.0f ) && ( sheenRoughnessSample > 0.0f );
	surfaceInput.useAniso = ( surfaceInput.aniso != 0.0f );

	surfaceInput.N = normalize( normal );
	surfaceInput.V = normalize( view.viewOrigin.xyz - attribs.worldPosition );
	surfaceInput.NoV = saturate( dot( surfaceInput.N, surfaceInput.V ) );
	surfaceInput.F0 = lerp( float3( 0.04f, 0.04f, 0.04f ), surfaceInput.albedo.rgb, surfaceInput.metallic );
	surfaceInput.F = F_SchlickRoughness( surfaceInput.NoV, surfaceInput.F0, surfaceInput.roughness );
	surfaceInput.T = attribs.T;
	surfaceInput.B = attribs.B;

	surfaceInput.position = attribs.worldPosition;
	surfaceInput.tangentNormal = normalSample;

	return surfaceInput;
}


// Rasterization interface
surfaceInput_t CalculateSurfaceInput( const gpuGlobals_t globals, const gpuView_t view, const gpuMaterial_t material, const vsToPsInterpolators input )
{
	sampleAttributes_t attribs;
	attribs.worldPosition = input.worldPosition.xyz;
	attribs.T = input.tangent;
	attribs.B = input.bitangent;
	attribs.N = input.TBN2;
	attribs.uv0 = input.uv0.xy;
	attribs.uv1 = input.uv1.xy;

	return CalculateSurfaceInput( globals, view, material, attribs );
}


lightingInput_t CalculateLightingInput( const surfaceInput_t surfaceInput, const gpuLight_t light )
{
	lightingInput_t lightingInput;

	lightingInput.lightRay = ( light.lightPos.xyz - surfaceInput.position );
	lightingInput.lightDistance = length( lightingInput.lightRay );
	lightingInput.L = lightingInput.lightRay / lightingInput.lightDistance;
	lightingInput.H = normalize( surfaceInput.V + lightingInput.L );
	lightingInput.intensity = light.intensity.rgb;

	lightingInput.NoL = max( dot( surfaceInput.N, lightingInput.L ), 0.0f );
	lightingInput.NoH = max( dot( surfaceInput.N, lightingInput.H ), 0.0f );
	lightingInput.LoH = max( dot( lightingInput.L, lightingInput.H ), 0.0f );
	lightingInput.HoV = max( dot( lightingInput.H, surfaceInput.V ), 0.0f );

	const float attenuation = 1.0f / ( lightingInput.lightDistance * lightingInput.lightDistance );
	const float spotFalloff = 1.0f;
	const float3 radiance = attenuation * spotFalloff * lightingInput.intensity;

	lightingInput.Li = radiance;

	return lightingInput;
}


// Evaluate BRDF functions evaluate a particular BRDF at a surface sample
// Apply* functions modify some BRDF
// Clearcoat attenuates the base BRDF, Sheen simply adds on top

brdfSample_t EvaluateBaseBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput )
{
	const float perceptualRoughness = surfaceInput.roughness;
	const float metallic = surfaceInput.metallic;
	const float3 F0 = surfaceInput.F0;

	const float Dc = D_GGX( lightingInput.NoH, perceptualRoughness );
	const float Gc = G_Smith( surfaceInput.NoV, lightingInput.NoL, perceptualRoughness);
	const float3 Fc = F_Schlick( lightingInput.HoV, F0 );

	const float3 kS = Fc;
	float3 kD = float3( 1.0f, 1.0f, 1.0f ) - kS;
	kD *= 1.0f - metallic;

	float3 numerator = Dc * Gc * Fc;
	float denominator = 4.0f * surfaceInput.NoV * lightingInput.NoL + 0.00001f;

    brdfSample_t brdf;

    brdf.Fr = numerator / denominator;
    brdf.Fd = ( kD * surfaceInput.albedo ) / PI;
    brdf.F = Fc;

	return brdf;
}

// FIXME: temp BS
brdfSample_t EvaluateAnisoBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput )
{
    const float perceptualRoughness = surfaceInput.roughness;
    const float metallic = surfaceInput.metallic;
    const float3 F0 = surfaceInput.F0;

    float sinRot, cosRot;
    sincos( surfaceInput.anisoRotation, sinRot, cosRot );
    const float3 T = cosRot * surfaceInput.T + sinRot * surfaceInput.B;
    const float3 B = -sinRot * surfaceInput.T + cosRot * surfaceInput.B;

    // Roughness split along tangent (at) and bitangent (ab)
    const float2 anisoR = AnisoRoughness( perceptualRoughness, surfaceInput.aniso );
    const float at = anisoR.x;
    const float ab = anisoR.y;

    // Project V and L onto the anisotropic tangent frame
    const float ToV = dot( T, surfaceInput.V );
    const float BoV = dot( B, surfaceInput.V );
    const float ToL = dot( T, lightingInput.L );
    const float BoL = dot( B, lightingInput.L );

    const float Dc = D_GGX_Aniso( lightingInput.NoH, lightingInput.H, T, B, at, ab );
    const float Gc = V_SmithGGXCorrelated_Aniso( at, ab, ToV, BoV, ToL, BoL, surfaceInput.NoV, lightingInput.NoL );
    const float3 Fc = F_Schlick( lightingInput.HoV, F0 );

    float3 kD = ( float3( 1.0f, 1.0f, 1.0f ) - Fc ) * ( 1.0f - metallic );

    brdfSample_t brdf;
    brdf.Fr = Dc * Gc * Fc;
    brdf.Fd = ( kD * surfaceInput.albedo ) / PI;
    brdf.F = Fc;

    return brdf;
}


void ApplyClearcoatBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput, inout brdfSample_t brdf )
{
    const float NoH = saturate( dot( surfaceInput.ccNormal, lightingInput.H ) );
    const float NoL = saturate( dot( surfaceInput.ccNormal, lightingInput.L ) );

	const float F0 = 0.04f;

    const float Dc = D_GGX( NoH, surfaceInput.ccRoughness );
    const float Vc = V_Kelemen( lightingInput.LoH );
    const float Fc = surfaceInput.ccStrength * F_Schlick( lightingInput.LoH, F0 ).x;

    float clearcoat = ( Dc * Vc ) * Fc;

	// Energy loss from base: attenuate by Fresnel of clearcoat
    const float attenuation = ( 1.0f - Fc );

    brdf.Fd *= attenuation;
    brdf.Fr *= attenuation * attenuation;
}


void ApplySheenBrdf( const surfaceInput_t surfaceInput, lightingInput_t lightingInput, inout brdfSample_t brdf )
{
    const float Dc = D_Charlie( lightingInput.NoH, surfaceInput.sheenRoughness );
    const float Vc = V_Neubelt( surfaceInput.NoV, lightingInput.NoL );

    const float3 sheenLobe = surfaceInput.sheenColor * Dc * Vc;

    const float sheenMax = max( surfaceInput.sheenColor.r, max( surfaceInput.sheenColor.g, surfaceInput.sheenColor.b ) );
    const float sheenScaling = 1.0f - sheenMax * 0.157f; // TODO: replace magic number with another BRDF LUT

    // Need to attenuate base energy
    brdf.Fd *= sheenScaling;
    brdf.Fr *= sheenScaling;
    brdf.Fr += sheenLobe;
}


float3 EvaluateDiffuseAmbient( TextureCube diffuseIBL, const surfaceInput_t surfaceInput )
{
    float3 kS = surfaceInput.F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - surfaceInput.metallic;

    float3 ambientHemisphere = AMBIENT.rgb * surfaceInput.albedo; // Overriden by IBL if enabled
    if ( globals.useDiffuseIBL )
    {
        const float3 irradiance = SampleCubeAuto( diffuseIBL, bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
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

            const float3 sheenIrradiance = SampleCubeAuto( diffuseIBL, bilinearSamplerWrap, CubeVector( surfaceInput.N ) ).rgb;
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

        const float2 envBRDF = brdfLUT.SampleLevel( bilinearSamplerClampEdge, float2( surfaceInput.NoV, surfaceInput.roughness ), 0.0f ).xy;
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

#endif // LIGHT_HLSL_H

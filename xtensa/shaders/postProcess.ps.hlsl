#include "globals.h"
#include "util.h"

struct postProcess_t
{
    float4 padding;
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, postProcess_t )


float SampleCoverage( const int2 pos, Texture2D depthTexture )
{
    return depthTexture.Load( int3( pos, 0 ) ).g;
}


float EdgeOutline( const int2 pixelLocation, Texture2D depthTexture )
{
    const float c00 = SampleCoverage( pixelLocation + int2( -1, -1 ), depthTexture );
    const float c10 = SampleCoverage( pixelLocation + int2( 0, -1 ), depthTexture );
    const float c20 = SampleCoverage( pixelLocation + int2( 1, -1 ), depthTexture );
    const float c01 = SampleCoverage( pixelLocation + int2( -1, 0 ), depthTexture );
    const float c11 = SampleCoverage( pixelLocation + int2( 0, 0 ), depthTexture );
    const float c21 = SampleCoverage( pixelLocation + int2( 1, 0 ), depthTexture );
    const float c02 = SampleCoverage( pixelLocation + int2( -1, 1 ), depthTexture );
    const float c12 = SampleCoverage( pixelLocation + int2( 0, 1 ), depthTexture );
    const float c22 = SampleCoverage( pixelLocation + int2( 1, 1 ), depthTexture );

    const float minCoverage = min( min( min( c00, c10 ), min( c20, c01 ) ),
        min( min( c11, c21 ), min( c02, min( c12, c22 ) ) ) );
    const float maxCoverage = max( max( max( c00, c10 ), max( c20, c01 ) ),
        max( max( c11, c21 ), max( c02, max( c12, c22 ) ) ) );

    const float edge = maxCoverage - minCoverage;

    const float outerStrength = 0.1f;
    const float innerStrength = 0.8f;
    const float stencilCoverage = smoothstep( outerStrength, innerStrength, edge );

    return stencilCoverage;
}


float3 ApplyDoF( const Texture2D dofTexture, const Texture2D dofCocTexture, const float3 sceneColor, const float2 uv )
{
    const bool dofEnabled = ( globals.dof.x != 0.0f );
    if ( dofEnabled )
    {
        const float coc = dofCocTexture.Sample( bilinearSamplerClampEdge, uv ).r;
        const float dofBlendWeight = ( abs( coc ) > 1.0f ) ? smoothstep( 1, 2, abs( coc ) ) : 0.0f;
        const float3 dofValue = dofTexture.Sample( bilinearSamplerClampEdge, uv ).rgb;
        return lerp( sceneColor, dofValue, dofBlendWeight );
    }
    return sceneColor;
}


float3 ApplyBloom( const Texture2D bloomTexture, const float3 sceneColor, const float2 uv )
{
	float3 bloomColor;
	
	if ( globals.bloom.x > 0.0f )
	{
		const float3 bloom = bloomTexture.SampleLevel( bilinearSamplerClampEdge, uv, 0 ).rgb;
		const float3 bloomHdr = lerp( sceneColor, bloom, globals.bloom.y );

		bloomColor = bloomHdr;
	}
	else
	{
		bloomColor = sceneColor;
	}
	return bloomColor;
}


float3 ApplyTonemap( const Texture2D luminanceTexture, const float3 sceneColor )
{
	const float middleGrey = globals.exposure.x;
	const float whitePoint = globals.exposure.z;
	const float darkLimit  = globals.exposure.w;

	const float reinhardAlpha = clamp( middleGrey, 0.045f, 0.72f ); // Suggested middle-grey range from reinhard paper

	float3 tonemapColor = sceneColor;

	if ( globals.exposure2.x == 1.0f )
	{
		const float maxLod = float( GetTextureLevels( luminanceTexture ) - 1 );
		const float luminance = luminanceTexture.SampleLevel( bilinearSamplerClampEdge, float2( 0.5f, 0.5f ), maxLod ).r;
		const float exposure = reinhardAlpha / clamp( luminance, darkLimit, 10000.0f );

		tonemapColor *= exposure;
	}

	const float whitePoint2 = whitePoint * whitePoint;
    tonemapColor = tonemapColor * ( 1.0f + tonemapColor / whitePoint2 ) / ( 1.0f + tonemapColor );

	return tonemapColor;
}


psOutput_t PSMain( vsToPsInterpolators input )
{
    psOutput_t output = (psOutput_t)0;

    const gpuView_t view = views[ viewId ];

	Texture2D sceneTexture = localTextures[ 0 ];
	Texture2D depthTexture = localTextures[ 1 ];
	Texture2D dofTexture = localTextures[ 2 ];
    Texture2D luminanceTexture = localTextures[ 3 ];
	Texture2D bloomTexture = localTextures[ 4 ];
	Texture2D dofCocTexture = localTextures[ 5 ];

    const float4x4 viewMat = view.viewMat;
    const float3 forward = -normalize( viewMat[2].xyz );
    const float3 up = normalize( viewMat[0].xyz );
    const float3 right = normalize( viewMat[1].xyz );
    const float3 viewVector = normalize( forward + input.uv0.x * up + input.uv0.y * right );

    const int2 pixelLocation = int2( view.dimensions.xy * input.uv0.xy );

    const float stencilCoverage = EdgeOutline( pixelLocation, depthTexture );

    float4 sceneColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );

    const float4 uvColor = float4( input.uv0.xy, 0.0f, 1.0f );

    output.outColor.rgb = float3( 0.0f, 0.0f, 0.0f );

    const float3 sceneTexColor = sceneTexture.Sample( bilinearSamplerClampEdge, input.uv0.xy ).rgb;
    const float3 hdrColor = ApplyDoF( dofTexture, dofCocTexture, sceneTexColor, input.uv0.xy );

    const float3 tint = globals.toneMapTint.rgb;

	float3 finalColor = tint * hdrColor;

	finalColor = ApplyBloom( bloomTexture, finalColor, input.uv0.xy );

	finalColor = ApplyTonemap( luminanceTexture, finalColor );

	sceneColor.rgb = LinearToSrgb( finalColor );
    
    const float3 selectedOutlineColor = float3( 0.0f, 1.0f, 0.0f );
    const float3 selectedFillColor = float3( 1.0f, 0.0f, 0.0f );

    output.outColor.rgb = lerp( sceneColor.rgb, selectedOutlineColor, stencilCoverage );
    output.outColor.a = 1.0f;
    
    output.outColor.r += length( output.outColor.rgb * selectedFillColor * depthTexture.Load( int3( pixelLocation, 0 ) ).g );

    return output;
}

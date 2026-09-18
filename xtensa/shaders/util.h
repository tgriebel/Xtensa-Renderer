#ifndef UTIL_HLSL_H
#define UTIL_HLSL_H

#ifdef USE_RT
#include "rayCone.h"
#endif

#ifndef PI
#define PI              3.14159265359f
#endif
#ifndef INV_PI
#define INV_PI          ( 1.0f / PI )
#endif

int2 GetTextureSize( Texture2D tex, int mipLevel )
{
	uint w, h, levels;
	tex.GetDimensions( mipLevel, w, h, levels );
	return int2( w, h );
}


uint GetTextureLevels( Texture2D tex )
{
	uint w, h, levels;
	tex.GetDimensions( 0, w, h, levels );
	return levels;
}


uint GetTextureLevelsCube( TextureCube tex )
{
	uint w, h, levels;
	tex.GetDimensions( 0, w, h, levels );
	return levels;
}


// GLSL mat4() fills columns-first; HLSL float4x4() fills rows-first.
// This is the transposed form so it matches the GLSL glslSpace matrix.
static const float4x4 glslSpace = float4x4(
	 0.0f, -1.0f, 0.0f, 0.0f,
	 0.0f,  0.0f, 1.0f, 0.0f,
	 1.0f,  0.0f, 0.0f, 0.0f,
	 0.0f,  0.0f, 0.0f, 0.0f
);


float3 CubeVector( const float3 v )
{
	return float3( -v.y, v.z, v.x ); // to glsl coordinate space
}


float3 EncodeNormal( const float3 normalMapTexel )
{
	return ( 0.5f * normalMapTexel + float3( 0.5f, 0.5f, 0.5f ) );
}


float3 DecodeNormal( const float3 normalMapTexel )
{
	return ( 2.0f * normalMapTexel - float3( 1.0f, 1.0f, 1.0f ) );
}


// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
// Spherical and Octehedral encoding
float2 EncodeSpherical( const float3 n )
{
	float2 f;
	f.x = INV_PI * atan2( n.y, n.x );
	f.y = n.z;

	f = 0.5f * f + float2( 0.5f, 0.5f );
	return f;
}


float3 DecodeSpherical( const float2 f )
{
	const float2 ang = f * 2.0f - 1.0f;

	float2 scth;
	sincos( PI * ang.x, scth.x, scth.y );
	float2 scphi = float2( sqrt( 1.0f - ang.y * ang.y ), ang.y );

	float3 n;
	n.x = scth.y * scphi.x;
	n.y = scth.x * scphi.x;
	n.z = scphi.y;
	return n;
}


float2 OctWrap( const float2 v )
{
	return ( 1.0f - abs( v.yx ) ) * ( step( 0.0f, v ) * 2.0f - 1.0f );
}


float2 OctEncode( const float3 n )
{
	float3 outV = n;
	outV /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	outV.xy = outV.z >= 0.0 ? outV.xy : OctWrap( outV.xy );
	outV.xy = outV.xy * 0.5 + 0.5;
	return outV.xy;
}


float3 OctDecode( const float2 f )
{
	float2 enc = 2.0f * f - float2( 1.0f, 1.0f );

	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3( enc.x, enc.y, 1.0f - abs( enc.x ) - abs( enc.y ) );
	float t = saturate( -n.z );
	n.xy += select( n.xy >= 0.0f, -t, t );
	return normalize( n );
}


float3 ComputeNormalWS( const float3 tangentNormal, const float3 T, const float3 B, const float3 N )
{
	return normalize( tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N );
}


float ClampColorFp16( const float color )
{
	return clamp( color, 0.0f, 65504.0f );
}



float2 ClampColorFp16( const float2 color )
{
	return clamp( color, 0.0f, 65504.0f );
}


float3 ClampColorFp16( const float3 color )
{
	return clamp( color, 0.0f, 65504.0f );
}


float4 ClampColorFp16( const float4 color )
{
	return clamp( color, 0.0f, 65504.0f );
}


float3 ViewForward( const float4x4 view )
{
	return float3( view[ 2 ][ 0 ], view[ 2 ][ 1 ], view[ 2 ][ 2 ] );
}


float3 ViewRight( const float4x4 view )
{
	return float3( view[ 0 ][ 0 ], view[ 0 ][ 1 ], view[ 0 ][ 2 ] );
}


float3 ViewUp( const float4x4 view )
{
	return float3( view[ 1 ][ 0 ], view[ 1 ][ 1 ], view[ 1 ][ 2 ] );
}


float LinearDepth( const float zDepth, const float4x4 invProj )
{
	//float a = proj[ 2 ][ 2 ];
	//float b = proj[ 3 ][ 2 ];
	//return b / ( z + a );

	float4 viewPos = mul( invProj, float4( 0.0f, 0.0f, zDepth, 1.0f ) );
	viewPos /= viewPos.w;
	return viewPos.z;
}


float3 ReconstructViewPos( const float2 uv, const float zDepth, const float4x4 invProj )
{
	//float depth = LinearDepth( zDepth, proj );

	float2 ndc = 2.0f * uv - float2( 1.0f, 1.0f );

	//float2 viewPos = ndc * depth * float2( 1.0f / proj[ 0 ][ 0 ], 1.0f / proj[ 1 ][ 1 ] );

	//return float3( viewPos, -depth ); // View-forward vector points towards origin
	float4 viewPos = mul( invProj, float4( ndc.xy, zDepth, 1.0f ) );
	viewPos /= viewPos.w;
	return viewPos.z;
}


// https://atyuwen.github.io/posts/normal-reconstruction/
float3 ReconstructNormal( Texture2D depthBuffer, SamplerState samp, const float2 uv, const float4 dimensions, const float4x4 proj )
{
	const float2 ts = float2( dimensions.z, dimensions.w );

	const float dR = depthBuffer.SampleLevel( samp, uv + float2(  ts.x, 0.0f ), 0 ).r;
	const float dL = depthBuffer.SampleLevel( samp, uv + float2( -ts.x, 0.0f ), 0 ).r;
	const float dU = depthBuffer.SampleLevel( samp, uv + float2( 0.0f,  ts.y ), 0 ).r;
	const float dD = depthBuffer.SampleLevel( samp, uv + float2( 0.0f, -ts.y ), 0 ).r;

	const float3 pR = ReconstructViewPos( uv + float2(  ts.x, 0.0f ), dR, proj );
	const float3 pL = ReconstructViewPos( uv + float2( -ts.x, 0.0f ), dL, proj );
	const float3 pU = ReconstructViewPos( uv + float2( 0.0f,  ts.y ), dU, proj );
	const float3 pD = ReconstructViewPos( uv + float2( 0.0f, -ts.y ), dD, proj );

	// Pick the horizontal and vertical pair with the smaller depth discontinuity
	// so normals stay sharp at silhouette edges.
	const float3 dX = ( abs( dR - dL ) < abs( dU - dD ) ) ? ( pR - pL ) : ( pL - pR );
	const float3 dY = ( abs( dU - dD ) < abs( dR - dL ) ) ? ( pU - pD ) : ( pD - pU );

	return normalize( cross( dX, dY ) );
}


void BuildTBN( in float3 N, out float3 T, out float3 B )
{
	float3 up = ( abs( N.z ) < 0.999f ) ? float3( 0.0f, 0.0f, 1.0f ) : float3( 1.0f, 0.0f, 0.0f );
	T = normalize( cross( up, N ) );
	B = cross( N, T );
}


// http://advances.realtimerendering.com/s2014/index.html, CoD:AW slide 123
float InterleavedGradientNoise( float2 screenPos )
{
	const float3 k = float3( 0.06711056f, 0.00583715f, 52.9829189f );
	return frac( k.z * frac( dot( screenPos, k.xy ) ) );
}


float SrgbToLinear( float value )
{
	return ( value <= 0.04045f ) ? value / 12.92f : pow( ( value + 0.055f ) / 1.055f, 2.4f );
}


float3 SrgbToLinear( float3 sRGB )
{
	return float3( SrgbToLinear( sRGB.r ), SrgbToLinear( sRGB.g ), SrgbToLinear( sRGB.b ) );
}


float4 SrgbToLinear( float4 sRGBA )
{
	return float4( SrgbToLinear( sRGBA.rgb ), sRGBA.a );
}


float LinearToSrgb( float value )
{
	return ( value < 0.0031308f ? value * 12.92f : 1.055f * pow( value, 0.41666f ) - 0.055f );
}


float3 LinearToSrgb( float3 inLinear )
{
	return float3( LinearToSrgb( inLinear.r ), LinearToSrgb( inLinear.g ), LinearToSrgb( inLinear.b ) );
}


float4 LinearToSrgb( float4 inLinear )
{
	return float4( LinearToSrgb( inLinear.rgb ), inLinear.a );
}


float3 VectorDebugColor( const float3 vector )
{
	return 0.5f * ( vector + float3( 1.0f, 1.0f, 1.0f ) );
}


#ifdef USE_RT
struct rtTexLodContext_t
{
	rayCone_t	cone;
	float		triangleWorldArea;
	float		triangleUvArea;
	float3		rayDir;
	float3		surfaceNormal;
};
static rtTexLodContext_t g_rtTexLod;
#endif


// Used to get around texture derivative requirement
float4 SampleTex2DAuto( Texture2D tex, SamplerState samp, float2 uv )
{
#ifdef USE_RT
	uint texWidth, texHeight;
	tex.GetDimensions( texWidth, texHeight );

	const float lod = ComputeTextureLOD( g_rtTexLod.cone, g_rtTexLod.triangleWorldArea, g_rtTexLod.triangleUvArea,
		g_rtTexLod.rayDir, g_rtTexLod.surfaceNormal, (float)texWidth, (float)texHeight );

	return tex.SampleLevel( samp, uv, lod );
#else
	return tex.Sample( samp, uv );
#endif
}


float4 SampleTexture( const Texture2D textures[], SamplerState sampler, const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[ materialTextureSlot ];

		return SampleTex2DAuto( textures[ textureUploadId ], sampler, transformedUv );

	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float4 SampleTextureSrgb( const Texture2D textures[], SamplerState sampler, const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[ materialTextureSlot ];

		return SrgbToLinear( SampleTex2DAuto( textures[ textureUploadId ], sampler, transformedUv.xy ) );
	}
	return float4( 1.0f, 1.0f, 1.0f, 1.0f );
}


float3 SampleTextureNormal( const Texture2D textures[], SamplerState sampler, const gpuMaterial_t material, const int materialTextureSlot, const float2 uv[ 2 ] )
{
	const int textureUploadId = material.textureId[ materialTextureSlot ];
	const uint channel = min( MaxTextureUVs - 1, material.uvChannel[ materialTextureSlot ] );

	if( textureUploadId >= 0 )
	{
		const float2 transformedUv = mul( material.uvTransform[ materialTextureSlot ], uv[ channel ] ) + material.uvOffset[ materialTextureSlot ];

		return DecodeNormal( SampleTex2DAuto( textures[ textureUploadId ], sampler, transformedUv ).xyz );
	}
	return float3( 0.0f, 0.0f, 1.0f );
}


float CircleOfConfusion( const float aperture, const float focallength, const float planeinfocus, const float objectdistance )
{
	// https://developer.nvidia.com/gpugems/gpugems/part-iv-image-processing/chapter-23-depth-field-survey-techniques
	// Also: Graphics Gems from Cryengine 3
	const float numerator = ( focallength * ( objectdistance - planeinfocus ) );
	const float denom = ( objectdistance * ( planeinfocus - focallength ) );

	const float coc = aperture * ( numerator / denom );
	return coc;
}


#endif // UTIL_HLSL_H

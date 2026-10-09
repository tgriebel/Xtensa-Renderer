// Crytek SSAO
// Based on "Finding Next Gen — CryEngine 2", Karol Mattsson, SIGGRAPH 2007.

#include "globals.h"
#include "util.h"

#define MaxSamples 16
#define NoiseSamples 64

struct SSAOConstants
{
    float   radius;                         // World-space sampling radius (meters)
    uint    numSamples;                     // Sample count — 8 (fast) to 32 (quality)
    float   bias;                           // Depth bias to prevent self-occlusion on flat surfaces (meters)
    float   strength;                       // AO multiplier: 1 = standard, higher = darker
    float4  sampleKernel[ MaxSamples ];     // Sample offsets in tangent space (x,y,z) and padding (w)
    float2  sampleNoise[ NoiseSamples ];    // Noise samples for dithering
};

PS_LAYOUT_IMAGE_SHADER( Texture2D, SSAOConstants )

// localTextures[0] = resolved depth buffer


psOutput_t PSMain( vsToPsInterpolators input )
{
    const float2 uv = input.uv0.xy;
    const int2 pixelPos = int2( dimensions.xy * uv );

    psOutput_t output = (psOutput_t)0;
    
    const uint SourceImageIx = 0;
    const uint ResourceImageIx0 = 1;
    const uint ResourceImageIx1 = 2;
    const uint ResourceImageIx2 = 3;

    const float depthP = localTextures[ ResourceImageIx0 ].Load( int3( pixelPos, 0 ) ).r;

    // Skip sky
    if ( depthP <= 0.0f )
    {
        output.outColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
        return output;
    }
    
    const float randomNoise = InterleavedGradientNoise( float2( pixelPos ) ) * 2.0f * PI;

    const float4x4 view = views[ viewId ].viewMat;
    const float4x4 proj = views[ viewId ].invProjMat;
    const float3 viewOrigin = views[ viewId ].viewOrigin;
    const float3 P = ReconstructViewPos( uv, depthP, proj );
    const float3 N = OctDecode( localTextures[ ResourceImageIx1 ].SampleLevel( bilinearSamplerClampEdge, uv, 0 ).ba );

    float3 T, B;
    BuildTBN( N, T, B );
    
    // UV-space extent of the sampling radius at this pixel's depth.
    // viewDepth = -P.z because ReconstructViewPos returns z < 0 for visible geometry.
    // proj[0][0] = 1/tan(fovX/2),  proj[1][1] = 1/tan(fovY/2).
    const float viewDepth = -P.z;
    const float2 radiusUV  = float2( imageProcess.radius * proj[ 0 ][ 0 ],
                                     imageProcess.radius * proj[ 1 ][ 1 ] )
                             / viewDepth * 0.5f;   
    
   // const float2 radiusUV = 0.5f;

    float occlusion = 0.0f;

    for ( uint i = 0; i < imageProcess.numSamples; ++i )
    {
        // Near-to-far radial scale, relative to the *live* sample count so it
        // always spans the full [0.1, 1.0] range regardless of numSamples.
        const float t = float( i ) / float( imageProcess.numSamples );
        const float scale = lerp( 0.1f, 1.0f, t * t );

        const float3 s = scale * imageProcess.sampleKernel[ i ].xyz;

        // Transform into view space: s.x along T, s.y along B, s.z along N.
        // Every sample is guaranteed to lie in the visible hemisphere of the surface.
        const float3 dir = s.x * T + s.y * B + s.z * N;

        const float2 sampleUV = uv + dir.xy * radiusUV;
        const float depthS =localTextures[ ResourceImageIx0 ].SampleLevel( bilinearSamplerClampEdge, sampleUV, 0 ).r;

        if ( depthS <= 0.0f ) {
            continue; // Sky behind the sample — skip
        }
        
        const float3 S = ReconstructViewPos( sampleUV, depthS, proj );
        
        // Occlusion (z increases from camera origin)
        const float coverage = ( S.z >= ( P.z + imageProcess.bias ) ) ? 1.0f : 0.0f;

        // Range. Discount samples as their distance increases
        float rangeCheck = smoothstep( 0.0f, 1.0f, imageProcess.radius / abs( P.z - S.z ) );
        occlusion += coverage; // * rangeCheck;
    }

    // Average, apply strength, then invert so 1 = fully lit, 0 = fully occluded.
    const float ao = 1.0f - saturate( imageProcess.strength * ( occlusion / imageProcess.numSamples ) );

    output.outColor = float4( ao.xxx, 1.0f );
    //output.outColor.rgb = EncodeNormal( N );
   // output.outColor.rgb = viewDepth.xxx;
    output.outColor.a = 1.0f;
    return output;
}
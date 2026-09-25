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

static float2 dim = float2( 256.0f, 240.0f );
// Emulated input resolution.
#if 1
  // Fix resolution to set amount.
#define res (float2(256.0/1.0,240.0/1.0))
#else
  // Optimize for resize.
#define res (dim.xy/6.0)
#endif

// Hardness of scanline.
//  -8.0 = soft
// -16.0 = medium
static float hardScan = -8.0;

// Hardness of pixels in scanline.
// -2.0 = soft
// -4.0 = hard
static float hardPix = -3.0;

// Display warp.
// 0.0 = none
// 1.0/8.0 = extreme
static float2 warp = float2( 1.0 / 32.0, 1.0 / 24.0 );

// Amount of shadow mask.
static float maskDark = 0.5;
static float maskLight = 1.5;

//------------------------------------------------------------------------

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
float3 Fetch( float2 pos, float2 off, vsToPsInterpolators input )
{
    pos = floor( pos * res + off ) / res;
    if ( max( abs( pos.x - 0.5 ), abs( pos.y - 0.5 ) ) > 0.5 ) return float3( 0.0, 0.0, 0.0 );

    const uint materialId = pushConstants.materialId;
    const uint textureId0 = materials[ materialId ].textureId[ 0 ];
    return SrgbToLinear( globalTextures[ textureId0 ].SampleBias( bilinearSamplerClampEdge, pos.xy, -16.0 ).rgb );
}

// Distance in emulated pixels to nearest texel.
float2 Dist( float2 pos )
{
    pos = pos * res;
    return -( ( pos - floor( pos ) ) - float2( 0.5, 0.5 ) );
}

// 1D Gaussian.
float Gaus( float pos, float scale )
{
    return exp2( scale * pos * pos );
}

// 3-tap Gaussian filter along horz line.
float3 Horz3( float2 pos, float off, vsToPsInterpolators input )
{
    float3 b = Fetch( pos, float2( -1.0, off ), input );
    float3 c = Fetch( pos, float2( 0.0, off ), input );
    float3 d = Fetch( pos, float2( 1.0, off ), input );
    float dst = Dist( pos ).x;
    // Convert distance to weight.
    float scale = hardPix;
    float wb = Gaus( dst - 1.0, scale );
    float wc = Gaus( dst + 0.0, scale );
    float wd = Gaus( dst + 1.0, scale );
    // Return filtered sample.
    return ( b * wb + c * wc + d * wd ) / ( wb + wc + wd );
}

// 5-tap Gaussian filter along horz line.
float3 Horz5( float2 pos, float off, vsToPsInterpolators input )
{
    float3 a = Fetch( pos, float2( -2.0, off ), input );
    float3 b = Fetch( pos, float2( -1.0, off ), input );
    float3 c = Fetch( pos, float2( 0.0, off ), input );
    float3 d = Fetch( pos, float2( 1.0, off ), input );
    float3 e = Fetch( pos, float2( 2.0, off ), input );
    float dst = Dist( pos ).x;

    // Convert distance to weight.
    float scale = hardPix;
    float wa = Gaus( dst - 2.0, scale );
    float wb = Gaus( dst - 1.0, scale );
    float wc = Gaus( dst + 0.0, scale );
    float wd = Gaus( dst + 1.0, scale );
    float we = Gaus( dst + 2.0, scale );
    // Return filtered sample.
    return ( a * wa + b * wb + c * wc + d * wd + e * we ) / ( wa + wb + wc + wd + we );
}

// Return scanline weight.
float Scan( float2 pos, float off ) {
    float dst = Dist( pos ).y;
    return Gaus( dst + off, hardScan );
}

// Allow nearest three lines to effect pixel.
float3 Tri( float2 pos, vsToPsInterpolators input )
{
    const float3 a = Horz3( pos, -1.0f, input );
    const float3 b = Horz5( pos, 0.0f, input );
    const float3 c = Horz3( pos, 1.0f, input );
    const float wa = Scan( pos, -1.0f );
    const float wb = Scan( pos, 0.0f );
    const float wc = Scan( pos, 1.0f );

    return ( a * wa ) + ( b * wb ) + ( c * wc );
}

// Distortion of scanlines, and end of screen alpha.
float2 Warp( float2 pos )
{
    float2 distorted = pos * 2.0f - 1.0f;
    distorted *= float2( 1.0f + ( distorted.y * distorted.y ) * warp.x, 1.0f + ( distorted.x * distorted.x ) * warp.y );
    return ( distorted * 0.5f + 0.5f );
}

// Shadow mask.
float3 Mask( float2 pos )
{
    float3 mask = float3( maskDark, maskDark, maskDark );
    pos.x += pos.y * 3.0f;
    pos.x = frac( pos.x / 6.0f );

    if ( pos.x < 0.333f )
        mask.r = maskLight;
    else if ( pos.x < 0.666f )
        mask.g = maskLight;
    else
        mask.b = maskLight;

    return mask;
}

// Draw dividing bars.
float Bar( float pos, float bar ) { pos -= bar; return pos * pos < 4.0 ? 0.0 : 1.0; }

// Entry.
psOutput_t PSMain( vsToPsInterpolators input )
{
	psOutput_t output = (psOutput_t)0;
    float2 pos = Warp( input.uv0.xy );

    float4 outColor;
    outColor.rgb = Tri( pos, input ) * Mask( input.uv0.xy / res );
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

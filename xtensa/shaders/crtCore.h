// This is modified from the original code by Timothy Lottes. Original header as follows:
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

// Emulated input resolution.
static const float2 CrtRes = float2( 256.0 / 1.0, 240.0 / 1.0 );

// Hardness of scanline. -8.0 = soft, -16.0 = medium.
static const float CrtHardScan = -8.0;

// Hardness of pixels in scanline. -2.0 = soft, -4.0 = hard.
static const float CrtHardPix = -3.0;

// Display warp. 0.0 = none, 1.0/8.0 = extreme.
static const float2 CrtWarpAmount = float2( 1.0 / 32.0, 1.0 / 24.0 );

// Amount of shadow mask.
static const float CrtMaskDark = 0.5;
static const float CrtMaskLight = 1.5;


// Nearest emulated sample given floating point position and texel offset. Also zeros off screen.
float3 CrtFetch( Texture2D tex, SamplerState samp, float2 pos, float2 off )
{
    pos = floor( pos * CrtRes + off ) / CrtRes;
    if ( max( abs( pos.x - 0.5 ), abs( pos.y - 0.5 ) ) > 0.5 ) return float3( 0.0, 0.0, 0.0 );

    // SampleLevel with an explicit LOD of 0 (rather than SampleBias) so this also works from
    // a hit shader, which has no screen-space derivatives to compute an implicit LOD from.
    return SrgbToLinear( tex.SampleLevel( samp, pos.xy, 0.0f ).rgb );
}


// Distance in emulated pixels to nearest texel.
float2 CrtDist( float2 pos )
{
    pos = pos * CrtRes;
    return -( ( pos - floor( pos ) ) - float2( 0.5, 0.5 ) );
}


// 1D Gaussian.
float CrtGaus( float pos, float scale )
{
    return exp2( scale * pos * pos );
}


// 3-tap Gaussian filter along horz line.
float3 CrtHorz3( Texture2D tex, SamplerState samp, float2 pos, float off )
{
    float3 b = CrtFetch( tex, samp, pos, float2( -1.0, off ) );
    float3 c = CrtFetch( tex, samp, pos, float2( 0.0, off ) );
    float3 d = CrtFetch( tex, samp, pos, float2( 1.0, off ) );
    float dst = CrtDist( pos ).x;

    float scale = CrtHardPix;
    float wb = CrtGaus( dst - 1.0, scale );
    float wc = CrtGaus( dst + 0.0, scale );
    float wd = CrtGaus( dst + 1.0, scale );

    return ( b * wb + c * wc + d * wd ) / ( wb + wc + wd );
}


// 5-tap Gaussian filter along horz line.
float3 CrtHorz5( Texture2D tex, SamplerState samp, float2 pos, float off )
{
    float3 a = CrtFetch( tex, samp, pos, float2( -2.0, off ) );
    float3 b = CrtFetch( tex, samp, pos, float2( -1.0, off ) );
    float3 c = CrtFetch( tex, samp, pos, float2( 0.0, off ) );
    float3 d = CrtFetch( tex, samp, pos, float2( 1.0, off ) );
    float3 e = CrtFetch( tex, samp, pos, float2( 2.0, off ) );
    float dst = CrtDist( pos ).x;

    float scale = CrtHardPix;
    float wa = CrtGaus( dst - 2.0, scale );
    float wb = CrtGaus( dst - 1.0, scale );
    float wc = CrtGaus( dst + 0.0, scale );
    float wd = CrtGaus( dst + 1.0, scale );
    float we = CrtGaus( dst + 2.0, scale );

    return ( a * wa + b * wb + c * wc + d * wd + e * we ) / ( wa + wb + wc + wd + we );
}


// Return scanline weight.
float CrtScan( float2 pos, float off )
{
    float dst = CrtDist( pos ).y;
    return CrtGaus( dst + off, CrtHardScan );
}


// Allow nearest three lines to affect pixel.
float3 CrtTri( Texture2D tex, SamplerState samp, float2 pos )
{
    const float3 a = CrtHorz3( tex, samp, pos, -1.0f );
    const float3 b = CrtHorz5( tex, samp, pos, 0.0f );
    const float3 c = CrtHorz3( tex, samp, pos, 1.0f );
    const float wa = CrtScan( pos, -1.0f );
    const float wb = CrtScan( pos, 0.0f );
    const float wc = CrtScan( pos, 1.0f );

    return ( a * wa ) + ( b * wb ) + ( c * wc );
}


// Distortion of scanlines, and end of screen alpha.
float2 CrtWarp( float2 pos )
{
    float2 distorted = pos * 2.0f - 1.0f;
    distorted *= float2( 1.0f + ( distorted.y * distorted.y ) * CrtWarpAmount.x, 1.0f + ( distorted.x * distorted.x ) * CrtWarpAmount.y );
    return ( distorted * 0.5f + 0.5f );
}


// Shadow mask.
float3 CrtMask( float2 pos )
{
    float3 mask = float3( CrtMaskDark, CrtMaskDark, CrtMaskDark );
    pos.x += pos.y * 3.0f;
    pos.x = frac( pos.x / 6.0f );

    if ( pos.x < 0.333f )
        mask.r = CrtMaskLight;
    else if ( pos.x < 0.666f )
        mask.g = CrtMaskLight;
    else
        mask.b = CrtMaskLight;

    return mask;
}


// Full effect: warp -> tri-line Gaussian sample -> shadow mask, given a UV in [0,1].
float3 CrtShade( Texture2D tex, SamplerState samp, float2 uv )
{
    const float2 pos = CrtWarp( uv );
    return CrtTri( tex, samp, pos ) * CrtMask( uv / CrtRes );
}

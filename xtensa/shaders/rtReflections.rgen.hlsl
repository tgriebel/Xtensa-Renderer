// Reflection ray generation — one ray per screen-space pixel

#include "rtGlobals.h"
#include "util.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
CODE_IMAGE_LAYOUT( 1, 6, Texture2D )
RT_PUSH_CONSTANTS

// localTextures[0] = resolved pre-pass G-buffer (outColor1.ba = oct-encoded view-space normal)
// localTextures[1] = resolved depth buffer

[shader( "raygeneration" )]
void RayGen()
{
    const uint2 launchIndex = DispatchRaysIndex().xy;
    const uint2 launchSize  = DispatchRaysDimensions().xy;

    const gpuView_t view = views[ rtConstants.viewId ];

    const float2 pixelCenter = float2( launchIndex ) + 0.5f;
    const float2 ndc = ( pixelCenter / float2( launchSize ) ) * 2.0f - 1.0f;

    const float depth = localTextures[ 1 ].Load( int3( launchIndex, 0 ) ).r;

    // Sky
    if ( depth <= 0.0f )
    {
        rtOutput[ launchIndex ] = float4( 0.0f, 0.0f, 0.0f, 1.0f );
        return;
    }

    float4 viewPosH = mul( view.invProjMat, float4( ndc.x, ndc.y, depth, 1.0f ) );
    viewPosH /= viewPosH.w;
    const float3 worldPos = view.viewOrigin + mul( viewPosH.xyz, (float3x3)view.viewMat );

    const float3 normalVS = OctDecode( localTextures[ 0 ].Load( int3( launchIndex, 0 ) ).ba );
    const float3 N = normalize( mul( normalVS, (float3x3)view.viewMat ) );

    const float3 V = normalize( view.viewOrigin - worldPos );
    const float3 R = reflect( -V, N );

    const float bias = 0.01f; // Bias to avoid self-intersection
    
    RayDesc ray;
    ray.Origin = worldPos + N * bias;
    ray.Direction = R;
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;

    // projMat[1][1] == 1 / tan(fovY/2) for a standard perspective projection
    const float verticalFovRadians = 2.0f * atan( 1.0f / view.projMat[ 1 ][ 1 ] );

    hitPayload_t payload;
    payload.color = float4( 0.0f, 0.0f, 0.0f, 1.0f );
    payload.cone  = InitRayCone( verticalFovRadians, launchSize.y );

    TraceRay(
        tlas,
        RAY_FLAG_NONE,
        0xFF,   // instance mask — all instances
        0,      // hit group index
        0,      // hit group stride
        0,      // miss shader index
        ray,
        payload );

    rtOutput[ launchIndex ] = payload.color;
}

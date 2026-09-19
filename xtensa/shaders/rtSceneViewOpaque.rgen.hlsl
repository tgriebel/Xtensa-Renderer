// Primary ray generation — one ray per screen-space pixel

#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_PUSH_CONSTANTS

[shader( "raygeneration" )]
void RayGen()
{
    const uint2 launchIndex = DispatchRaysIndex().xy;
    const uint2 launchSize  = DispatchRaysDimensions().xy;

    const gpuView_t view = views[ rtConstants.viewId ];

    const float2 pixelCenter = float2( launchIndex ) + 0.5f;
    const float2 ndc = ( pixelCenter / float2( launchSize ) ) * 2.0f - 1.0f;

    const float4 viewTarget = mul( view.invProjMat, float4( ndc.x, ndc.y, 1.0f, 1.0f ) );
    const float3 camDir = normalize( viewTarget.xyz / viewTarget.w );

    const float3 worldDir = normalize( mul( camDir, (float3x3)view.viewMat ) );

    RayDesc ray;
    ray.Origin = view.viewOrigin;
    ray.Direction = worldDir;
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

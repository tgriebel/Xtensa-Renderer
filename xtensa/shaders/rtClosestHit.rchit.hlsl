#include "rtGlobals.h"

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_VERTEX_BUFFER( 1, 2, vtxBuffer )
RT_INDEX_BUFFER( 1, 3, idxBuffer )
RT_SURFACE_INFO( 1, 4, surfaceInfos )
RT_PUSH_CONSTANTS


// Interpolate a float3 attribute across a triangle using barycentric weights.
float3 BaryLerp3( float3 a, float3 b, float3 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}

float2 BaryLerp2( float2 a, float2 b, float2 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}


[shader( "closesthit" )]
void closesthit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes attribs )
{
    // InstanceID() returns instanceCustomIndex, which we set to the BLAS index.
    // This maps directly to the surface info entry for this geometry.
    const uint surfIdx = InstanceID();
    const gpuRtSurface_t surf = surfaceInfos[ surfIdx ];

    // Locate the hit triangle in the index buffer
    const uint triBase = surf.firstIndex + PrimitiveIndex() * 3;
    const uint i0 = idxBuffer[ triBase + 0 ];
    const uint i1 = idxBuffer[ triBase + 1 ];
    const uint i2 = idxBuffer[ triBase + 2 ];

    // Fetch the three vertices (raw index + vertexOffset = absolute VB position)
    const rtVertex_t v0 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i0 );
    const rtVertex_t v1 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i1 );
    const rtVertex_t v2 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i2 );

    // Barycentric weights (b0 + b1 + b2 = 1.0)
    const float b1 = attribs.barycentrics.x;
    const float b2 = attribs.barycentrics.y;
    const float b0 = 1.0f - b1 - b2;

    // Interpolate geometric attributes
    const float3 localNormal = normalize( BaryLerp3( v0.normal, v1.normal, v2.normal, b0, b1, b2 ) );
    const float2 uv = BaryLerp2( v0.uv.xy, v1.uv.xy, v2.uv.xy, b0, b1, b2 );

    // Transform normal to world space.
    const float3 N = normalize( mul( localNormal, (float3x3)WorldToObject3x4() ) );

    // Simple diffuse + ambient shading with a fixed directional light
    const float3 L = normalize( float3( 1.0f, 2.0f, 1.0f ) );
    const float NoL = saturate( dot( N, L ) );

    const float3 diffuse = float3( 0.8f, 0.8f, 0.8f ) * NoL;
    const float3 ambient = float3( 0.03f, 0.03f, 0.03f );

    payload.color = float4( diffuse + ambient, 1.0f );
}

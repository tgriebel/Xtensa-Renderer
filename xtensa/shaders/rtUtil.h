#ifndef RTUTIL_HLSL_H
#define RTUTIL_HLSL_H

#include "surfaceTypes.h"

// Interpolate a float3 attribute across a triangle using barycentric weights.
float3 BaryLerp3( float3 a, float3 b, float3 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}

float2 BaryLerp2( float2 a, float2 b, float2 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}


struct rtTriangle_t
{
    rtVertex_t v0;
    rtVertex_t v1;
    rtVertex_t v2;
    float      b0;
    float      b1;
    float      b2;
};


// Looks up a triangle's three indices, vertices, and barycentric weights
rtTriangle_t LoadRtTriangle( ByteAddressBuffer vtxBuf, StructuredBuffer<uint> idxBuf,
    uint triBase, uint vertexOffset, BuiltInTriangleIntersectionAttributes hitAttribs )
{
    const uint i0 = idxBuf[ triBase + 0 ];
    const uint i1 = idxBuf[ triBase + 1 ];
    const uint i2 = idxBuf[ triBase + 2 ];

    rtTriangle_t tri;
    tri.v0 = LoadRtVertex( vtxBuf, vertexOffset + i0 );
    tri.v1 = LoadRtVertex( vtxBuf, vertexOffset + i1 );
    tri.v2 = LoadRtVertex( vtxBuf, vertexOffset + i2 );

    tri.b1 = hitAttribs.barycentrics.x;
    tri.b2 = hitAttribs.barycentrics.y;
    tri.b0 = 1.0f - tri.b1 - tri.b2;

    return tri;
}


// Reconstructs surface sample at the intersection location.
geometryAttributes_t BuildSampleAttributes( const rtTriangle_t tri )
{
    const float3 localNormal = normalize( BaryLerp3( tri.v0.normal,    tri.v1.normal,    tri.v2.normal,    tri.b0, tri.b1, tri.b2 ) );
    const float3 localTangent = normalize( BaryLerp3( tri.v0.tangent,   tri.v1.tangent,   tri.v2.tangent,   tri.b0, tri.b1, tri.b2 ) );
    const float3 localBitangent = normalize( BaryLerp3( tri.v0.bitangent, tri.v1.bitangent, tri.v2.bitangent, tri.b0, tri.b1, tri.b2 ) );

    geometryAttributes_t surfaceSample;

    surfaceSample.N = normalize( mul( localNormal, (float3x3)WorldToObject3x4() ) );
    surfaceSample.T = normalize( mul( localTangent, (float3x3)ObjectToWorld3x4() ) );
    surfaceSample.B = normalize( mul( localBitangent, (float3x3)ObjectToWorld3x4() ) );

    surfaceSample.uv0 = BaryLerp2( tri.v0.uv.xy, tri.v1.uv.xy, tri.v2.uv.xy, tri.b0, tri.b1, tri.b2 );
    surfaceSample.uv1 = BaryLerp2( tri.v0.uv.zw, tri.v1.uv.zw, tri.v2.uv.zw, tri.b0, tri.b1, tri.b2 );

    surfaceSample.worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    return surfaceSample;
}

#endif // RTUTIL_HLSL_H

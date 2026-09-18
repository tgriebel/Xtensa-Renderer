#ifndef RTUTIL_HLSL_H
#define RTUTIL_HLSL_H

// Interpolate a float3 attribute across a triangle using barycentric weights.
float3 BaryLerp3( float3 a, float3 b, float3 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}

float2 BaryLerp2( float2 a, float2 b, float2 c, float b0, float b1, float b2 )
{
    return ( a * b0 + b * b1 + c * b2 );
}


// Reconstructs surface sample at the intersection location
sampleAttributes_t BuildSampleAttributes( const rtVertex_t v0, const rtVertex_t v1, const rtVertex_t v2, const float b0, const float b1, const float b2 )
{
    const float3 localNormal = normalize( BaryLerp3( v0.normal,    v1.normal,    v2.normal,    b0, b1, b2 ) );
    const float3 localTangent = normalize( BaryLerp3( v0.tangent,   v1.tangent,   v2.tangent,   b0, b1, b2 ) );
    const float3 localBitangent = normalize( BaryLerp3( v0.bitangent, v1.bitangent, v2.bitangent, b0, b1, b2 ) );

    sampleAttributes_t surfaceSample;

    surfaceSample.N = normalize( mul( localNormal, (float3x3)WorldToObject3x4() ) );
    surfaceSample.T = normalize( mul( localTangent, (float3x3)ObjectToWorld3x4() ) );
    surfaceSample.B = normalize( mul( localBitangent, (float3x3)ObjectToWorld3x4() ) );

    surfaceSample.uv0 = BaryLerp2( v0.uv.xy, v1.uv.xy, v2.uv.xy, b0, b1, b2 );
    surfaceSample.uv1 = BaryLerp2( v0.uv.zw, v1.uv.zw, v2.uv.zw, b0, b1, b2 );

    surfaceSample.worldPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    return surfaceSample;
}

#endif // RTUTIL_HLSL_H

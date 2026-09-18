#include "rtGlobals.h"

GLOBAL_BINDS( 0 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_VERTEX_BUFFER( 1, 2, vtxBuffer )
RT_INDEX_BUFFER( 1, 3, idxBuffer )
RT_SURFACE_INFO( 1, 4, surfaceInfos )
LIGHT_LAYOUT( 1, 5 )
RT_PUSH_CONSTANTS

#include "lighting.h"


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
    const float3 localNormal    = normalize( BaryLerp3( v0.normal,    v1.normal,    v2.normal,    b0, b1, b2 ) );
    const float3 localTangent   = normalize( BaryLerp3( v0.tangent,   v1.tangent,   v2.tangent,   b0, b1, b2 ) );
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


[shader( "closesthit" )]
void closesthit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes hitAttribs )
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
    const float b1 = hitAttribs.barycentrics.x;
    const float b2 = hitAttribs.barycentrics.y;
    const float b0 = 1.0f - b1 - b2;

    const sampleAttributes_t surfaceSample = BuildSampleAttributes( v0, v1, v2, b0, b1, b2 );

    // Build ray-cone for texture LOD calculation
    {
        // TODO: derive from surface curvature from roughness
        const float surfaceSpreadAngle = 0.0f;
        g_rtTexLod.cone = Propagate( payload.cone, surfaceSpreadAngle, RayTCurrent() );

        const float3 worldPos0 = mul( float4( v0.position.xyz, 1.0f ), ObjectToWorld4x3() );
        const float3 worldPos1 = mul( float4( v1.position.xyz, 1.0f ), ObjectToWorld4x3() );
        const float3 worldPos2 = mul( float4( v2.position.xyz, 1.0f ), ObjectToWorld4x3() );
        g_rtTexLod.triangleWorldArea = 0.5f * length( cross( worldPos1 - worldPos0, worldPos2 - worldPos0 ) );

        const float2 uvEdge1 = ( v1.uv.xy - v0.uv.xy );
        const float2 uvEdge2 = ( v2.uv.xy - v0.uv.xy );
        g_rtTexLod.triangleUvArea = 0.5f * abs( uvEdge1.x * uvEdge2.y - uvEdge2.x * uvEdge1.y );

        g_rtTexLod.rayDir = WorldRayDirection();
        g_rtTexLod.surfaceNormal = surfaceSample.N;
    }

    // Gather surface data
    const gpuView_t view = views[ rtConstants.viewId ];
    const gpuMaterial_t material = materials[ surf.materialId ];
    const surfaceInput_t surfaceInput = CalculateSurfaceInput( globals, view, material, surfaceSample );

    // Simplified lighting loop without IBL and shadows
    // Eventually this can be combined with lit.ps.hlsl
    float3 Lo = float3( 0.0f, 0.0f, 0.0f );
    for ( int i = 0; i < (int)view.numLights; ++i )
    {
        const gpuLight_t light = lights[ i ];
        const lightingInput_t lightingInput = CalculateLightingInput( surfaceInput, light );

        brdfSample_t brdf;
        if ( surfaceInput.useAniso ) {
            brdf = EvaluateAnisoBrdf( surfaceInput, lightingInput );
        } else {
            brdf = EvaluateBaseBrdf( surfaceInput, lightingInput );
        }

        if ( surfaceInput.useClearCoat ) {
            ApplyClearcoatBrdf( surfaceInput, lightingInput, brdf );
        }
        if ( surfaceInput.useSheen ) {
            ApplySheenBrdf( surfaceInput, lightingInput, brdf );
        }

        Lo += ( brdf.Fd + brdf.Fr ) * lightingInput.Li * lightingInput.NoL;
    }

    const float3 diffuseAmbient = EvaluateDiffuseAmbient( globalCubemaps[ surf.diffuseIblCubeId ], surfaceInput );
    const float3 specularAmbient = EvaluateSpecularAmbient( globalCubemaps[ surf.envCubeId ], globalTextures[ globals.brdfLutId ], surfaceInput );

    payload.color = float4( Lo + diffuseAmbient + specularAmbient + surfaceInput.emissive, 1.0f );
}

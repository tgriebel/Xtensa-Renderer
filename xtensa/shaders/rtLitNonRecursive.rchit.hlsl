#include "rtGlobals.h"

GLOBAL_BINDS( 0 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
VERTEX_BUFFER_LAYOUT( 1, 2, vtxBuffer )
INDEX_BUFFER_LAYOUT( 1, 3, idxBuffer )
RT_SURFACE_INFO( 1, 4, surfaceInfos )
LIGHT_LAYOUT( 1, 5 )
RT_PUSH_CONSTANTS

#include "lighting.h"
#include "rtUtil.h"


[shader( "closesthit" )]
void closesthit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes hitAttribs )
{
    // InstanceID() returns instanceCustomIndex, which we set to the BLAS index.
    // This maps directly to the surface info entry for this geometry.
    const uint surfIdx = InstanceID();
    const gpuRtSurface_t surf = surfaceInfos[ surfIdx ];

    // Locate the hit triangle
    const uint triBase = surf.firstIndex + PrimitiveIndex() * 3;
    const triangle_t tri = LoadTriangle( vtxBuffer, idxBuffer, triBase, surf.vertexOffset, hitAttribs.barycentrics );
    const gpuVertex_t v0 = tri.v0;
    const gpuVertex_t v1 = tri.v1;
    const gpuVertex_t v2 = tri.v2;

    const geometryAttributes_t surfaceSample = BuildSampleAttributes( tri );

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

    // Simplified lighting loop without shadows
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

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
#include "rtUtil.h"

// FIXME: Claude generated!!

[shader( "closesthit" )]
void closesthit_main( inout hitPayload_t payload, in BuiltInTriangleIntersectionAttributes hitAttribs )
{
    const uint surfIdx = InstanceID();
    const gpuRtSurface_t surf = surfaceInfos[ surfIdx ];

    const uint triBase = surf.firstIndex + PrimitiveIndex() * 3;
    const uint i0 = idxBuffer[ triBase + 0 ];
    const uint i1 = idxBuffer[ triBase + 1 ];
    const uint i2 = idxBuffer[ triBase + 2 ];

    const rtVertex_t v0 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i0 );
    const rtVertex_t v1 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i1 );
    const rtVertex_t v2 = LoadRtVertex( vtxBuffer, surf.vertexOffset + i2 );

    const float b1 = hitAttribs.barycentrics.x;
    const float b2 = hitAttribs.barycentrics.y;
    const float b0 = 1.0f - b1 - b2;

    const sampleAttributes_t surfaceSample = BuildSampleAttributes( v0, v1, v2, b0, b1, b2 );

    const gpuView_t view = views[ rtConstants.viewId ];
    const gpuMaterial_t material = materials[ surf.materialId ];
    const surfaceInput_t surfaceInput = CalculateSurfaceInput( globals, view, material, surfaceSample );

    const float3 worldPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    const float3 I = WorldRayDirection();

    const bool entering = dot( I, surfaceInput.N ) < 0.0f;
    const float3 frontNormal = entering ? surfaceInput.N : -surfaceInput.N;

    const float ior = max( material.ior, 1.0f );
    const float eta = entering ? ( 1.0f / ior ) : ior;

    const float3 refractedDir = refract( I, frontNormal, eta );
    const bool totalInternalReflection = ( dot( refractedDir, refractedDir ) < 1e-6f );

    const float cosTheta = saturate( -dot( I, frontNormal ) );
    const float F0 = pow( ( ior - 1.0f ) / ( ior + 1.0f ), 2.0f );
    const float fresnel = F0 + ( 1.0f - F0 ) * pow( 1.0f - cosTheta, 5.0f );

    const float bias = 0.001f;

    float3 reflected;
    float3 transmitted;

    if ( payload.depth < MaxRtRecursionDepth )
    {
        const uint nextDepth = payload.depth + 1;

        {
            hitPayload_t reflPayload;
            reflPayload.color = float4( 0.0f, 0.0f, 0.0f, 1.0f );
            reflPayload.cone = payload.cone;
            reflPayload.depth = nextDepth;

            RayDesc reflRay;
            reflRay.Origin = worldPos + frontNormal * bias;
            reflRay.Direction = reflect( I, frontNormal );
            reflRay.TMin = 0.001f;
            reflRay.TMax = 10000.0f;

            TraceRay( tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, reflRay, reflPayload );
            reflected = reflPayload.color.rgb;
        }

        {
            hitPayload_t refrPayload;
            refrPayload.color = float4( 0.0f, 0.0f, 0.0f, 1.0f );
            refrPayload.cone = payload.cone;
            refrPayload.depth = nextDepth;

            const float3 transmitDir = totalInternalReflection ? reflect( I, frontNormal ) : refractedDir;

            RayDesc refrRay;
            refrRay.Origin = worldPos - frontNormal * bias;
            refrRay.Direction = transmitDir;
            refrRay.TMin = 0.001f;
            refrRay.TMax = 10000.0f;

            TraceRay( tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, refrRay, refrPayload );
            transmitted = refrPayload.color.rgb * material.Tf;
        }
    }
    else
    {
        reflected = EvaluateSpecularAmbient( globalCubemaps[ surf.envCubeId ], globalTextures[ globals.brdfLutId ], surfaceInput );
        transmitted = EvaluateDiffuseAmbient( globalCubemaps[ surf.diffuseIblCubeId ], surfaceInput ) * material.Tf;
    }

    const float blendWeight = totalInternalReflection ? 1.0f : fresnel;

    payload.color = float4( lerp( transmitted, reflected, blendWeight ) + surfaceInput.emissive, material.opacity );
}

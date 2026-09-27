#include "rtGlobals.h"

GLOBAL_BINDS( 0 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_VERTEX_BUFFER( 1, 2, vtxBuffer )
RT_INDEX_BUFFER( 1, 3, idxBuffer )
RT_SURFACE_INFO( 1, 4, surfaceInfos )
LIGHT_LAYOUT( 1, 5 )
RT_PUSH_CONSTANTS

#include "util.h"
#include "lighting.h"
#include "rtUtil.h"
#include "crtCore.h"

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

    const gpuMaterial_t material = materials[ surf.materialId ];
    const Texture2D screenTex = globalTextures[ material.textureId[ 0 ] ];

    const float3 crtColor = CrtShade( screenTex, bilinearSamplerClampEdge, surfaceSample.uv0 );

    payload.color = float4( crtColor, material.opacity );
}

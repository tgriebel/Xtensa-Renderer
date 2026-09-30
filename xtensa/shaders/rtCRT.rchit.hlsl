#include "rtGlobals.h"

GLOBAL_BINDS( 0 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
VERTEX_BUFFER_LAYOUT( 1, 2, vtxBuffer )
INDEX_BUFFER_LAYOUT( 1, 3, idxBuffer )
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
    const triangle_t tri = LoadTriangle( vtxBuffer, idxBuffer, triBase, surf.vertexOffset, hitAttribs.barycentrics );
    const geometryAttributes_t surfaceSample = BuildSampleAttributes( tri );

    const gpuMaterial_t material = materials[ surf.materialId ];
    const Texture2D screenTex = globalTextures[ material.textureId[ 0 ] ];

    const float3 crtColor = CrtShade( screenTex, bilinearSamplerClampEdge, surfaceSample.uv0 );

    payload.color = float4( crtColor, material.opacity );
}

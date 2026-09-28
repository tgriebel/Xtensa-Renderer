#include "globals.h"
#include "util.h"
#include "visBuffer.h"

// Creates a GBuffer from a visibility buffer

struct visToGBufferParms_t
{
    uint2   dimensions;
    uint    viewId;
    uint    pad;
};

GLOBALS_LAYOUT( 0, 0 )
VIEW_LAYOUT( 0, 1 )
WRITE_IMAGE_LAYOUT( 0, 2, Texture2D<uint2>, visBufferImage )
WRITE_IMAGE_LAYOUT( 0, 3, RWTexture2D<float4>, outNormalVelocity )
WRITE_IMAGE_LAYOUT( 0, 4, RWTexture2D<float4>, outWorldPosition )
SAMPLER_2D_LAYOUT( 0, 5 )
SAMPLER( 0, 6, bilinearSamplerWrap )
SAMPLER( 0, 7, bilinearSamplerClampEdge )
MODEL_LAYOUT( 0, 8 )

BIND_INLINE visToGBufferParms_t visToGBufferParms;

[numthreads( 8, 8, 1 )]
void CSMain( uint3 dtid : SV_DispatchThreadID )
{
    const uint x = dtid.x;
    const uint y = dtid.y;
    const uint z = dtid.z;

    const uint width = uint( visToGBufferParms.dimensions.x );
    const uint height = uint( visToGBufferParms.dimensions.y );

    if ( x >= width || y >= height )
    {
        return;
    }

    const visibilitySample_t packedIds = DecodeVisibility( visBufferImage.Load( int3( x, y, 0 ) ) );
    
    gpuSurface_t surf = surfaces[ packedIds.objectId ];

    //// Locate the hit triangle
    //const uint triBase = surf.firstIndex + PrimitiveIndex() * 3;
    
    //const uint i0 = idxBuf[ triBase + 0 ];
    //const uint i1 = idxBuf[ triBase + 1 ];
    //const uint i2 = idxBuf[ triBase + 2 ];

    //rtTriangle_t tri;
    //tri.v0 = LoadRtVertex( vtxBuf, vertexOffset + i0 );
    //tri.v1 = LoadRtVertex( vtxBuf, vertexOffset + i1 );
    //tri.v2 = LoadRtVertex( vtxBuf, vertexOffset + i2 );

    //tri.b1 = hitAttribs.barycentrics.x;
    //tri.b2 = hitAttribs.barycentrics.y;
    //tri.b0 = 1.0f - tri.b1 - tri.b2;
}

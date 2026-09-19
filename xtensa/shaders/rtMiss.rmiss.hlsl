#include "rtGlobals.h"
#include "util.h"

GLOBAL_BINDS( 0 )
RT_ACCELERATION_STRUCTURE( 1, 0, tlas )
RT_OUTPUT( 1, 1, rtOutput )
RT_PUSH_CONSTANTS

[shader( "miss" )]
void miss_main( inout hitPayload_t payload )
{
    const gpuView_t view = views[ rtConstants.viewId ];
    const float3 skyColor = globalCubemaps[ view.skyboxCubeId ].SampleLevel( bilinearSamplerWrap, CubeVector( WorldRayDirection() ), 0.0f ).rgb;
    payload.color = float4( SrgbToLinear( skyColor ), 1.0f );
}

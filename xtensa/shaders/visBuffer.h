#ifndef VISBUFFER_HLSL_H
#define VISBUFFER_HLSL_H

#include "surfaceTypes.h"

// See John Hable's blog post for all the core implementation details on visibility buffers
// https://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/

struct visibilitySample_t
{
	uint objectId;
	uint triangleId;
};


uint2 EncodeVisibility( const uint objectId, const uint triangleId )
{
	return uint2( objectId, triangleId );
}


uint2 EncodeVisibility( const visibilitySample_t vis )
{
	return EncodeVisibility( vis.objectId, vis.triangleId );
}


visibilitySample_t DecodeVisibility( const uint2 packed )
{
	visibilitySample_t vis;
	vis.objectId = packed.x;
	vis.triangleId = packed.y;
	return vis;
}


geometryAttributes_t ResolveVisibility( const visibilitySample_t vis, const float2 pixelNdc, const gpuView_t view )
{
	geometryAttributes_t result = (geometryAttributes_t)0;
	// TODO: implement
	return result;
}

#endif // VISBUFFER_HLSL_H
